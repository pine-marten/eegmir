//
//	Code to handle a text/graphics console
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

//
//	A console is an area of the screen that is split into a grid
//	of character cells.  Into these may be written characters of
//	any colour, and also pixel graphics.  Areas of text/graphics
//	may be marked as buttons, which respond to mouse rollover and
//	clicking.  On a click, they generate a sequence of keypress
//	events, allowing keyboard operations to be driven by the
//	mouse.
//
//	All of the console display, including the graphics, are stored
//	in the Console structure, so a Console may be operated hidden,
//	and then displayed at a later time without problem.
//
//	The console code is tied into the Page code (to handle the
//	mouse regions for the buttons and the faked events), and into
//	the graphics code (to handle drawing text and graphics into
//	the framebuffer).
//
//	  Console *con;
//	
//	Create a new Console, initially visible, on the current page
//	('page') with the given font (e.g. 'font8x16'), covering the
//	given area, initially cleared to the background colour
//	colour[col], with colour[col+1] as the default foreground
//	colour for text.  Afterwards, 'con->nr' and 'con->nc' give the
//	number of rows and columns.
//	
//	  con= con_new(font, xx, yy, sx, sy, col);
//	  con->nr;
//	  con->nc;
//
//	Delete the entire Console.
//
//	  con_delete(con);
//
//	Move and resize the Console to the given dimensions.  Resizing
//	is not a friendly operation for a Console.  The bottom-left
//	part of the console area is maintained.  Character cells will
//	be added/truncated on the top and right edges.  This call can
//	only be made when the console is hidden, so call con_hide()
//	first.
//
//	  con_move(con, xx, yy, sx, sy);
//
//	Hide the console.  Mouse-over regions will no longer react,
//	and the screen area associated with the console will not be
//	updated in any way.  The console contents may still be updated
//	and modified whilst it is hidden, though.
//
//	  con_hide(con);
//
//	Show the console.  This may also be called at any point to
//	force a full redraw of the region controlled by the console.
//
//	  con_show(con);
//
//	Clear a number of lines of the console to colour[col], with
//	colour[col+1] as the default foreground.
//
//	  con_clear(con, yy, sy, col);
//
//	Scroll a number of lines of the console.  The uncovered area
//	is filled with colour[col] as the background, with
//	colour[col+1] as the default foreground.  'off' is -ve for
//	scrolling upwards, or +ve for scrolling downwards.
//
//	  con_scroll(con, yy, sy, off, col);
//
//
//	The following few calls are used to modify an area of the
//	console screen.  You start out by pointing out the place you
//	want to write to using con_pos() or con_area().  Next you
//	write to it using one or more calls to con_wr(), using
//	con_plot() to fill in any graphical areas.  Finally, the data
//	is written to the buffer and display, either on the next call
//	to a con_* routine, or explicitly using con_flush().
//
//
//	Set the position for writing to in terms of character cell
//	coordinates (0,0 being top-left).  This allows the display
//	from this position to the end of the line to be modified.  If
//	less characters than the available space are written, then the
//	original trailing characters remain.  Using a '\n' avoids this
//	by clearing to the end of the line.  The default page colour
//	is used unless this is overridden.
//
//	  con_pos(con, xx, yy);
//
//	Set the position more accurately.  'px' gives the number of
//	pixels across to start.
//
//	  con_ppos(con, px, yy);
//
//	Set an area to write to.  This is similar to setting a
//	position, except that it is not possible to write beyond the
//	end of the area given, and the area is always filled to the
//	end with white space, as if a '\n' was included.  The 'p'
//	version uses pixel-coords instead of cell-coords for the
//	horizontal positioning.
//
//	  con_area(con, xx, yy, sx);
//	  con_parea(con, px, yy, psx);
//
//	Write some characters to the current working position.
//	Returns the current column after completing the operation.
//	The format string may include normal printf sequences, but in
//	addition there are special sequences used for changing
//	colour/etc, which are documented below.  
//
//	  xx= con_wr(con, fmt, ...);
//
//	Plot a pixel into the last-mentioned graphical area (in a
//	recent con_wr() call).  Pixel values are given as for the
//	graphics library -- i.e. either generated or a colour[] value.
//	Other means of getting data into the graphics area could be
//	provided -- but this will do for now.
//
//	  con_plot(con, xx, yy, val);
//
//	Flush recent con_wr() and con_plot() changes out to the
//	console display.  The flush is done automatically if any call
//	other than con_wr() or con_plot() is done, but sometimes you
//	do need to flush the data out explicitly with this call.
//
//	  con_flush(con);
//	
//	
//	Console formatting sequences
//	----------------------------
//
//	Real '[' or ']' characters.  Note that '[' or ']' characters
//	within strings included via %s/etc do not need to be escaped
//	in this way.
//
//	  [[
//	  ]]
//
//	Change colour.  The new text background colour becomes
//	colour[col], with foreground colour[col+1].  If a link
//	appears, then colour[col+2] gives the roll-over background,
//	and colour[col+3] gives the pressed background colour.
//
//	  [c43]		i.e. [c<col>]
//
//	Insert a graphical area.  This area may be addressed using
//	con_plot() after this call has completed.  The width is the
//	size of the area given either in cells or pixels.
//
//	  [G4]		i.e. [G<width-cells>]
//	  [g40]		i.e. [g<width-pixels>]
//
//	Insert a transparent area.  The area is left untouched on the
//	display (except that it will be scrolled if the console is
//	scrolled), and it would normally be written to directly using
//	the graphics library calls.  This allows an independent
//	graphical area to be embedded within a Console display.  The
//	width is the size of the area given either in cells or pixels.
//
//	  [T4]		i.e. [T<width-cells>]
//	  [t40]		i.e. [t<width-pixels>]
//
//	Start and end a button area.  If the button is clicked, then
//	the <key-spec> text is interpreted as a sequence of keypresses
//	to feed to the application.  Other areas on the page with an
//	identical <key-spec> will also respond to roll-over of this
//	area at the same time.
//
//	  [/M-x 'goto-line' C-m]	i.e. [/<key-spec>] 
//	  [/]
//
//	Fill to the end of the line or area with spaces.
//
//	  \n
//
//	Insert a space of the given number of pixels or cells.  [h]
//	gives a half-cell space.
//
//	  [S4]				i.e. [S<width-cells>]
//	  [s40]				i.e. [s<width-pixels>]
//	  [h]				
//
//	Adjust the current text vertical offset.  This permits the
//	text to be moved up and down within the character cell.  If it
//	goes off the top or bottom, it is truncated.  Two lines with
//	identical text/graphics, one using [v50] and the other using
//	[v-50] will appear as a single line offset by half a character
//	cell.
//
//	  [V-50]			i.e. [V<offset-percent>]
//	  [v-5]				i.e. [v<offset-pixels>]
//
//

