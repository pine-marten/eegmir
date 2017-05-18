//
//	F1 console screen handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	This page displays error messages as they occur, along with
//	timestamps.  It allows paging up and down the history, and
//	also reverting to 'tail' mode if you hit the end.
//
//	A function 'applog' is provided to dump information to this
//	display from anywhere in the program, from any thread.
//

#ifndef NO_ALL_H
#include "all.h"
#endif

#define HIST_LEN 1024		// Must be power of 2
#define HIST_MASK (HIST_LEN-1)
static char *history[HIST_LEN];
static int hist_wr;	// Offset of current end+1 in history array
static int hist_rd;	// Offset of current end+1 of display in history array
static int hist_tail= 1; // Tail-mode set?
static int hist_wid= 80; // Width of history display (for wrapping)
static short *hist_font;	// Font to use
static int hist_ox, hist_oy;	// Offset of top-left of region for character grid
static int hist_nc, hist_nr;	// Number of columns/rows
static int hist_redraw;		// Redraw pending?
int applog_force_update;	// Set during config so that applog calls redraw the screen

static void draw_history();

//
//	Write a string to the application history log, which appears
//	on the front screen.  Embedded colour changes are permitted,
//	using bytes 128-255.  Long lines will be word-wrapped if
//	possible, else character-wrapped.  A '\n' in the string forces
//	a wrap.
//
//	Always returns 1, which may be useful as a pass-thru error
//	return.
//

int 
applog(char *fmt, ...) {
   va_list ap;
   char buf[4096];
   char *p, *q, *r;
   int len, a;
   char tim[11];

   time_hhmmss(tim);

   va_start(ap, fmt);
   len= vsnprintf(buf, sizeof(buf), fmt, ap);
   if (len < 0 || len >= sizeof(buf)-1) 
      error("applog() call exceeded buffer");

   p= buf;
   while (1) {
      int last_col= '\x86';
      for (a= hist_wid-10, q= p; *q && *q != '\n' && a>0; q++) 
	 if (0 == (*q & 128)) a--; else last_col= *q;
      if (!(isspace(*q) || *q==0 || (*q & 128))) {
	 for (r= q-1; r >= p && !isspace(r[0]) && !(r[0]&128); r--) ;
	 if (r > p) q= r;
      }
      
      if (history[hist_wr]) free(history[hist_wr]);
      r= history[hist_wr++]= Alloc(q-p + 1 + 12);
      hist_wr &= HIST_MASK;
      if (tim[0])
	 r += sprintf(r, "\x88%s\x86 ", tim); 
      else 
	 r += sprintf(r, "\x86         "); 
      tim[0]= 0;	       // Time only on first line if wrapping
      memcpy(r, p, q-p);
      r[q-p]= 0;
      p= q;
      if (*p == '\n') p++;
      else while (isspace(*p)) p++;
      if (!*p) break;
      *--p= last_col;
   }

   // In server mode, we dump it out to STDERR, less the colours
   if (server) {
      while (hist_rd != hist_wr) {
	 for (p= history[hist_rd++]; *p; p++)
	    if (*p >= 32) 
	       fputc(*p, stderr);
	 hist_rd &= HIST_MASK;
	 fputc('\n', stderr);
      }
      return 1;
   }

   // Force an update of the screen at this point if requested and
   // possible
   if (applog_force_update &&
       SDL_ThreadID() == main_threadid &&
       page == p_fn[0]) {
      if (hist_tail) hist_rd= hist_wr;
      draw_history();
   }

   // @@@ Maybe force an update of the screen at this point if the F1
   // screen is active -- but remember we may be running in a
   // different thread here.  In any case, GUI thread code can pick it
   // up from the change to hist_wr.
   return 1;
}

//
//	Setup page-handler
//

static void event(Event *ev);

Page *
p_console_init() {
   Page *pg= ALLOC(Page);
   pg->event= event;
   hist_font= font6x8;		// Safe default if no 'RESZ' event ever arrives
   hist_wid= 80;		// Safe default
   return pg;
}

//
//	Redraw the text area and update
//

static void 
draw_history() {
   int off, a;

   clear_rect(0, 0, disp_sx, disp_sy-hist_font[1], colour[0]);
   off= hist_rd;
   for (a= hist_nr-1; a>=0; a--) {
      off= (off-1) & HIST_MASK;
      if (off == hist_wr) break;
      if (!history[off]) break;
      drawtext(hist_font, hist_ox, hist_oy + hist_font[1]*a, history[off]);
   }
   if (!hist_tail) {
      char buf[128];
      sprintf(buf, "\x84 PAGING BACK: %d to %d \x80", 
	      ((hist_wr-hist_rd) & HIST_MASK) + (hist_nr-1),
	      ((hist_wr-hist_rd) & HIST_MASK));
      drawtext(hist_font, hist_ox, hist_oy + hist_font[1]*hist_nr, buf);
   }
   update(0, 0, disp_sx, disp_sy-hist_font[1]);
}


