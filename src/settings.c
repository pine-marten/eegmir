//
//	Settings handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	This provides the standard settings area display for any
//	screen that wants it.  Each setting is represented by a
//	floating-point number in an array.  There are 26 settings
//	maximum, one for each letter key.  The way the settings are
//	handled are described by a string passed to the setup routine.
//	Each entry takes the form:
//
//	  [a-z] <init> <min> <max> {LF|LI|EF|EI} <step> 'Description';
//
//	For example:
//
//	  b 1 0 1e10 EF 8 'Brightness';
//
//	LF/LI/EF/EI means "linear floating-point", "linear integer",
//	"exponential floating-point", "exponential integer".  Linear
//	versus exponential refers the the behaviour whilst stepping
//	with the '+'/'-' keys.  For linear, <step> is the value to
//	add/subtract for each step.  For exponential, <step> is the
//	number of steps per power-of-two increase.
//

#ifdef HEADER

typedef struct SetDetail SetDetail;
struct SetDetail {
   double init, min, max; // Initial/min/max values
   int f_exp;		// Exponential? 0: linear, 1: exponential
   int f_int;		// Integer? 0: fp, 1: integer
   double step;		// Step for linear
   double mul;		// Multiplier for exponential
   char *desc;		// Description (strdup'd)
   int num;		// Display ordering number 0..(ss->num-1)
   int ii;		// Index corresponding to letter a-z (0..25)
   int ox, oy;		// Place to draw setting with current arrangement
   double pre[10];	// Preset values
   int n_pre;		// Number of preset values
};   

typedef struct Settings Settings;
struct Settings {
   double val[26];
   SetDetail *set[26];
   int desc_min;	// Length required for descriptions
   int num;		// Number of settings we actually have
   short *font;		// Current font
   int sx, sy;		// Area to draw to
   int ox, oy;		// Offset of area within screen
   int desc_len;	// Space actually allocated for the description (>= desc_min)	
   int entry_sx;	// Pixel-width consumed by each entry
   int curr;		// Currently active letter index (0..25), or -1
};

#else

#ifndef NO_ALL_H
#include "all.h"
#endif

//
//	Allocate and initialise a Settings structure based on a
//	spec-string containing a list of settings entries as described
//	above.
//

Settings *
set_new(char *spec) {
   Settings *ss= ALLOC(Settings);
   Parse parse_tmp;
   Parse *pp= &parse_tmp;
   SetDetail *sd;
   int ii;
   int len;

   pp->txt= spec;
   pp->pos= spec;
   ss->num= 0;
   ss->curr= -1;
   
   while (1) {
      char *p0, *p1, *p2, *p3;
      sd= ALLOC(SetDetail);
      if (!parse(pp, "%t %f %f %f %t %f %Q;",
		 &p0, &p1, &sd->init, &sd->min, &sd->max,
		 &p2, &p3, &sd->step, &sd->desc)) {
	 free(sd);
	 break;
      }

      sd->num= ss->num++;
      if (p1-p0 != 1 || !islower(*p0) ||
	  p3-p2 != 2 || !(p2[0] == 'E' || p2[0] == 'L') ||
	  !(p2[1] == 'F' || p2[1] == 'I'))
	 error("Internal error: Bad settings description passed to set_new():\n  %s", 
	       pp->rew);

      ii= p0[0] - 'a';
      ss->set[ii]= sd;
      ss->val[ii]= sd->init;
      sd->ii= ii;
      sd->f_exp= (p2[0] == 'E');
      sd->f_int= (p2[1] == 'I');

      if (sd->f_exp) sd->mul= pow(2, 1/sd->step);
      len= strlen(sd->desc);
      if (len > ss->desc_min) ss->desc_min= len;
   }

   if (!parseEOF(pp))
      error("Internal error: Unrecognised stuff at end of settings \n"
	    "description passed to set_new():\n  %s", 
	    pp->pos);
   
   return ss;
}