//
//	Internal storage of console-line data
//	-------------------------------------
//
//	Using UTF-8 encoding, we can store integers as character
//	values, which is efficient for small values.  In the
//	following, I'm using .B to indicate byte values, .N to
//	indicate UTF-8 encoded values, and .SN to indicate signed
//	values encoded as UTF-8 values.
//	
//	Stored text and other elements are in packets:
//	
//        'X' <colour-index.N> <v-offset.SN> <nul-term-link-spec.N> <nul-term-chars.N>
//      
//           Text region, with given colours, link and vertical offset.
//      
//        'S' <colour-index.N> <width-pixels.N> <nul-term-link-spec.N>
//      
//           White space of given colour and width.
//      
//        'G' <colour-index.N> <width-pixels.N> <nul-term-link-spec.N> <raw-data.B>
//      
//           Graphical area
//      
//        'T' <width-pixels.N>
//      
//           Transparent area
//      
//        'E'
//      
//           End marker
//


#ifdef HEADER

typedef struct Console Console;
typedef struct ConKey ConKey;
typedef struct ConRegion ConRegion;

struct Console {
   int xx, yy, sx, sy;	// Display size
   int nr, nc;		// Number of rows, columns
   int col;		// Default colour for the whole page (from con_new)
   int show;		// Currently visible?
   char **line;		// Line stores
   char *wrk;		// Work buffer if currently updating an area, else 0
   char *wp, *wrkend;	// Current work pointer and end of work buffer
   int wrk_xx, wrk_yy;	// Place where work buffer should be substituted
   int wrk_sx;		// Number of horizontal pixels covered, or -1 if not specified
   char *plot;		// Last mentioned graphical area, for con_plot()
   int plot_sx;		// Width of plot area in pixels
   ConKey *key;		// List of currently-active keys on the display
};

