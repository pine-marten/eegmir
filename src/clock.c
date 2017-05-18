//
//	Clock handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	This provides a ms/65536 clock which runs at the given rate
//	but which receives hints on the correct time now and again.
//	It uses these to gently adjust itself to keep in sync with the
//	correct time by drifting towards it over periods of about a
//	second.  This way the increasing time stays continuous
//	(without any sudden jumps) but also correctly compensates for
//	drift between the two clocks.
//
//	It is assumed that the provided 'correct' clock times in
//	milliseconds may be late but never early.  Late ones are
//	ignored.
//
//	Note that the clock value wraps every 65 seconds
//	approximately.
//

#ifdef HEADER

typedef struct Clock Clock;
struct Clock {
   int clock;         // Current ms/65536 time
   int clockinc;      // Increment to 'clock' for each time unit
   int targ;          // Target ms/65536 time, for calculating offset
   int targinc;       // Target increment per time unit
   int offset;        // Minimum offset encountered between correct time and target
   int cnt;           // Countdown to next targ/clock update
   int cntper;        // Period for countdown (roughly 1 sec)
};

#else

#ifndef NO_ALL_H
#include "all.h"
#endif

//
//	Setup a clock
//

void 
clock_setup(Clock *ck, double rate, int now) {
   ck->clock= now << 16;
   ck->clockinc= (int)(65536000/rate);
   ck->targ= ck->clock;
   ck->targinc= ck->clockinc;
   ck->offset= 0x7FFFFFFF;
   ck->cntper= (int)rate;
   if (ck->cntper < 6) ck->cntper= 6;
   ck->cnt= ck->cntper;
}

//
//	Increment a clock by one time unit as defined by the original
//	'rate' setup, and gently synchronize with the given measured
//	time (ms), which may be late but never early.  Returns the
//	current clock time (ck->clock).
//

int 
clock_inc(Clock *ck, int now) {
   int off;

   ck->clock += ck->clockinc;
   ck->targ += ck->targinc;

   off= (now<<16) - ck->targ;
   if (off < ck->offset) ck->offset= off;

   if (--ck->cnt <= 0) {
      // Adjust target to match ideal line according to last second or so
      ck->targ += ck->offset;

      // Cause ck->clock to drift to match ck->targ over the next
      // ck->cntper time units
      ck->clockinc= ck->targinc + (ck->targ-ck->clock) / ck->cntper;

      ck->cnt= ck->cntper;
      ck->offset= 0x7FFFFFFF;

      //applog("%p timing adjust: %.2fms", ck, (ck->targ-ck->clock) / 65536.0);
   }

   return ck->clock;
}

#endif

// END //