//
//	Delete a Settings structure
//

void 
set_delete(Settings *ss) {
   error("set_delete NYI");
}

//
//	Load up some presets.  This is an ASCII description with zero
//	or more specs as follows:
//
//	  <letter> <val> <val> ...;
//
//	There may be 0 to 10 <val> entries following the letter, which
//	give the presets from key '1' up to key '0'.  Any presets
//	given here override any presets given previously.
//

void 
set_load_presets(Settings *ss, char *txt) {
   Parse parse_tmp;
   Parse *pp= &parse_tmp;
   char *p0, *p1;
   int ii;
   SetDetail *sd;
   
   pp->txt= pp->pos= txt;
   
   while (parse(pp, "%t", &p0, &p1)) {
      if (p1-p0 != 1 || !islower(p0[0]))
	 error("Bad preset data:\n  %s", pp->rew);
      ii= p0[0] - 'a';
      sd= ss->set[ii];
      if (!sd) error("Preset data for non-existant setting:\n  %s", pp->rew);
      
      sd->n_pre= 0;
      while (!parse(pp, ";")) {
	 if (sd->n_pre == 10 ||
	     !parse(pp, "%f", &sd->pre[sd->n_pre]))
	    error("Bad preset data:\n  %s", pp->rew);
	 sd->n_pre++;
      }
   }

   if (!parseEOF(pp))
      error("Bad preset data:\n  %s", pp->pos);
}

//
//	Rearrange the settings display to fit within the given pixel
//	width using the given font, and return the number of pixel
//	lines required.
//

int 
set_rearrange(Settings *ss, short *font, int sx) {
   int len, n_col, n_lin, wid;
   int a;
   SetDetail *sd;
   
   ss->sx= sx;
   ss->font= font;
   wid= sx / font[0];			// Width in characters
   len= ss->desc_min + 1 + 2 + 1 + 6 + 1;	// <desc><gap>XN<gap>######<gap>
   n_col= (wid + 1) / len;		// +1 because we don't need the final gap
   if (ss->num < n_col) n_col= ss->num;
   n_lin= (ss->num-1)/n_col + 1;
   ss->sy= n_lin * font[1];
   len= (wid + 1) / n_col - 1;		// Actual space for each setting, excluding final gap
   ss->desc_len= len - 10;
   ss->entry_sx= len * font[0];
   
   for (a= 0; a<26; a++) if ((sd= ss->set[a])) {
      sd->ox= (len+1) * (sd->num / n_lin) * font[0];
      sd->oy= (sd->num % n_lin) * font[1];
   }

   return ss->sy;
}

//
//	Change the base position of the settings display
//

void 
set_position(Settings *ss, int ox, int oy) {
   ss->ox= ox; 
   ss->oy= oy;
}

//
//	Draw a single setting entry
//

static void 
set_draw_entry(Settings *ss, SetDetail *sd) {
   char buf[256];
   char valstr[32];
   int a;
   int key= '?';
   double val= ss->val[sd->ii];
   for (a= 0; a<sd->n_pre; a++) {
      double pre= sd->pre[a];
      if (pre == val || 
	  fabs(pre-val) < 0.0005 * (fabs(pre) + fabs(val))) {
	 key= "1234567890"[a];
	 break;
      }
   }

   if (sd->f_int) 
      sprintf(valstr, "%g      ", floor(0.5 + val));
   else 
      sprintf(valstr, "%.6f      ", val);
   valstr[6]= 0;
   
   sprintf(buf, "%c%c%c\x8c \x90%s\x8c %-*s",
	   (ss->curr == sd->ii) ? '\x92' : '\x8e',
	   'a' + sd->ii, key, valstr,
	   ss->desc_len, sd->desc);
   
   drawtext(ss->font, ss->ox + sd->ox, ss->oy + sd->oy, buf);
   update(ss->ox + sd->ox, ss->oy + sd->oy, 
	  ss->entry_sx, ss->font[1]);
}