struct ConKey {
   ConKey *nxt;		// Next key, or 0
   ConRegion *reg;	// List of regions associated with this key
   int state;		// Current display state: 0 default, 1 rollover, 2 pressed
   int hash;		// Hash of keystring
   char key[4];		// Keystring
};

struct ConRegion {
   ConRegion *nxt;	// Next region, or 0
   int x0, x1, y0, y1;	// Region, relative
   int col;		// Base colour for background colour changes
   int inside;		// Is mouse currently inside?
};

#else

#ifndef NO_ALL_H
#include "../all.h"
#endif


//
//	Return an allocated block just containing a blank line with
//	the given colour index and length.
//

static char *
blank_line(int col, int wid) {
   char buf[16];
   char *p= buf;

   *p++= 'S';
   wrN(&p, col);
   wrN(&p, wid);
   *p++= 0;
   *p++= 'E';
   
   return DatDup(buf, p-buf);
}

//
//	Region-handling code
//

// Returns a pointer to the pointer pointing to the one we are after, or 0
static ConKey **
find_key(Console *con, char *key) {
   int hash= strhash(key);
   ConKey **prvp= &con->key;
   ConKey *p;

   while (p= *prvp) {
      if (p->hash == hash && 0 == strcmp(p->key, key))
	 return prvp;
      prvp= &p->nxt;
   }
   return 0;
}

// Returns a pointer to the old/new ConKey
static ConKey *
find_or_add_key(Console *con, char *key) {
   ConKey **prvp= find_key(con, key);
   ConKey *p;
   int len;

   if (prvp) return *prvp;
   
   len= strlen(key);
   p= (ConKey*)Alloc(sizeof(*p) - sizeof(p->key) + len + 1);
   p->hash= strhash(key);
   strcpy(p->key, key);
   p->nxt= con->key;
   con->key= p;

   return p;
}

static void 
add_region(char *key, int x0, int x1, int y0, int y1, int col) {
   ConKey *ck= find_or_add_key(key);
   ConRegion *cr= ALLOC(ConRegion);
   cr->x0= x0;
   cr->x1= x1;
   cr->y0= y0;
   cr->y1= y1;
   cr->col= col;

   cr->nxt= ck->reg;
   ck->reg= cr;
}

// Delete all the regions with top-left within the given area
static void 
del_regions(Console *con, int x0, int x1, int y0, int y1) {
   ConKey *kk, **kkp;
   ConRegion *rr, **rrp;

   for (kkp= &con->key; kk= *kkp; ) {
      for (rrp= &kk->reg; rr= *rrp; ) {
	 if (rr->x0 >= x0 && rr->x0 < x1 &&
	     rr->y0 >= y0 && rr->y0 < y1) {
	    *rrp= rr->nxt;
	    free(rr);
	 } else 
	    rrp= &rr->nxt;
      }
      if (!kk->reg) {
	 *kkp= kk->nxt;
	 free(kk);
      } else 
	 kkp= &kk->nxt;
   }
}

// Delete all the regions 
static void 
del_all_regions(Console *con) {
   ConKey *kk;
   ConRegion *rr;

   while (con->key) {
      kk= con->key;
      con->key= kk->nxt;
      
      while (kk->reg) {
	 rr= con->key;
	 kk->reg= rr->nxt;
	 free(rr);
      }
      free(kk);
   }
}

// Scroll regions within the given area, deleting regions that drop
// off the edge
static void 
scroll_regions(Console *con, int x0, int x1, int y0, int y1, int yoff) {
   ConKey *kk, **kkp;
   ConRegion *rr, **rrp;

   for (kkp= &con->key; kk= *kkp; ) {
      for (rrp= &kk->reg; rr= *rrp; ) {
	 if (rr->x0 >= x0 && rr->x0 < x1 &&
	     rr->y0 >= y0 && rr->y0 < y1) {
	    rr->y0 += yoff;
	    rr->y1 += yoff;
	    if (!(rr->y0 >= y0 && rr->y0 < y1)) {
	       *rrp= rr->nxt;
	       free(rr);
	    } else
	       rrp= &rr->nxt;
	 } else
	    rrp= &rr->nxt;
      }
      if (!kk->reg) {
	 *kkp= kk->nxt;
	 free(kk);
      } else 
	 kkp= &kk->nxt;
   }
}

