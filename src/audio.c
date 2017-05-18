//
//	Audio device handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

#ifdef HEADER

typedef void AudioHandler(void *vp, short *buf, int cnt);

#define SINTAB_AMP 0x7FFFF	// Amplitude of wave in sine-table
#define SINTAB_HRM 4096		// Headroom above SINTAB_AMP to fit within 32-bit int
#define SINTAB_SIZE 16384	// Number of elements in sine-table (power of 2)
#define SINTAB_FBC 18		// Fraction bit count for offsets; i.e. SIZE<<FBC == 2**32
#define SINTAB_MASK (SINTAB_SIZE-1)	// Mask to convert to sintab[] index

#else

#ifndef NO_ALL_H
#include "all.h"
#endif

//
//	Globals
//

int audio;			// Audio running?
int audio_rate;			// Output sampling rate
int audio_bufsz;		// Buffer/block size in samples
int sintab[SINTAB_SIZE];	// Sine table

//
//	Time handing.  'audio_clock.clock' provides the current time
//	in ms/65536 according to the audio device output clock.  This
//	wraps every ~65 seconds.  This is gently adjusted over periods
//	of a second or more to correct for any relative drift between
//	this clock and the computer's millisecond clock.  Every time
//	the audio callback is made a sample of the computer clock is
//	taken.  This may be late due to scheduling delays, but taking
//	all the minimum values over a period allows a synchronisation
//	to be made.  The reason why a separate audio clock is required
//	is to keep audio timing accurate even if the audio callback
//	routine is called late.  'audio_clock.clockinc' provides the
//	fractional time increment per block of audio.
//

Clock audio_clock;

//
//	Static globals
//

static SDL_AudioSpec aud;

typedef struct AudioCB {
   AudioHandler *fn;		// Function to call, or 0 for end of list
   void *vp;
} AudioCB;

static AudioCB *cb_arr[2];	// Two arrays of callbacks, or 0 if unallocated
static int curr;		// Which of the above lists is actually the current one to use
static int inuse;		// Set during the audio callback

static int test_osc_off= 0;

//
//	Add/Del calls -- these are designed to be called only from one
//	thread at a time, i.e. probably exclusively from the GUI
//	thread.
//

//
//	Add an audio handler to the active list.
//

void 
audio_add(AudioHandler *fn, void *vp) {
   int ii;
   int len= 0;
   AudioCB *p;

   // Wait for any current audio callback to complete
   while (inuse) SDL_Delay(1);

   // After this point, the audio callback will only ever use 'curr'.
   // This means that we are free to manipulate the other array.

   ii= !curr;

   if ((p= cb_arr[curr])) while (p->fn) { p++; len++; }
   if (cb_arr[ii]) free(cb_arr[ii]);
   cb_arr[ii]= ALLOC_ARR(len+2, AudioCB);
   
   if (len) memcpy(cb_arr[ii], cb_arr[curr], len*sizeof(AudioCB));
   cb_arr[ii][len].fn= fn;
   cb_arr[ii][len].vp= vp;
   cb_arr[ii][len+1].fn= 0;
   cb_arr[ii][len+1].vp= 0;

   // Switch over.  Any currently executing audio callback will use
   // the old 'curr', and any future call to this routine will wait
   // until the callback has finished before messing with the old
   // 'curr' array.
   curr= ii;
}

//
//	Remove an audio handler from the list.  Returns 1 success, 0
//	not found.
//

