//
//	Code to handle display pages
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

//
//	The 'Page' gives some structure to help the page-specific
//	display and event-handling code.  Key events are always sent
//	straight to the 'key' routine.  However, mouse events are
//	handled, being used to update the list of Regions.  These
//	regions actually handle the mouse events, rather than any
//	page-global routine.
//

#ifdef HEADER

typedef struct Page Page;
typedef struct Region Region;
typedef struct KeyRegion KeyRegion;
typedef struct Event Event;

//
//	Event types, with structure members used in brackets:
//
//	'RESZ'	Resize event (sx,sy); this will always be followed by a
//		 redraw event, so this does not need to be handled if the
//	 	 redraw event already adjusts to the display size every time.
//	'SHOW'	Show event, sent when the page is switched to.  This is a 
//		 chance to set up a timer if required.  A 'DRAW' event will 
//		 follow, so there is no need to do any drawing.
//	'HIDE'	Hide event, sent when the page is switched away from.  Timers
//		 are cleared anyway, so this probably does not need to be 
//		 handled for most apps.
//	'DRAW'	Redraw event; the entire display should be redrawn and updated.
//	'KD'	Key down (sym,mod,uc)
//	'KR'	Key repeat (sym,mod,uc)
//	'KU'	Key up (sym,mod,uc)
//	'QUIT'	Quit event
//	'PAUS'	Pause (called just before going to sleep to wait for another 
//		 event -- this can be used to handle updates once the rush of
//		 mouse events (for example) have been processed, to keep things
//		 running nice and crisp)
//	'TICK'	Tick event generated from the timer, assuming a timer has been 
//		 set up for this page (using tick_timer()).
//
//	The following events are sent to regions:
//
//	'ENTR'	Region entry event (mbm,xx,yy)
//	'EXIT'	Region exit event
//	'MD'	Mouse button down (but,mbm,xx,yy)
//	'MU'	Mouse button up (but,mbm,xx,yy)
//	'MM'	Mouse movement (mbm,xx,yy,dx,dy)
//
//	Note that very simple apps ignore most of these events, just
//	watching out for the few that matter to them.
//	

struct Event {
   int typ;		// Type code
   int now;		// Time in ms at which this event was received
   int xx, yy;		// Mouse position (relative to top-left of region)
   int dx, dy;		// Mouse relative motion
   int mbm;		// Mouse buttons pressed bitmap (b0==left, b1==right, b2==mid, etc)
   int but;		// Mouse button number pressed/released (1,2,3 == left,right,mid)
   int sx, sy;		// For resize events, new window size
   int sym;		// SDL keysym
   int mod;		// Current key modifiers
   int uc;		// Unicode key translation
   double val;		// For custom events (e.g. settings value changes)
};

struct Page {
   void (*event)(Event *ev);	// Handle event (global 'page' points to Page structure)
   Region *reg;			// List of page regions
   int resize_cache_sx;		// To save sending too many resize events
   int resize_cache_sy;
};

struct Region {
   Region *nxt;
   int x0, x1, y0, y1;		// Region is x0 to x1-1, y0 to y1-1
   int inside;			// Mouse currently inside this region? (as last reported)
   void (*event)(Region*,Event*); // Handle region event: ENTR EXIT MM MU MD
   void *parent;		// Object this belongs to, or 0
};

struct KeyRegion {
   Region reg;
   int sym, mod, uc;
   int col0, col1, col2;
};

#else

#ifndef NO_ALL_H
#include "../all.h"
#endif

//
//	Release all the regions associated with a page
//

void 
free_all_regions(Page *pg) {
   while (pg->reg) {
      void *tmp= pg->reg;
      pg->reg= pg->reg->nxt;
      free(tmp);
   }
}

//
//	Release all the page regions which have 'parent' as their
//	parent.
//

void 
free_my_regions(Page *pg, void *parent) {
   Region **prvp, *p;

   prvp= &pg->reg;
   while ((p= *prvp)) {
      if (p->parent == parent) {
	 void *tmp= p;
	 *prvp= p->nxt;
	 free(tmp);
      } else {
	 prvp= &p->nxt;
      }
   }
}

//
//	Add a region to the page that maps clicks to emulated
//	keypresses.  It also changes the background colour of the
//	region between three states: default (col0), roll-over (col1)
//	and pressed (col2).  This assumes that the area is initially
//	drawn with a background colour of col0.
//