// Change the background of the regions to match the given state
static void 
draw_regions(Console *con, ConRegion *rr, int state) {
   int to= (state == 0) ? 0 : (state == 1) ? 2 : 3;
   int fr0= (state == 0) ? 2 : (state == 1) ? 3 : 0;
   int fr1= (state == 0) ? 3 : (state == 1) ? 0 : 2;

   for (; rr; rr= rr->nxt) {
      mapcol(con->xx + rr->x0, con->yy + rr->y0,
	     rr->x1 - rr->x0, rr->y1 - rr->y0, 
	     colour[rr->col + fr0], colour[rr->col + to]);
      mapcol(con->xx + rr->x0, con->yy + rr->y0,
	     rr->x1 - rr->x0, rr->y1 - rr->y0, 
	     colour[rr->col + fr1], colour[rr->col + to]);
   }
}

// Update the display according to the mouse status
static void 
update_regions(Console *con, int xx, int yy, int but) {
   ConKey *kk;
   ConRegion *rr;

   for (kk= con->key; kk; kk= kk->nxt) {
      int state= 0;
      for (rr= kk->reg; rr; rr= rr->nxt) {
	 rr->inside= (xx >= rr->x0 && xx < rr->x1 &&
		      yy >= rr->y0 && yy < rr->y1);
	 if (rr->inside) state= but ? 2 : 1;
      }
      if (state != kk->state) {
	 kk->state= state;
	 draw_regions(con, kk->reg, state);
      }
   }
}

// Register a 'click' at the given point, and take action if it is
// within a region
static void 
click_region(Console *con, int xx, int yy) {
   ConKey *kk;
   ConRegion *rr;

   for (kk= con->key; kk; kk= kk->nxt) {
      for (rr= kk->reg; rr; rr= rr->nxt) {
	 if (xx >= rr->x0 && xx < rr->x1 &&
	     yy >= rr->y0 && yy < rr->y1)
	    action_keyspec(kk->key);
      }
   }
}   

// Redraw all the displayed regions
static void 
redraw_regions(Console *con) {
   ConKey *kk;

   for (kk= con->key; kk; kk= kk->nxt) 
      if (kk->state != 0)
	 draw_regions(con, kk->reg, kk->state);
}   
	 

//
//	Console code
//
   
Console *
con_new(int xx, int yy, int sx, int sy, int col) {
   Console *con= ALLOC(Console);
   int a;
   
   con->xx= xx;
   con->yy= yy;
   con->sx= sx;
   con->sy= sy;
   con->nr= sy / font_sy;
   con->nc= sx / font_sx;
   con->col= col;
   con->line= ALLOC_ARR(con->nr, char*);

   for (a= 0; a<con->nr; a++)
      con->line[a]= blank_line(col, sx);

   con_show(con);

   return con;
}


void 
con_delete(Console *con) {
   int a;

   if (con->show)
      free_my_regions(page, con);

   if (con->line) {
      for (a= 0; a<con->nr; a++) 
	 free(con->line[a]);
      free(con->line);
   }

   del_all_regions(con);

   if (con->wrk) free(con->wrk);
   free(con);
}

void 
con_move(Console *con, int xx, int yy, int sx, int sy) {
   if (con->wrk) con_flush(con);
   @@@;
}

void 
con_hide(Console *con) {
   free_my_regions(page, con);
   con->show= 0;
   if (con->wrk) con_flush(con);
}

static void 
con_event(Region *rr, Event *ev) {
   Console *con= rr->parent;
   switch (ev->typ) {
    case 'ENTR':
    case 'MM':
    case 'MD':
       update_regions(con, ev->xx, ev->yy, ev->mbm&1);
       break;
    case 'EXIT':
       update_regions(con, -1, -1, 0);
       break;
    case 'MU':
       update_regions(con, ev->xx, ev->yy, ev->mbm&1);
       if (but == 1) click_region(con, ev->xx, ev->yy);
       break;
   }
}

