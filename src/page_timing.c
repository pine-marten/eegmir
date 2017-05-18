//
//	'timing' page handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	The purpose of this page is to give an idea of how well the
//	timing of the whole system is holding up -- not just of the
//	EEG device, but also of the serial input and the
//	responsiveness of the OS and eegmir code to keep up with the
//	serial input.  It shows the last 9 seconds of data by default,
//	indicating the estimated actual time of data sampling and the
//	time the data actually arrived.  It calculates the maximum and
//	average jitter and the average sampling rate (which might turn
//	out to be different to the theoretical sampling rate).
//	

#ifdef HEADER

typedef struct PageTiming PageTiming;
struct PageTiming {
   Page pg;
   int n_tim;		// Number of time-samples taken
   int *tim;		// List of time-samples in ms
   char *flag;		// List of error flags corresponding to tim[]
   double rate;		// Calculated sampling rate
   double j_max;	// Maximum jitter, ms
   double j_ave;	// Average jitter, ms
   double off, inc;	// Straight line representing last call to find_min_off()
   int err;
};

#else

#ifndef NO_ALL_H
#include "all.h"
#endif

//
//	Setup page-handler
//

static void event(Event *ev);

Page *
p_timing_init(Parse *pp) {
   PageTiming *pg= ALLOC(PageTiming);

   pg->pg.event= event;

   //   // Top-level stuff
   //   while (1) {
   //      if (parse(pp, "fps %d;", &pg->fps)) continue;
   //      if (parse(pp, "gain %f;", &pg->gain)) continue;
   //      if (parse(pp, "sgain %f;", &pg->sgain)) continue;
   //      if (parse(pp, "spots;")) { pg->spots= 1; continue; }
   //      if (parse(pp, "title %Q;", &pg->title)) continue;
   //      break;
   //   }

   if (!parseEOF(pp)) {
      line_error(pp, pp->pos, "Unexpected/invalid stuff at end of section");
      return 0;
   }

   return (Page*)pg;
}


//
//	Look for the least 'sticking out' timing value assuming the
//	given sampling rate, and return its index.
//

static int 
find_min_off(PageTiming *pg, double rate) {
   double inc= 1000.0 / rate;
   double off= pg->tim[0];
   double min= 0.0;
   int min_ii= 0;
   int a;
   
   for (a= 0; a<pg->n_tim; a++) {
      if (pg->tim[a] - off < min) {
	 min= pg->tim[a] - off;	
	 min_ii= a;
      }
      off += inc;
   }

   pg->off= min;
   pg->inc= inc;
   pg->rate= rate;

   return min_ii;
}
   

//
//	Analyse the last 'per' seconds of timing data
//

static void 
analyse_data(PageTiming *pg, double per) {
   int n_tim= per * dev->rate;
   int *tim;
   char *flag;
   int n_smp= dev->n_smp * 9 / 10;
   int wr, rd, a;
   int midp;	// Midpoint (n_tim/2)
   double r0, r1, r2;
   int cnt;
   int adj;

   pg->err= 0;

   // Setup a buffer for the time-stamp values
   if (n_tim > n_smp) n_tim= n_smp;
   if (pg->tim) free(pg->tim);
   pg->tim= tim= ALLOC_ARR(n_tim, int);
   pg->flag= flag= ALLOC_ARR(n_tim, char);
   pg->n_tim= n_tim;

   // Take a copy of the timestamps (so they don't change whilst we
   // are analysing)
   wr= dev->wr;
   rd= wr - n_tim; SAMPLE_WRAP(rd);
   for (a= 0; a<n_tim; a++) {
      Sample *ss= SAMPLE(rd);
      tim[a]= ss->stamp;
      flag[a]= (ss->err != 0);
      SAMPLE_INC(rd);
   }
   adj= -tim[0];
   for (a= 0; a<n_tim; a++) tim[a] += adj;

   // Look for the accurate sampling rate
   midp= n_tim/2;
   r0= dev->rate * 0.707;
   r2= dev->rate * 1.414;
   cnt= 20;
   while (cnt > 0 && find_min_off(pg, r0) < midp) {
      r2= r0; r0 *= 0.5; cnt--;
   }
   while (cnt > 0 && find_min_off(pg, r2) > midp) {
      r0= r2; r2 *= 2.0; cnt--;
   }

   if (cnt <= 0 ||
       find_min_off(pg, r0) < midp ||
       find_min_off(pg, r2) > midp) {
      find_min_off(pg, dev->rate);
      pg->j_max= 0;
      pg->j_ave= 0;
      pg->err= 1;
      return;
   }
   
   // Binary search for the accurate value; 20 => 6sf accuracy
   for (cnt= 20; cnt>0; cnt--) {
      r1= (r0 + r2) / 2;
      if (find_min_off(pg, r1) < midp) 
	 r2= r1;
      else 
	 r0= r1;
   }

   // pg->off/inc/rate will have been set by the last call to find_min_off()

   // Calculate the jitter values
   {
      double sum= 0.0;
      double off= pg->off;
      double inc= pg->inc;
      pg->j_max= 0;
      for (a= 0; a<n_tim; a++) {
	 double jitter= tim[a] - off;
	 if (jitter > pg->j_max)
	    pg->j_max= jitter;
	 sum += jitter;
	 off += inc;
      }
      pg->j_ave= sum / n_tim;
   }
}
	 