//
//	Draw the settings display in full
//

void 
set_draw(Settings *ss) {
   int a;
   SetDetail *sd;

   suspend_update++;
   clear_rect(ss->ox, ss->oy, ss->sx, ss->sy, colour[0]);

   for (a= 0; a<26; a++) 
      if ((sd= ss->set[a])) 
	 set_draw_entry(ss, sd);

   suspend_update--;
   update(ss->ox, ss->oy, ss->sx, ss->sy);
}

//
//	See if the settings display can handle this event.  It handles
//	unshifted keypresses for [-+=a-z0-9].  There are three
//	possibilities.  First the event is not handled, and 0 is
//	returned.  Second, the event is handled and 1 is returned to
//	indicate that nothing else should attempt to process the
//	event.  Third, the event is consumed, but the event structure
//	is changed to an event that indicates that a setting has
//	changed value, and 0 is returned.  So, the return value
//	indicates whether the event still needs to be processed.
//
//	The special event returned has a ->typ == 'SET', ->sym set to
//	the settings letter that has changed, and ->val is set to the
//	new value.
//
//	Returns: 1: event consumed, 0: event still needs to be
//	processed.
//

int 
set_event(Settings *ss, Event *ev) {
   SetDetail *sd;

   // We are only interested in unshifted keypresses
   if (!(ev->typ == 'KD' || ev->typ == 'KR') ||
       0 != (ev->mod & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_META)))
      return 0;

   // Handle presets
   if (ss->curr >= 0 && ev->sym >= '0' && ev->sym <= '9') {
      int pre= (ev->sym - '0' + 9) % 10;	// '0' counts as 9, '1'..'9' are 0..8
      sd= ss->set[ss->curr];
      if (pre >= sd->n_pre) {
	 status("There is no '%c%c' preset", sd->ii + 'a', ev->sym);
	 return 1;
      }
      ss->val[sd->ii]= sd->pre[pre];
      set_draw_entry(ss, sd);
      ev->typ= 'SET';
      ev->sym= 'a' + sd->ii;
      ev->val= sd->pre[pre];
      return 0;		// Indicate that the caller should handle the event
   }

   // Handle inc/dec
   if (ss->curr >= 0 && (ev->sym == '-' || ev->sym == '+' || ev->sym == '=')) {
      double val, val0;
      int inc= ev->sym != '-';

      sd= ss->set[ss->curr];
      val= val0= ss->val[sd->ii];

      if (sd->f_exp) 
	 if (inc) val *= sd->mul; else val /= sd->mul;
      else 
	 if (inc) val += sd->step; else val -= sd->step;

      if (sd->f_int) {
	 val= floor(0.5 + val);
	 if (val == val0) val= val0 + (inc ? 1 : -1);
      }
      
      if (val < sd->min || val > sd->max) {
	 warn("min/val/max: %g %g %g", sd->min, val, sd->max);
	 status("Reached %simum value for setting", inc ? "max" : "min");
	 return 1;
      }

      ss->val[sd->ii]= val;
      set_draw_entry(ss, sd);
      ev->typ= 'SET';
      ev->sym= 'a' + sd->ii;
      ev->val= val;
      return 0;		// Indicate that the caller should handle the event
   }

   // Handle setting section change
   if (ev->sym >= 'a' && ev->sym <= 'z') {
      int ii= ev->sym - 'a';
      if (!ss->set[ii]) {
	 status("There is no '%c' setting type", ev->sym);
	 return 1;
      }
      if (ss->curr >= 0) {
	 int prev= ss->curr;
	 ss->curr= -1;
	 set_draw_entry(ss, ss->set[prev]);
      }
      ss->curr= ii;
      set_draw_entry(ss, ss->set[ii]);
      return 1;
   }

   // We couldn't handle it after all
   return 0;
}

#endif

// END //