void 
con_show(Console *con) {
   if (con->wrk) con_flush(con);

   con->show= 1;

   // Add a region to the current page, enabling mouse operations
   {
      Region *rr= ALLOC(Region);
      rr->x0= con->xx;
      rr->x1= con->xx + con->sx;
      rr->y0= con->yy;
      rr->y1= con->yy + con->sy;
      rr->event= con_event;
      rr->parent= con;

      rr->nxt= page->reg;
      page->reg= rr;
   }
   
   @@@ Draw the entire console, and do an update() for the region;
}

void 
con_clear(Console *con, int yy, int sy, int col) {
   if (con->wrk) con_flush(con);

   if (yy < 0) { sy += yy; yy= 0; }
   if (yy + sy > con->nr) { sy= con->nr - yy; }
   if (sy <= 0) return;
   
   for (a= 0; a<con->nr; a++) {
      free(con->line[a]);
      con->line[a]= blank_line(col);
   }
   
   del_regions(con, 0, con->sx, yy*font_sy, (yy+sy)*font_sy);

   if (con->show) {
      int rxx= con->xx;
      int ryy= con->yy + yy * font_sy;
      int rsx= con->sx;
      int rsy= sy * font_sy;

      clear_rect(rxx, ryy, rsx, rsy, colour[col]);
      update(rxx, ryy, rsx, rsy);
   }
}

void 
con_scroll(Console *con, int yy, int sy, int off, int col) {
   int dy0, dy1;	// Deleted area
   int uy0, uy1;	// Uncovered area
   int a;

   if (con->wrk) con_flush(con);

   if (yy < 0) { sy += yy; yy= 0; }
   if (yy + sy > con->nr) { sy= con->nr - yy; }
   if (sy <= 0) return;

   if (off == 0) return;
   if (off >= sy || -off >= sy) {
      con_clear(con, yy, sy, col);
      return;
   }

   if (off > 0) {
      uy0= yy;
      uy1= yy+off;
      dy0= yy+sy-off;
      dy1= yy+sy;
   } else {
      dy0= yy;
      dy1= yy-off;
      uy0= yy+sy+off;
      uy1= yy+sy;
   }

   // Update the line array
   for (a= dy0; a<dy1; a++) free(con->line[a]);
   
   if (off > 0) 
      memmove(con->line + off, con->line, (sy-off)*sizeof(char*));
   else 
      memmove(con->line, con->line - off, (sy+off)*sizeof(char*));

   for (a= uy0; a<uy1; a++) con->line[a]= blank_line(col);

   // Update the regions
   scroll_regions(con, 0, con->sx, yy*font_sy, (yy+sy)*font_sy, off * font_sy);
   
   // Update the screen
   if (con->show) {
      scroll(con->xx, con->yy + yy * font_sy, 
	     con->sx, sy * font_sy, 
	     0, off * font_sy);
      clear_rect(con->xx, con->yy + uy0*font_sy,
		 con->sx, con->yy + uy1*font_sy, colour[col]);
      update(con->xx, con->yy + yy * font_sy,
	     con->sx, sy * font_sy);
   }
}

inline void 		// inline so that it gets inlined into the other con_* functions
con_parea(Console *con, int xx, int yy, int sx) {
   int len= 1024;
   if (con->wrk) con_flush(con);

   con->wrk= Alloc(len);
   con->wp= con->wrk;
   con->wrkend= con->wrk + len;
   con->wrk_xx= xx;
   con->wrk_yy= yy;
   con->wrk_sx= sx;
}

void 
con_area(Console *con, int xx, int yy, int sx) {
   con_parea(con, xx*font_sx, yy, sx*font_sx);
}

void
con_ppos(Console *con, int xx, int yy) {
   con_parea(con, xx, yy, -1);
}

void 
con_pos(Console *con, int xx, int yy) {
   con_parea(con, xx*font_sx, yy, -1);
}