int 
audio_del(AudioHandler *fn, void *vp) {
   AudioCB *p;
   int len= 0;
   int off= -1;
   int ii;

   // Search to see if it is in the list
   if ((p= cb_arr[curr]))
      while (p->fn) {
	 if (p->fn == fn &&
	     p->vp == vp) off= len;
	 len++; p++;
      }

   if (off < 0) return 0;

   // Wait for any current audio callback to complete
   while (inuse) SDL_Delay(1);

   // After this point, the audio callback will only ever use 'curr'.
   // This means that we are free to manipulate the other array.

   ii= !curr;
   if (cb_arr[ii]) free(cb_arr[ii]);
   cb_arr[ii]= ALLOC_ARR(len, AudioCB);

   if (off) memcpy(cb_arr[ii], cb_arr[curr], off*sizeof(AudioCB));
   memcpy(cb_arr[ii]+off, cb_arr[curr]+off+1, (len-off)*sizeof(AudioCB));
   
   // Switch over.  Any currently executing audio callback will use
   // the old 'curr', and any future call to this routine will wait
   // until the callback has finished before messing with the old
   // 'curr' array.
   curr= ii;
   
   return 1;
}


//
//      Setup the device
//

static void audio_callback(void*, Uint8*, int);

int
handle_audio_setup(Parse *pp) {
   SDL_AudioSpec want;
   int a;

   if (dev) return line_error(pp, pp->pos, 
			      "[audio] section should appear before the [*-dev] section");

   // Defaults
   audio_rate= 44100;
   audio_bufsz= 512;

   // Pick up any options
   while (1) {
      if (parse(pp, "rate %d;", &audio_rate)) continue;
      if (parse(pp, "buf %d;", &audio_bufsz)) {
	 if (!audio_bufsz ||
	     (audio_bufsz & (audio_bufsz-1)))
	    return line_error(pp, pp->rew, 
			      "Buffer size should be a power of 2 (512,1024,2048,etc)");
	 continue;
      }
      break;
   }
   
   if (!parseEOF(pp))
      return line_error(pp, pp->pos, "Unrecognised trailing [audio] section entries");

   // Setup the audio device
   memset(&want, 0, sizeof(want));
   want.freq= audio_rate;
   want.format= AUDIO_S16SYS;
   want.channels= 2;
   want.samples= audio_bufsz;
   want.callback= audio_callback;

   if (SDL_OpenAudio(&want, &aud) < 0) {
      applog("    \x98""failed to open audio device; continuing without audio");
      audio_rate= 0;
      audio= 0;
      return 0;
   }

   audio= 1;
   audio_rate= aud.freq;
   audio_bufsz= aud.samples;
   
   applog("    audio device initialised at %dHz with %.1fms buffer",
	  audio_rate, audio_bufsz * 1000.0 / audio_rate);

   // Setup timing
   clock_setup(&audio_clock, 
	       audio_rate * 1.0 / audio_bufsz, 
	       time_now_ms());

   // Setup sintab[] array
   for (a= 0; a<SINTAB_SIZE; a++)
      sintab[a]= floor(0.5 + SINTAB_AMP * sin(a * 2 * M_PI / SINTAB_SIZE));

   SDL_PauseAudio(0);
   return 0;
}

//
//	Audio callback.  Timing is treated as critical in this routine
//	and routines called from it.
//

static void 
audio_callback(void *vp, Uint8 *cdat, int clen) {
   AudioCB *p;
   int now;
   short *dat= (short*)cdat;
   int len= clen / (2 * sizeof(short));

   // Calculate the current time only once, and share it with all
   // routines called to save any overhead connected with this.
   now= time_now_ms();

   // Sanity check
   if (len != audio_bufsz) 
      applog("Audio callback called for wrong number of samples: %d", len);

   // Update/adjust audio timing clock
   clock_inc(&audio_clock, now);

   // Do serial input if audio-callback-based serial is enabled
   serial_audio_callback(now);

   // Test oscillator -- for debugging
   if (0) {
      short *p= dat;
      int cnt= len;
      while (cnt-- > 0) {
	 short val= sintab[test_osc_off] >> 8;
	 *p++= val;
	 *p++= val;
	 test_osc_off += 100;
	 test_osc_off &= SINTAB_MASK;
      }
   }

   // Flag that the callback is active to stop the array referred to
   // by 'curr' from disappearing, and run through all the current
   // audio handlers.
   inuse= 1;
   p= cb_arr[curr];
   if (p) for (; p->fn; p++)
      p->fn(p->vp, dat, len);
   inuse= 0;
}   

#endif

// END //

