//
//	'audio' page handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

#ifdef HEADER

//typedef struct AudioFB AudioFB;
typedef struct PageAudio PageAudio;

//struct AudioFB {
//   char *nam;		// StrDup'd display name
//
//   void (*init)(AudioFB*,PageAudio*);			// Init or reinit after being stopped
//   void (*hand)(AudioFB*,PageAudio*,short*,int);	// Generate a bunch of samples
//
//   int chan;		// First EEG channel to generate output for (0..)
//   int n_chan;		// Number of EEG channels to generate output for
//   int delay;		// Current delay to use (ms/65536)
//   int rd;		// Read-offset in sample buffer
//   int vol;		// Overall volume level: 0 to SINTAB_HRM
//};
//
//typedef struct AFB_FM AFB_FM;
//struct AFB_FM {		// FM feedback
//   AudioFB afb;
//   Uint32 osc0, osc1;	// Oscillators (<<SINTAB_FBC)
//   int incmin, incwid;	// Carrier/width expressed as min+wid oscillator increments
//}

struct PageAudio {
   Page pg;

   //   Settings *set;
   //
   //   int typ;		// Feedback type (0..): index into arrays
   //   AudioFB *typarr[10];	// List of loaded feedback types, or 0 if slot is free

   // Just for testing ...
   int fmrd;                  // Read-offset in sample buffer
   int fmdelay;               // Delay (ms/65536)
   Uint32 fmosc0, fmosc1;     // Oscillators (<<SINTAB_FBC)
   int fmincmin, fmincwid;    // Carrier/width expressed as min+wid oscillator increments
   int fmamp;                 // Amplitude

};

#else

#ifndef NO_ALL_H
#include "all.h"
#endif

//
//	Setup page-handler
//

static void event(Event *ev);
static AudioHandler fm_handler;

Page *
p_audio_init(Parse *pp) {
   PageAudio *pg= ALLOC(PageAudio);
   double v0, v1, v2;

   pg->pg.event= event;

   // Top-level stuff
   while (1) {
      

      if (parse(pp, "test-fmsig %dms %f+%f/%f;", 
		&pg->fmdelay, &v0, &v1, &v2)) {
	 pg->fmdelay *= 65536;
	 pg->fmrd= (dev->wr - 2) & dev->mask;
	 pg->fmincmin= (int)((65536*65536.0) * (v0-v1) / audio_rate);
	 pg->fmincwid= (int)((65536*65536.0) * (2*v1) / audio_rate);
	 pg->fmamp= (int)(SINTAB_HRM * v2 / 100.0);
	 audio_add(fm_handler, pg);
	 continue;
      }
      break;
   }
   
   if (!parseEOF(pp)) {
      line_error(pp, pp->pos, "Unexpected/invalid stuff at end of section");
      return 0;
   }

   return (Page*)pg;
}

//
//	Event handler
//

static void 
event(Event *ev) {
   //PageAudio *pg= (void*)page;
   
   switch (ev->typ) {
    case 'RESZ':	// Resize (sx,sy)
       break;
    case 'SHOW':	// Show
       break;
    case 'HIDE':	// Hide
       break;
    case 'SET':		// Settings change
       break;
    case 'TICK':	// New frame
       break;
    case 'DRAW':	// Redraw
       clear_rect(0, 0, disp_sx, disp_sy, colour[0]);
       drawtext(font10x20, 0, 0, "\x82 JUST TESTING FM AUDIO FEEDBACK RIGHT NOW ");
       update_all();
       break;
    case 'PAUS':
       break;
    case 'QUIT':	// Quit event
       exit(0);
       break;
    case 'KU':		// Key up (sym,mod,uc)
       break;
    case 'KD':		// Key down (sym,mod,uc)
    case 'KR':		// Key repeat (sym,mod,uc)
       break;
   }
}

//
//	FM audio feedback handling.  This is just for testing.  At the
//	moment it is hardcoded to use just the first two channels, and
//	feed them to left and right ears.
//
//	It does accurate time-matching to the data sample stream, and
//	even does linear interpolation for the frequency changes.
//

static void
fm_handler(void *vp, short *buf, int cnt) {
   PageAudio *pg= vp;
   int now= audio_clock.clock - pg->fmdelay;
   int nowinc= audio_clock.clockinc / cnt;
   int smpsmp;		// Audio samples per data sample
   int smpcnt;
   Sample *ss0, *ss1;
   int rd0, rd1, rd2;
   int inc0, inc1;
   int incinc0, incinc1;
   Uint32 osc0, osc1;
   int devmin= dev->min;
   double mul= 1.0 * pg->fmincwid / (dev->max - dev->min);

   osc0= pg->fmosc0;
   osc1= pg->fmosc1;

   rd0= pg->fmrd;
   rd1= (rd0+1) & dev->mask;
   rd2= (rd0+2) & dev->mask;

   //   {
   //      Sample *ss= SAMPLE((dev->wr-1) & dev->mask);
   //      warn("Block-start at time %d, last sample read at %d", now, ss->time);
   //   }

   ss0= SAMPLE(rd0);
   ss1= SAMPLE(rd1);

   smpsmp= (ss1->time-ss0->time) / nowinc;
   inc0= pg->fmincmin + (int)(mul * (ss0->val[0] - devmin));
   incinc0= (int)(mul * (ss1->val[0] - ss0->val[0]) / smpsmp);
   inc1= pg->fmincmin + (int)(mul * (ss0->val[1] - devmin));
   incinc1= (int)(mul * (ss1->val[1] - ss0->val[1]) / smpsmp);

   smpcnt= (now-ss0->time) / nowinc;
   inc0 += incinc0 * smpcnt;
   inc1 += incinc1 * smpcnt;

   for (; cnt-- > 0; now += nowinc) {
      while (rd2 != dev->wr && now-ss1->time > 0) {
	 rd0= rd1; rd1= rd2; SAMPLE_INC(rd2);
	 ss0= ss1; ss1= SAMPLE(rd1);
	 smpsmp= (ss1->time-ss0->time) / nowinc;
	 inc0= pg->fmincmin + (int)(mul * (ss0->val[0] - devmin));
	 incinc0= (int)(mul * (ss1->val[0] - ss0->val[0]) / smpsmp);
	 inc1= pg->fmincmin + (int)(mul * (ss0->val[1] - devmin));
	 incinc1= (int)(mul * (ss1->val[1] - ss0->val[1]) / smpsmp);
	 //	 warn("Read-pos %d, time %d, sample-time %d, count %d", rd0, now, ss0->time, cnt);
      }

      osc0 += inc0; osc1 += inc1;
      inc0 += incinc0; inc1 += incinc1;
      *buf++ += (sintab[osc0>>SINTAB_FBC] * pg->fmamp) >> 16;
      *buf++ += (sintab[osc1>>SINTAB_FBC] * pg->fmamp) >> 16;
   }

   //   warn("Block-end at time %d", now);

   pg->fmosc0= osc0;
   pg->fmosc1= osc1;
   pg->fmrd= rd0;
}

#endif

// END //