// Make the work area bigger
char *
con_wrk_realloc(Console *con, char *wp) {
   int max= con->wrkend-con->wrk;
   int len= wp-con->wrk;
   int plot= con->plot-con->wrk;
   char *tmp= Alloc(2 * max);
   memcpy(tmp, con->wrk, len);
   free(con->wrk);
   con->wrk= tmp;
   con->wrkend= tmp + 2 * max;
   con->plot= tmp + plot;
   return con->wp= tmp + len;
}
   
#define WP_CHECK(len) \
   while (wp + (len) > con->wrkend) \
      wp= con_wrk_realloc(con, wp);

static char hex[16]= "0123456789ABCDEF";

static int 
argUI(char **pp, int *rvp) {
   char *p= *pp;
   int val= 0;
   if (!isdigit(*p)) return 0;
   while (isdigit(*p)) val= val*10 + (*p++ - '0');
   if (*p++ != 29) return 0;
   *rvp= val;
   *pp= p;
   return 1;
}

static int 
argSI(char **pp, int *rvp) {
   char *p= *pp;
   int val, neg;
   if (neg= (*p == '-')) p++;
   if (!argUI(&p, &val)) return 0;
   *pp= p;
   *rvp= neg ? -val : val;
   return 1;
}

static int 
argUIdef(char **pp, int *rvp, int def) {
   char *p= *pp;
   if (*p == 29) { *rvp= def; *pp= p+1; return 1; }
   return argUI(pp, rvp);
}

void 
con_wr(Console *con, char *fmt, ...) {
   va_list ap;
   char buf[4096], *bufend= buf + sizeof(buf);
   unsigned char *p, *q, ch;
   int len, cnt, val;
   char *wp;
   int in_txt;		// Currently in a text section ?
   char *link;		// Current link (StrDup'd), never 0
   int link_len;	// Length of data to copy, including NUL

   va_start(ap, fmt);

   link= StrDup("");
   link_len= 1;

   p= fmt;
   q= buf;
   while (ch= *p++) {
      if (ch == '[' && *p == '[')
	 p++;
      else if (ch == '[')
	 ch= 27;
      else if (ch == ']' && *p == ']')
	 p++;
      else if (ch == ']')
	 ch= 29;
      *q++= ch;
   }
   *q++= 0;

   if (q > bufend)
      error("con_wr() internal buffer overflow");

   len= bufend-q;
   cnt= vsnprintf(q, len, buf, ap);
   if (cnt < 0 || cnt >= len)
      error("con_wr() internal buffer overflow");

   // Okay, we have the formatted string at 'q'.  We now need to add
   // this to con->wp

   wp= con->wp;
   in_txt= 0;
   col= con->col;
   voff= 0;

   for (p= q; ch= *p++; ) {
      WP_CHECK(16 + link_len);

      // Just copy all non-control characters straight over
      if (ch >= ' ') {
	 if (!in_txt) {
	    *wp++= 'X';
	    wrN(&wp, col);
	    wrSN(&wp, voff);
	    memcpy(wp, link, link_len); wp += link_len;
	    in_txt= 1;
	 }
	 *wp++= ch;
	 continue;
      }

      // Guaranteed not to be in a text-block after this point
      if (in_txt) { *wp++= 0; in_txt= 0; }

      if (ch == '\n') {
	 *wp++= 'S';
	 wrN(&wp, col);
	 wrN(&wp, con->sx);
	 *wp++= 0;
	 goto done;	// No point in reading any more
      }
      
      if (ch == 27) {
	 switch (ch= *p++) {
	  case 'c':	// Change colour
	     if (!argUI(&p, &col)) goto bad;
	     break;
	  case 'G':	// Graphics area
	     if (!argUI(&p, &val)) goto bad;
	     val *= font_sx;
	     goto case_gG;
	  case 'g':
	     if (!argUI(&p, &val)) goto bad;
	 case_gG:
	    *wp++= 'G';
	    wrN(&wp, col);
	    wrN(&wp, val);
	    memcpy(wp, link, link_len); wp += link_len;
	    len= (disp_pix32 ? sizeof(Uint32) : sizeof(Uint16)) * font_sy * val;
	    WP_CHECK(len);
	    con->plot= wp;
	    con->plot_sx= val;
	    //@@@ necessary to clear the area? it will be zero initially
	    wp += len;
	    break;
	  case 'T':	// Transparent area
	     if (!argUI(&p, &val)) goto bad;
	     val *= font_sx;
	     goto case_tT;
	  case 't':
	     if (!argUI(&p, &val)) goto bad;
	 case_tT:
	    *wp++= 'T';
	    wrN(&wp, val);
	    break;
	  case '/':	// Change link
	     q= p;
	     while (*p && *p != 29) p++;
	     if (*p != 29) goto bad;
	     *p++= 0;
	     free(link);
	     link= StrDup(q);
	     link_len= p-q;
	     break;
	  case 'h':
	     if (!argUIdef(&p, &val, 1)) goto bad;
	     val *= font_sx/2;
	     goto case_sS;
	  case 'S':
	     if (!argUI(&p, &val)) goto bad;
	     val *= font_sx;
	     goto case_sS;
	  case 's':
	     if (!argUI(&p, &val)) goto bad;
	 case_sS:
	    *wp++= 'S';
	    wrN(&wp, col);
	    wrN(&wp, val);
	    memcpy(wp, link, link_len); wp += link_len;
	    break;
	  case 'V':
	     if (!argSI(&p, &voff)) goto bad;
	     voff *= font_sy / 100;
	     break;
	  case 'v':
	     if (!argSI(&p, &voff)) goto bad;	
	     break;
	  default:
	     p--;
	     goto bad;
	 bad:	// Insert an error marker and display the rest of the sequence as character
	    *wp++= 'S';
	    *wp++= 3;  	// Colour 3
	    *wp++= font_sx/2;
	    *wp++= 0;
	    *wp++= 'S';
	    *wp++= 2;  	// Colour 2
	    *wp++= font_sx - font_sx/2;
	    *wp++= 0;
	    break;
	 }
	 continue;
      }

      // Some control character we don't support (relies on
      // colour[2..3] being error colours); generates e.g. |07|
      WP_CHECK(32);
      *wp++= 'S';
      *wp++= 3;	       	// Colour 3
      *wp++= font_sx/2;
      *wp++= 0;
      *wp++= 'X';
      *wp++= 2;		// Colours 2,3
      *wp++= 0;		// No voff
      *wp++= 0;		// No link
      *wp++= hex[ch >> 4];
      *wp++= hex[ch & 15];
      *wp++= 0;
      *wp++= 'S';
      *wp++= 0;	       	// Colour 0
      *wp++= font_sx - (font_sx/2);
      *wp++= 0;
      continue;
   }

 done:
   if (in_txt) { *wp++= 0; in_txt= 0; }
   con->wp= wp;
   if (link != null) free(link);
   return;
}