//
//	Event handler
//

static void 
event(Event *ev) {
   int a;

   // warn("Event: %c%c%c%c sx:%d sy:%d sym:%d mod:%d uc:%d", 
   // (ev->typ>>24)?:' ', (ev->typ>>16)?:' ', (ev->typ>>8)?:' ', (ev->typ)?:' ',
   // ev->sx, ev->sy, ev->sym, ev->mod, ev->uc);

   switch (ev->typ) {
    case 'RESZ':	// Resize (sx,sy)
       if (ev->sx / 10 >= 80) 
	  hist_font= font10x20;
       else if (ev->sx / 8 >= 80) 
	  hist_font= font8x16;
       else 
	  hist_font= font6x12;
       hist_nc= ev->sx / hist_font[0];
       hist_nr= ev->sy / hist_font[1];
       hist_ox= (ev->sx - hist_nc * hist_font[0]) / 2;
       hist_oy= (ev->sy - hist_nr * hist_font[1]) / 2;
       hist_wid= hist_nc;
       hist_nr--;	// Drop one line for use as the status line
       hist_nr--; 	// and another for the MORE line
       break;
    case 'SHOW':	// Show
       tick_timer(100);		// Check for new log entries 10 times a second
       break;
    case 'HIDE':	// Hide
       break;
    case 'DRAW':	// Redraw
       if (hist_tail) hist_rd= hist_wr;
       hist_redraw= 0;
       draw_history();
       draw_status(hist_font);
       update_all();
       break;
    case 'PAUS':	// All redraws are held off until this point
       if (hist_redraw) {
	  hist_redraw= 0;
	  draw_history();
       }
       break;
    case 'TICK':	// Check to see if there are any new log entries
       if (hist_tail && hist_rd != hist_wr) {
	  hist_rd= hist_wr;
	  hist_redraw= 1;
       }
       break;
    case 'QUIT':	// Quit event
       exit(0);
       break;
    case 'KU':		// Key up (sym,mod,uc)
       break;
    case 'KD':		// Key down (sym,mod,uc)
    case 'KR':		// Key repeat (sym,mod,uc)
       {
	  // Check to see where we are in the list
	  int top= hist_rd;
	  int at_top;
	  int at_bot;
	  for (a= hist_nr; a>0; a--) {
	     top= (top-1) & HIST_MASK;
	     if (top == hist_wr) break;
	     if (!history[top]) break;
	  }
	  at_top= a>0;
	  at_bot= (hist_rd == hist_wr);

	  // Handle the keys.  Note that the redraw itself is flagged
	  // to be picked up on the next 'PAUS' event, so there is
	  // never a big delay for a massive backlog of repeat events.
	  switch (ev->sym) {
	   case SDLK_UP:
	      if (!at_top) {
		 hist_tail= 0;
		 hist_rd--;
		 hist_rd &= HIST_MASK;
		 hist_redraw++;
	      }
	      break;
	   case SDLK_RETURN:
	   case SDLK_DOWN:
	      if (!at_bot) {
		 hist_rd++;
		 hist_rd &= HIST_MASK;
		 if (hist_rd == hist_wr) 
		    hist_tail= 1;
		 hist_redraw++;
	      }
	      break;
	   case SDLK_BACKSPACE:
	   case SDLK_PAGEUP:
	      if (!at_top) {
		 hist_tail= 0;
		 hist_rd -= hist_nr-1;
		 hist_rd &= HIST_MASK;
		 hist_redraw++;
	      }
	      break;
	   case SDLK_SPACE:
	   case SDLK_PAGEDOWN:
	      if (!at_bot) {
		 for (a= hist_nr-1; a>0; a--) {
		    hist_rd++;
		    hist_rd &= HIST_MASK;
		    if (hist_rd == hist_wr) break;
		 }
		 if (hist_rd == hist_wr) 
		    hist_tail= 1;
		 hist_redraw++;
	      }
	      break;
	   case 'd':
	      if (!at_bot) {
		 for (a= hist_nr/2; a>0; a--) {
		    hist_rd++;
		    hist_rd &= HIST_MASK;
		    if (hist_rd == hist_wr) break;
		 }
		 if (hist_rd == hist_wr) 
		    hist_tail= 1;
		 hist_redraw++;
	      }
	      break;
	  }
       }
   }
}

// END //