//
//	Event handler
//

static void 
event(Event *ev) {
   PageTiming *pg= (void*)page;
   int a;
   
   switch (ev->typ) {
    case 'RESZ':	// Resize (sx,sy)
       break;
    case 'SHOW':	// Show
       break;
    case 'HIDE':	// Hide
       if (pg->tim) { free(pg->tim); pg->tim= 0; }
       break;
    case 'SET':		// Settings change
       break;
    case 'TICK':	// New frame
       break;
    case 'DRAW':	// Redraw
       // Do a full analysis of the last 9 seconds and display it
       {
	  char txt[256];
	  int len;
	  short *font;
	  int yy, sy;
	  int n_col;
	  int wid;	// Column width
	  int gap;	// Gap between columns
	  double widms;	// Column width in ms
	  char *p;
	  double off, inc;

	  analyse_data(pg, 9);
	  clear_rect(0, 0, disp_sx, disp_sy, colour[0]);
	  if (pg->err) {
	     p= txt + sprintf(txt, "\x82 UNABLE TO CALCULATE RATE OR JITTER ");
	     p++;	// After the NUL
	  } else {
	     p= txt + sprintf(txt, "\x84Sampling rate: %.5gHz (%gHz),  "
			      "Jitter: max %.1fms, average %.1fms,  "
			      "Column width: ",
			      pg->rate, dev->rate, pg->j_max, pg->j_ave);
	  }
	  len= strlen(txt) - 1 + 7;
	  for (a= 2; a>=0; a--) {
	     font= (a==2) ? font10x20 : (a==1) ? font8x16 : font6x12;
	     if (len * font[0] <= disp_sx) break;
	  }
	  yy= font[1]; sy= disp_sy - yy;

	  n_col= ((pg->n_tim - 1) / sy) + 1;
	  wid= disp_sx / n_col;
	  gap= wid/10;
	  if (gap < 1) gap= 1;
	  wid= (disp_sx + gap) / n_col - gap;
	  widms= wid * 1000.0 / pg->rate;
	  sprintf(p, "%.1fms", widms);
	  drawtext(font, 0, 0, txt);

	  off= pg->off;
	  inc= pg->inc;
	  for (a= 0; a<pg->n_tim; a++) {
	     int ox= (a/sy) * (wid + gap);
	     int oy= (a%sy) + yy;
	     int bg= pg->flag[a] ? colour[22] : colour[20];
	     double dmy;
	     int x0, x1, xp0, xp1;
	     xp0= (int)floor(100 * wid * modf(off / widms, &dmy));
	     xp1= (int)floor(100 * wid * modf(pg->tim[a] / widms, &dmy));
	     if (xp0 < 0) xp0= 0;
	     x0= xp0/100;
	     x1= xp1/100;
	     xp0 %= 100; 
	     xp1 %= 100;
	     if (x0 >= wid) x0= 0;	// Sanity check
	     if (x1 >= wid) x1= 0;	// Sanity check
	     hline(ox, oy, wid, bg);
	     if (x0 == x1) {
		plot(ox+x0, oy, colour_mix(xp1-xp0, bg, colour[21]));
	     } else {
		plot(ox+x0, oy, colour_mix(xp0, colour[21], bg));
		if (++x0 >= wid) x0= 0;
		while (x0 != x1) {
		   plot(ox+x0, oy, colour[21]);
		   if (++x0 >= wid) x0= 0;
		}
		plot(ox+x1, oy, colour_mix(xp1, bg, colour[21]));
	     }
	     off += inc;
	  }

	  update_all();
       }
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

#endif

// END //