static void keyregion_event(Region *rr, Event *ev);

void 
add_keyregion(Page *pg, 
	      int x0, int x1, int y0, int y1, 
	      int col0, int col1, int col2, 
	      int sym, int mod, int uc) {
   KeyRegion *kr= ALLOC(KeyRegion);
   Region *rr= (void*)kr;
   rr->nxt= pg->reg;
   pg->reg= rr;
   rr->x0= x0;
   rr->x1= x1;
   rr->y0= y0;
   rr->y1= y1;
   rr->event= keyregion_event;
   kr->sym= sym;
   kr->mod= mod;
   kr->uc= uc;
   kr->col0= col0;
   kr->col1= col1;
   kr->col2= col2;
}

static void 
keyregion_event(Region *rr, Event *ev) {
   KeyRegion *kr= (void*)rr;
   switch (ev->typ) {
    case 'MD':
       mapcol(rr->x0, rr->y0, rr->x1-rr->x0, rr->y1-rr->y0, kr->col1, kr->col2);
       update(rr->x0, rr->y0, rr->x1-rr->x0, rr->y1-rr->y0);
       break;
    case 'MU':
       mapcol(rr->x0, rr->y0, rr->x1-rr->x0, rr->y1-rr->y0, kr->col2, kr->col1);
       update(rr->x0, rr->y0, rr->x1-rr->x0, rr->y1-rr->y0);
       ev->typ= 'KD';
       ev->sym= kr->sym;
       ev->mod= kr->mod;
       ev->uc= kr->uc;
       page->event(ev);
       break;
    case 'MM':
       break;
    case 'ENTR':
       mapcol(rr->x0, rr->y0, rr->x1-rr->x0, rr->y1-rr->y0, 
	      kr->col0, (ev->mbm&1) ? kr->col2 : kr->col1);
       update(rr->x0, rr->y0, rr->x1-rr->x0, rr->y1-rr->y0);
       break;
    case 'EXIT':
       mapcol(rr->x0, rr->y0, rr->x1-rr->x0, rr->y1-rr->y0, kr->col1, kr->col0);
       mapcol(rr->x0, rr->y0, rr->x1-rr->x0, rr->y1-rr->y0, kr->col2, kr->col0);
       update(rr->x0, rr->y0, rr->x1-rr->x0, rr->y1-rr->y0);
       break;
   }
}

//
//	Draw a button with the given text and add it as a keyregion to
//	the current page that will generate a fake keypress when
//	clicked on.  Note that the first character of the string
//	should be a colour-change character.  This is used not only to
//	colour the text, but also to give the colours to use for the
//	background when the button is rolled over or clicked.  From
//	the base index of ch-128, the colours should be:
//
//	  background default 
//	  foreground
//	  background for rollover
//	  background for when pressed
//

// @@@ Commented out temporarily until I've decided whether I really
// want a global font setting
//
//void 
//draw_keybutton(int xx, int yy, 		// Top-left
//	       int bx, int by,		// Border width in half-characters
//	       char *txt,		// Text (first character should be a colour-change)
//	       int sym, 		// Parameters for fake keypress
//	       int mod, 
//	       int uc) {
//   int wid= colstr_len(txt);
//   int *col= colour + (*txt - 128);
//   int ox= (bx * font_sx) / 2;
//   int oy= (by * font_sy) / 2;
//   int sx= (wid+bx) * font_sx;
//   int sy= (1+by) * font_sy;
//
//   clear_rect(xx, yy, sx, sy, col[0]);
//   drawtext(font, xx+ox, yy+oy, txt);
//   update(xx, yy, sx, sy);
//
//   add_keyregion(page, xx, yy, xx+sx, yy+sy, col[0], col[2], col[3], sym, mod, uc);
//}

//
//      Draw status line (redraw it, really)
//
//	The working of 'status_font' relies on the page-specific
//	'DRAW' code calling draw_status() to redraw the status line
//	when it is switched to.  From then on status() updates to the
//	page will use this font.  If the page 'DRAW' code does not
//	call draw_status() at all, then no status line will appear
//	whilst that page is active.
//

static char *status_str= 0;
static short *status_font;

void 
draw_status(short *font) {
   status_font= font;		// Cache it for status()
   if (font) {
      drawtext(font, 0, disp_sy-font[1], status_str);
      update(0, disp_sy-font[1], disp_sx, font[1]);
   }
}

