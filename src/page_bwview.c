//
//	'bands' page handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	This display is based on shifting the frequencies by AM with a
//	complex oscillator, then filtering with a FidFilter low-pass
//	filter specified by the user.  This gives a complex output,
//	from which the amplitude and phase are extracted.  
//
//	The amplitude can be output immediately as a fast-reacting
//	trace.  It is also filtered by a user-supplied low-pass filter
//	to give a slower reacting trace (a bar).
//
//	The phase could also be output on both sides symmetrically to
//	give a sense of the connection between the sides.  @@@ One day.
//

#ifdef HEADER

typedef struct PB_Bar PB_Bar;
struct PB_Bar {
   PB_Bar *nxt;		// Next in chain
   int num;		// Bar number on the display, counting from 0
   char *label;		// Label for this bar, including colour change code
   int label_wid;	// Width of label in characters
   int col;		// Colour value
   double freq;		// Centre frequency
   FidFilter *lp;	// Lowpass band-limit filter
   FidFilter *sm;	// Smoothing low-pass

   // Runtime stuff
   FidRun *lp_run;
   FidFunc *lp_func;
   FidRun *sm_run;
   FidFunc *sm_func;

   double osc[4];	// Complex oscillator
   struct {
      void *lp0;	// Real part of band-limit filter
      void *lp1;	// Imaginary part of band-limit filter
      void *sm;		// Smoothing filter
      double mag;	// Output magnitude
      //double pha;	// Output phase
      double magsm;	// Output smoothed magnitude
      double maghist[5]; // Recent history of 'mag', used for visual effects
   } chan[1];
};

typedef struct PageBands PageBands;
struct PageBands {
   Page pg;
   int fps;		// Frames per second for display update
   int fms;		// Frame interval in ms (1000 / fps)
   int n_bar;		// Number of bars on this display
   int rd;		// Current read-offset into dev->smp[]
   PB_Bar *bar;		// Chain of bars
   int label_max;	// Maximum length of a label
   double gain;		// Gain for bar displays
   double sgain;	// Gain for signal display
   int spots;		// Display spots ?
   char *title;		// Title (strdup'd)