void 
con_plot(Console *con, int xx, int yy, int val) {
   int off;

   if (!con->plot ||
       xx < 0 || xx >= con->plot_sx ||
       yy < 0 || yy >= font_sy) 
      return;

   off= xx + yy * con->plot_sx;
   
   if (disp_pix32) 
      ((Uint32*)con->plot)[off]= val;
   else if (disp_pix16) 
      ((Uint16*)con->plot)[off]= val;
}

void 
con_flush(Console *con) {

   // Job is to insert the sequence of packets in the work buffer into
   // the space defined by wrk_*.  Tidy-up required: current line
   // might require a final character turned into a partial space, and
   // also the character at the end of the section being inserted, and
   // in the original line at the ending position.  All of this needs
   // to be updated to the display.  Entries in the region lists need
   // to be deleted or created.
   @@@@@@@@@
}


//
//	Draw the single character given.  Inlined for speed.
//

static inline void 
drawchar(short *fp, int xx, int yy, int sx, int sy, int ci) {
   int bg= col[ci];
   int fg= col[ci+1];

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (sy-- > 0) {
	 for (val= *fp++; val != 1; val >>= 1)
	    *p++= (val&1) ? fg : bg;
	 dp += disp_my - sx;
      }
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (sy-- > 0) {
	 for (val= *fp++; val != 1; val >>= 1)
	    *p++= (val&1) ? fg : bg;
	 dp += disp_my - sx;
      }
   }
}
      
//
//	Draw the single character given with the given offset.  Not
//	inlined as this is used less often.
//