//
//      Display a status line.  Colours may be selected using
//      characters from 128 onwards (see drawtext()).  There are two
//      types of status lines -- temporary ones and permanent ones.
//      Permanent ones have a '+' at the front of the formatted text
//      (although this is not displayed).  If the status is cleared
//      using status(""), then a permanent message will not go away.
//      However, it will go away if any other message is displayed,
//      including status("+").
//

void
status(char *fmt, ...) {
   va_list ap;
   char buf[4096], *p;
   static int c_perm= 0;
   int perm= 0;

   if (fmt[0] == '+') { fmt++; perm= 1; }
   if (!fmt[0] && c_perm && !perm) return;
   c_perm= perm;

   va_start(ap, fmt);
   buf[0]= 128;         // Select white on black
   vsprintf(buf+1, fmt, ap);
   p= strchr(buf, 0);
   *p++= 0x80;          // Restore white on black
   *p++= '\n';          // Blank to end of line
   *p= 0;

   if (status_str) free(status_str);
   status_str= StrDup(buf);

   // If we are active on this screen, update the status line.  (Force
   // it through even if updates are suspended.)
   if (status_font) {
      int sy= status_font[1];
      drawtext(status_font, 0, disp_sy-sy, status_str);
      update_force(0, disp_sy-sy, disp_sx, sy);
   }
}

//
//      Setup the page timer.  This timer provides a TICK event
//      approximately at the interval requested.  However, it makes no
//      attempt to 'catch up' if timing is lost; instead it schedules
//      the next TICK for the given interval after the previous one
//      was generated.  Under very heavy load (e.g. a frame rate set
//      too high for the processor), the TICK rate will go way down.
//      However, this is exactly what you want in the case of frame
//      updates.  Note that there will only ever be one TICK between
//      any two PAUS events.
//
//      Setting the interval to 0 turns off the timer, and no TICK
//      events will be generated.
//
//      If you require some aspect to be locked to real-time also
//      check the event time in 'ev->now' to see much time has elapsed
//      and adjust accordingly.
//

void
tick_timer(int ms) {
   tick_inc= ms;
   tick_targ= time_now_ms() + tick_inc;
}
 

// *** Timer now handled in the main loop, so this code no longer
// required.  It was abandoned due to getting into occasional uneven
// patterns (e.g. 30/50/30/50)
//
//	//
//	//	Timer handler
//	//
//	
//	static Uint32
//	timer_cb(Uint32 ms) {
//	   SDL_Event ev;
//	   ev.type= SDL_USEREVENT;
//	   
//	   // Ignore PushEvent return: unreliable on SDL versions before 1.2.5
//	   SDL_PushEvent(&ev);		
//	
//	   return ms;
//	}
//	
//	
//	//
//	//	Setup a timer for this page which will generate 'TICK' events
//	//	at the given inverval.  Note that only one 'TICK' will ever
//	//	arrive between any two 'PAUS' events, and any extra 'TICK's
//	//	will be dropped.  This means this is no good for accurate
//	//	timing, but only for doing regular updates that should handle
//	//	processor overload successfully.
//	//
//	
//	void 
//	page_timer(int ms) {
//	   SDL_SetTimer(ms, ms ? timer_cb : 0);
//	}


//
//	Switch from one page to another.  Clears everything that the
//	previous page might have setup, including status line and
//	timer.
//

void 
page_switch(Page *new_page) {
   Event event;

   // Tell the old page it is being hidden
   if (page) {
      event.typ= 'HIDE';
      page->event(&event);
      page= 0;
   }

   // Clear any leftover setup from the old page
   draw_status(0);
   tick_timer(0);

   // Set new page
   page= new_page;

   // Resize if this page doesn't know about the current size
   if (page->resize_cache_sx != disp_sx ||
       page->resize_cache_sy != disp_sy) {
      page->resize_cache_sx= disp_sx;
      page->resize_cache_sy= disp_sy;
      event.sx= disp_sx;
      event.sy= disp_sy;
      event.typ= 'RESZ';
      page->event(&event);
   }
   
   // Just SHOW and DRAW
   event.typ= 'SHOW';
   page->event(&event);
   event.typ= 'DRAW';
   page->event(&event);
}

#endif

// END //