   // Display-related
   int c_bg, c_fg;	// Background/foreground colours
   int c_sig0, c_sig1;	// Signal colours
   int c_sig2;		// Sync error colour
   int c_tbg, c_tfg;	// Tower background/foreground
   int c_spot;		// Translucent spot colour
   short *font;		// Font to use
   int chan;		// Current base channel number (0,2,4, etc)
   int font_cx, font_cy; // Font cell size
   int tow_sx;		// Width of tower in pixels
   Settings *set;	// Settings for display
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
p_bands_init(Parse *pp) {
   PageBands *pg= ALLOC(PageBands);
   PB_Bar *bb;
   int ival, a;
   char *p0, *p1;
   char *err;

   pg->pg.event= event;
   pg->fps= 10;
   pg->gain= 1.0;
   pg->sgain= 1.0;
   pg->chan= 0;
   pg->c_bg= map_rgb(0x000000);
   pg->c_fg= map_rgb(0xFFFFFF);
   //pg->c_sig0= map_rgb(0x80A080);
   pg->c_sig0= map_rgb(0x405040);
   pg->c_sig1= map_rgb(0xC0FFC0);
   pg->c_sig2= map_rgb(0xFF0000);
   pg->c_tbg= map_rgb(0x332200);
   pg->c_tfg= map_rgb(0xFFFFFF);
   pg->c_spot= map_rgb(0xFFFFFF);

   // Top-level stuff
   while (1) {
      if (parse(pp, "fps %d;", &pg->fps)) continue;
      if (parse(pp, "gain %f;", &pg->gain)) continue;
      if (parse(pp, "sgain %f;", &pg->sgain)) continue;
      if (parse(pp, "spots;")) { pg->spots= 1; continue; }
      if (parse(pp, "title %Q;", &pg->title)) continue;
      break;
   }

   pg->fms= 1000 / pg->fps;
   if (pg->fms <= 0) pg->fms= 1;
       
   // Setup all the bars for the display
   while (parse(pp, "bar %q %x;", &p0, &p1, &ival)) {
      //@@@warn("dev->n_chan == %d", dev->n_chan);
      bb= Alloc(sizeof(PB_Bar) + sizeof(bb->chan[0]) * (dev->n_chan-1));
      bb->num= pg->n_bar++;
      bb->nxt= pg->bar; pg->bar= bb;
      bb->label= StrDupRange(p0-1, p1);
      bb->label[0]= '\x8a';	// Colour change (see draw_tower)
      bb->label_wid= p1-p0;
      bb->col= map_rgb(ival);
      if (p1-p0 > pg->label_max) pg->label_max= p1-p0;
      while (1) {
	 if (parse(pp, "filter %f, %r;", &bb->freq, &p0, &p1)) {
	    err= fid_parse(dev->rate, &p0, &bb->lp);
	    if (err) {
	       line_error(pp, pp->rew, "Bad filter-spec: %s", err);
	       free(err);
	       return 0;
	    }
	    if (p0 != p1) {
	       line_error(pp, p0, "Junk following filter-spec");
	       return 0;
	    }
	    continue;
	 }

	 if (parse(pp, "smooth %r;", &p0, &p1)) {
	    err= fid_parse(dev->rate, &p0, &bb->sm);
	    if (err) {
	       line_error(pp, pp->rew, "Bad filter-spec: %s", err);
	       free(err);
	       return 0;
	    }
	    if (p0 != p1) {
	       line_error(pp, p0, "Junk following filter-spec");
	       return 0;
	    }
	    continue;
	 }
	 break;
      }

      if (!bb->lp) {
	 line_error(pp, pp->pos, "Please specify a 'filter' for this 'bar' section");
	 return 0;
      }
   }

   if (!parseEOF(pp)) {
      line_error(pp, pp->pos, "Unexpected/invalid stuff at end of section");
      return 0;
   }

   // Setup run-time data
   pg->rd= dev->wr;
   for (bb= pg->bar; bb; bb= bb->nxt) {
      sincos_init(bb->osc, bb->freq / dev->rate);
      bb->lp_run= fid_run_new(bb->lp, &bb->lp_func);
      if (bb->sm) bb->sm_run= fid_run_new(bb->sm, &bb->sm_func);

      for (a= 0; a<dev->n_chan; a++) {
	 bb->chan[a].lp0= fid_run_newbuf(bb->lp_run);
	 bb->chan[a].lp1= fid_run_newbuf(bb->lp_run);
	 bb->chan[a].sm= bb->sm_run ? fid_run_newbuf(bb->sm_run) : 0;
      }
   }

   // Initialise settings
   pg->set= set_new("b 1 1e-10 1e10 EF 8 'Bar gain';"
		    "s 1 1e-10 1e10 EF 8 'Signal gain';"
		    "c 1 1 10 LI 1 'Channel';"
		    );
   set_load_presets(pg->set, 
		    "b 1 2 5 10 20 50 100 200 500 1000;"
		    "s 1 2 4 8 16 32 64 128 256 512;"
		    "c 1 2 3 4 5 6 7 8 9 10;"
		    );
   pg->set->val['b'-'a']= pg->gain;
   pg->set->val['s'-'a']= pg->sgain;
   pg->set->val['c'-'a']= pg->chan + 1;

   return (Page*)pg;
}

//
//	Process all data since the last time we were called
//

static void 
process_data(PageBands *pg) {
   PB_Bar *bb;
   int wr= dev->wr;		// Make sure we have a static target!
   int n_chan= dev->n_chan;
   int off= -((dev->min + dev->max + 1)/2);
   double mul= 2.0/(dev->max+1-dev->min);
   int a;

   while (pg->rd != wr) {
      int last;
      Sample *ss= SAMPLE(pg->rd);
      SAMPLE_INC(pg->rd);
      last= (pg->rd == wr);	// Do extra calculations if this is the last one
      
      for (bb= pg->bar; bb; bb= bb->nxt) {
	 sincos_step(bb->osc);
	 for (a= 0; a<n_chan; a++) {
	    double val= (ss->val[a] + off) * mul;
	    double out0= bb->lp_func(bb->chan[a].lp0, val * bb->osc[0]);
	    double out1= bb->lp_func(bb->chan[a].lp1, val * bb->osc[1]);
	    double out= hypot(out0, out1);
	    double outsm= bb->sm_func ? bb->sm_func(bb->chan[a].sm, out) : out;
	    bb->chan[a].mag= out;
	    //bb->chan[a].pha= last ? atan2(out1, out0) : 0;
	    bb->chan[a].magsm= outsm;
	 }
      }
   }
}

//
//	Restart the analysis
//
//	This zaps all the filter buffers, and rewinds the read point
//	to the given number of seconds ago, and recalculates
//	everything up to this point.  This is necessary when switching
//	to this page because our analysis data and filter buffers may
//	be completely out of date.
//

static void 
restart_analysis(PageBands *pg, double rew_sec) {
   PB_Bar *bb;
   int a;
   int n_chan= dev->n_chan;
   int rew= rew_sec * dev->rate;
   if (rew >= dev->n_smp * 9 / 10) rew= dev->n_smp * 9 / 10;
   pg->rd= dev->wr - rew;
   if (pg->rd < 0) pg->rd += dev->n_smp;
   
   // Go through zapping all the buffers
   for (bb= pg->bar; bb; bb= bb->nxt) {
      for (a= 0; a<n_chan; a++) {
	 fid_run_zapbuf(bb->chan[a].lp0);
	 fid_run_zapbuf(bb->chan[a].lp1);
	 if (bb->sm) fid_run_zapbuf(bb->chan[a].sm);
	 memset(bb->chan[a].maghist, 0, sizeof(bb->chan[a].maghist));
      }
   }

   // Process everything up to this moment
   process_data(pg);
}

//
//	Draw the signal area
//

static void 
draw_signal(PageBands *pg, int xx, int yy, int sx, int sy, int tsx) {
   int tb= 1;	// Timebase -- samples/pixel
   int a, b;
   int off= -((dev->min + dev->max + 1)/2);
   int wr= dev->wr;		// Static target, please!
   double mul= 2.0/(dev->max+1-dev->min);
   mul *= pg->sgain;

   clear_rect(xx, yy, sx, sy, pg->c_bg);
   
   for (a= 0; a<2; a++) {
      int chan= pg->chan + a;
      int inc= a ? 1 : -1;
      int ox= (sx/2) + inc * (tsx/2) - (inc < 0);
      int rd= pg->rd;
      Sample *ss;
      if (chan >= dev->n_chan) continue;
      for (; ox < sx && ox >= 0; ox += inc) {
	 double min= 2.0, max= -2.0, val;
	 int oy0, oy1, oyz, err= 0;
	 for (b= 0; b<tb; b++) {
	    SAMPLE_DEC(rd);
	    if (rd == wr) goto no_more_data;
	    ss= SAMPLE(rd);
	    val= (ss->val[chan] + off) * mul;
	    if (val < min) min= val;
	    if (val > max) max= val;
	    if (ss->err) err++;
	 }
	 oy0= (int)floor((1.0 - max) * 0.4999 * sy);
	 oy1= (int)floor((1.0 - min) * 0.4999 * sy);
	 oyz= sy/2;
	 if (err) 
	    vline(xx + ox, yy, sy, pg->c_sig2);
	 else {
	    if (oy1 < oyz) {
	       int oy= (oy1 < 0) ? 0 : oy1;
	       vline(xx + ox, yy + oy, oyz-oy, pg->c_sig0);
	    } else if (oy0 >= oyz) {
	       int oy= (oy0 > sy) ? sy : oy0;
	       vline(xx + ox, yy + oyz, oy-oyz, pg->c_sig0);
	    }
	    if (oy0 < sy && oy1 >= 0) {
	       if (oy0 < 0) oy0= 0;
	       if (oy1 >= sy) oy1= sy-1;
	       vline(xx + ox, yy+oy0, oy1-oy0+1, pg->c_sig1);
	    }
	 }
      }
   no_more_data:
   }
}

//
//	Draw the tower
//

static void 
draw_tower(PageBands *pg, int xx, int yy, int sx, int sy) {
   PB_Bar *bb;
   clear_rect(xx, yy, sx, sy, pg->c_tbg);
   colour[10]= pg->c_tbg;
   colour[11]= pg->c_tfg;
   for (bb= pg->bar; bb; bb= bb->nxt) 
      drawtext(pg->font, 
	       xx + (sx-pg->font_cx*bb->label_wid)/2, 
	       yy + pg->font_cy * bb->num,
	       bb->label);
}
	 
//
//	Draw the bars
//

static void 
draw_bars(PageBands *pg, int xx, int yy, int sx, int sy) {
   PB_Bar *bb;
   int wid= sx/2 - pg->tow_sx/2;
   int ox0= 0;
   int ox1= wid-1;
   int ox2= sx-wid;
   int ox3= sx-1;
   int cy= pg->font_cy;
   int ssy= cy >= 16 ? 4 : 2;	// Spot size-Y
   int a, b;
   clear_rect(xx+ox0, yy, ox1-ox0+1, sy, pg->c_bg);
   clear_rect(xx+ox2, yy, ox3-ox2+1, sy, pg->c_bg);
   for (bb= pg->bar; bb; bb= bb->nxt) {
      int oy= bb->num * cy;
      for (a= 0; a<2; a++) {
	 int chan= pg->chan + a;
	 int val;
	 double *dp;

	 if (chan >= dev->n_chan) continue;

	 // Bar
	 val= (int)floor(0.5 + bb->chan[chan].magsm * pg->gain * wid);	 
	 if (val > wid) val= wid;
	 clear_rect(xx + (a ? sx-wid : wid-val), yy + oy + 1, val, cy-2, bb->col);

	 // Spots
	 if (pg->spots) {
	    dp= &bb->chan[chan].maghist[0];
	    memmove(dp+1, dp, 4 * sizeof(double));
	    dp[0]= bb->chan[chan].mag;
	    for (b= 4; b>=0; b--) {
	       int sox, soy;	// Spot offset-X, offset-Y
	       val= (int)floor(0.5 + dp[b] * pg->gain * wid);
	       sox= (a ? sx-wid + val : wid-val-ssy);
	       soy= oy + cy/4 + cy * (4-b) / 8 - ssy/2;
	       alpha_rect(xx + sox, yy + soy, ssy, ssy, pg->c_spot, 100-20*b);
	    }
	 }
      }
   }
}

//
//	Event handler
//

static void 
event(Event *ev) {
   PageBands *pg= (void*)page;
   int a;
   
   //   warn("Event: %c%c%c%c sx:%d sy:%d sym:%d mod:%d uc:%d", 
   //	(ev->typ>>24)?:' ', (ev->typ>>16)?:' ', (ev->typ>>8)?:' ', (ev->typ)?:' ',
   //	ev->sx, ev->sy, ev->sym, ev->mod, ev->uc);

   //   {
   //      static int prev_tim;
   //      int tim= time_now_ms();
   //      warn("Event: %c%c%c%c %d", 
   //	   (ev->typ>>24)?:' ', (ev->typ>>16)?:' ', (ev->typ>>8)?:' ', (ev->typ)?:' ',
   //	   tim-prev_tim);
   //      prev_tim= tim;
   //   }

   // Pass it through the settings handler to see if it's an event it
   // can deal with
   if (set_event(pg->set, ev)) return;

   switch (ev->typ) {
    case 'RESZ':	// Resize (sx,sy)
       for (a= 3; a>=0; a--) {
	  short *font= (a==3) ? font10x20 : (a==2) ? font8x16 : (a==1) ? font6x12 : font6x8;
	  int cy= font[1];	// Font cell size-Y
	  int set_sy= set_rearrange(pg->set, font, disp_sx);
	  int sy= (4 + pg->n_bar + 2) * cy;	// Signal, bars, title
	  sy += set_sy + cy;			// Settings and Status
	  if (sy > disp_sy && a) continue;	// Try next if it doesn't fit
	  pg->font= font;
	  pg->font_cx= font[0];
	  pg->font_cy= font[1];
	  pg->tow_sx= (pg->label_max + 2) * pg->font_cx;
	  set_position(pg->set, 0, disp_sy - cy - set_sy);
	  break;
       }
       break;
    case 'SHOW':	// Show
       restart_analysis(pg, 10);
       tick_timer(pg->fms);
       break;
    case 'HIDE':	// Hide
       break;
    case 'SET':		// Settings change
       if (ev->sym == 'b')
	  pg->gain= ev->val;
       else if (ev->sym == 's')
	  pg->sgain= ev->val;
       else if (ev->sym == 'c') {
	  pg->chan= (int)ev->val - 1;
	  if (pg->chan < 0) pg->chan= 0;
       } else 
	  break;
       goto case_draw;
    case 'TICK':	// New frame
       goto case_draw;
    case 'DRAW':	// Redraw
    case_draw:
       {
	  int yy= 0, sy;
	  process_data(pg);
	  sy= pg->font_cy * 4;
	  draw_signal(pg, 0, yy, disp_sx, sy, pg->tow_sx);
	  yy += sy; sy= pg->font_cy * pg->n_bar;
	  draw_bars(pg, 0, yy, disp_sx, sy);
	  if (ev->typ == 'DRAW') 
	     draw_tower(pg, (disp_sx-pg->tow_sx)/2, yy, pg->tow_sx, sy);
	  yy += sy;
	  if (ev->typ == 'DRAW') {
	     suspend_update++;
	     clear_rect(0, yy, disp_sx, disp_sy-yy, pg->c_bg);
	     if (pg->title) 
		drawtext(pg->font, (disp_sx - pg->font_cx * strlen(pg->title)) / 2,
			 yy + pg->font_cy/2, pg->title);
	     set_draw(pg->set);
	     update_all();
	     draw_status(pg->font);
	  } else {
	     update(0, 0, disp_sx, yy);
	  }
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