static void 
drawchar_offset(short *fp, int xx, int yy, int sy, int ci, int oy) {
   int bg= col[ci];
   int fg= col[ci+1];
   int a;
   int cnt= sy;

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (oy < 0) {
	 for (a= sx; a>0; a--) *dp++= bg;
	 dp += disp_my - sx;
	 oy++; sy--; cnt--;
      }
      if (oy > 0) { fp += oy; cnt -= oy; }
      while (cnt-- > 0) {
	 for (val= *fp++; val != 1; val >>= 1)
	    *p++= (val&1) ? fg : bg;
	 dp += disp_my - sx; sy--;
      }
      while (sy-- > 0) {
	 for (a= sx; a>0; a--) *dp++= bg;
	 dp += disp_my - sx;
      }
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (oy < 0) {
	 for (a= sx; a>0; a--) *dp++= bg;
	 dp += disp_my - sx;
	 oy++; sy--; cnt--;
      }
      if (oy > 0) { fp += oy; cnt -= oy; }
      while (cnt-- > 0) {
	 for (val= *fp++; val != 1; val >>= 1)
	    *p++= (val&1) ? fg : bg;
	 dp += disp_my - sx; sy--;
      }
      while (sy-- > 0) {
	 for (a= sx; a>0; a--) *dp++= bg;
	 dp += disp_my - sx;
      }
   }
}   

//
//	Dump graphic data to the display
//

static inline void
drawgraphic(void **pp, int xx, int yy, int sx, int sy) {
   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      Uint32 *src= *pp;
      while (sy-- > 0) {
	 memcpy(dp, src, sx * sizeof(*src));
	 dp += disp_my;
	 src += sx;
      }
      *pp= src;
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      Uint16 *src= *pp;
      while (sy-- > 0) {
	 memcpy(dp, src, sx * sizeof(*src));
	 dp += disp_my;
	 src += sx;
      }
      *pp= src;
   }
}

//
//	Draw a console line on the display at the given coordinates,
//	with the given maximum length.  Any items (characters or
//	graphic areas) overhanging the end-boundary are replaced with
//	a space.  Any spare space at the end is filled with
//	colour[col].
//
//	Assumes valid data.
//

static void
draw_line(int xx, int yy, int sx, char *p, int col) {
   int ci;	// Colour index
   int oy;	// Vertical offset
   int wid;	// Width of chunk

   while (sx > 0) {
      switch (*p++) {
       case 'X':		// Text
	  ci= rdN(&p);
	  oy= rdSN(&p);
	  while (*p++) ;	// Skip link-text
	  while (ch= *p++) {
	     // Everything else assumes UTF-8 encoding, but here we only handle ASCII
	     if (sx < font_sx) {
		clear_rect(xx, yy, sx, font_sy, colour[ci]);
		return;
	     }
	     if (oy) drawchar_offset(font + 4 + (ch - font[2]) * font_sy, xx, yy, 
				     font_sx, font_sy, ci, oy);
	     else drawchar(font + 4 + (ch - font[2]) * font_sy, xx, yy, 
			   font_sx, font_sy, ci);
	     xx += font_sx;
	     sx -= font_sx;
	  }
	  break;
       case 'S':		// White space
	  ci= rdN(&p);
	  wid= rdN(&p);
	  while (*p++) ;	// Skip link-text
	  if (sx <= wid) wid= sx;
	  clear_rect(xx, yy, wid, font_sy, colour[ci]);
	  xx += wid;
	  sx -= wid;
	  break;
       case 'G':		// Graphic data
	  ci= rdN(&p);
	  wid= rdN(&p);
	  while (*p++) ;	// Skip link-text
	  if (wid > sx) {
	     clear_rect(xx, yy, sx, font_sy, colour[ci]);
	     return;
	  }
	  drawgraphic(&p, xx, yy, wid, font_sy);
	  xx += wid;
	  sx -= wid;
	  break;
       case 'T':		// Transparent area
	  wid= rdN(&p);
	  xx += wid;
	  sx -= wid;
	  break;
       case 'E':		// End marker
	  clear_rect(xx, yy, sx, font_sy, colour[col]);
	  xx += sx;
	  sx= 0;
	  break;
      }
   }
}
	  
#endif
	  
// END //

