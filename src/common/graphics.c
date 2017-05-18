
//	Graphics handling using libSDL
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	We handle only 16bpp and 32bpp displays directly, and let SDL
//	emulate the other modes.  Much of this is "fast enough" but
//	not really optimised, except for the most critical bits.
//

#ifndef NO_ALL_H
#include "../all.h"
#endif

//
//      Array of predefined colours in 0xRRGGBB form, terminated with
//      a -999 value.  Put any fixed colours required in here, and
//      they will automatically be converted to pixel values in
//      colour[] when SDL starts up.
//

extern int colour_data[];

//
//	Globals
//

int suspend_update= 0;		// If non-zero, update() calls will have no effect

//
//	Initialise SDL for graphics.  The initial window/screen size
//	is passed as arguments, and all the disp_* and colour[]
//	globals are set up.  The 'bpp' argument may be 0 for a normal
//	window using whatever bpp is current, or 16 or 32 to run the
//	app in full-screen mode.
//

void 
graphics_init(int sx, int sy, int bpp) {
   SDL_VideoInfo const *vid;
   int full= bpp != 0;
   int len, a;

   if (!full) {
      vid= SDL_GetVideoInfo();
      bpp= vid->vfmt->BitsPerPixel;
      
      // We are only going to handle 16-bit and 32-bit modes in this
      // code.  For 8-bit modes, we ask for 16bpp, and for 24 bit we
      // ask for 32bpp.  SDL will convert the image data to the real
      // format for us.
      if (bpp <= 16) bpp= 16; else bpp= 32;
   }
   
   disp_sx= sx;
   disp_sy= sy;
   if (!(disp= SDL_SetVideoMode(disp_sx, disp_sy, bpp, SDL_SWSURFACE |
				(full ? SDL_FULLSCREEN : SDL_RESIZABLE))))
      errorSDL("Couldn't set video mode");
   
   if (SDL_MUSTLOCK(disp)) 
      error("OOPS: According to the docs this shouldn't happen -- "
	    "I'm supposed to lock this SDL_SWSURFACE display");
   
   if (disp->format->BytesPerPixel == 2) {
      disp_pix16= disp->pixels; 
      disp_pix32= 0;
      if (disp->pitch & 1)
	 error("Display pitch isn't a multiple of 2: %d", disp->pitch);
      disp_my= disp->pitch >> 1;
   } else if (disp->format->BytesPerPixel == 4) {
      disp_pix16= 0;
      disp_pix32= disp->pixels; 
      if (disp->pitch & 3)
	 error("Display pitch isn't a multiple of 4: %d", disp->pitch);
      disp_my= disp->pitch >> 2;
   } else 
      error("Unexpected error -- SDL didn't give me the number of bpp I asked for");
   
   disp_rl= disp->format->Rloss;
   disp_gl= disp->format->Gloss;
   disp_bl= disp->format->Bloss;
   disp_rs= disp->format->Rshift;
   disp_gs= disp->format->Gshift;
   disp_bs= disp->format->Bshift;
   disp_am= disp->format->Amask;

   // Turn 16-bit 5-6-5 into 5-5-1-5, so we don't get that alternate
   // red-green effect in subtle shades of gray
   while (disp_gl < disp_rl) disp_gl++, disp_gs++;

   // Setup the static colours I'll be needing
   for (len= 0; len < 1024 && colour_data[len] != -999; len++) ;
   if (len == 1024) error("Bad colour_data array; no -999 end-marker");

   colour= ALLOC_ARR(len, int);
   for (a= 0; a<len; a++) {
      int cc= colour_data[a];
      colour[a]= (cc == -1) ? -1 : map_rgb(cc);
   }

   // Initialise pure hues and colour-intensity table
   init_pure_hues();
   init_cint_table();

   // Clear the screen
   clear_rect(0, 0, sx, sy, colour[0]);
   update(0, 0, sx, sy);

   // All done
}

//
//	Map an 0xRRGGBB colour into whatever is required for the
//	current video mode.
//

int 
map_rgb(int col) {
   int r= 255 & (col >> 16);
   int g= 255 & (col >> 8);
   int b= 255 & col;

   return disp_am | 
      ((r >> disp_rl) << disp_rs) |
      ((g >> disp_gl) << disp_gs) |
      ((b >> disp_bl) << disp_bs);
}

//
//	Generate a colour at the given percentage from colour 'col0'
//	to 'col1', prop==0 giving col0, prop==100 giving col1.
//	Colours are internal-format, as returned by map_rgb().
//

int 
colour_mix(int prop, int col0, int col1) {
   int r0, g0, b0, r1, g1, b1;

   r0= ((col0 >> disp_rs) << disp_rl) & 255;
   g0= ((col0 >> disp_gs) << disp_gl) & 255;
   b0= ((col0 >> disp_bs) << disp_bl) & 255;
   r1= ((col1 >> disp_rs) << disp_rl) & 255;
   g1= ((col1 >> disp_gs) << disp_gl) & 255;
   b1= ((col1 >> disp_bs) << disp_bl) & 255;
   r0 += (r1-r0) * prop / 100;
   g0 += (g1-g0) * prop / 100;
   b0 += (b1-b0) * prop / 100;

   return disp_am | 
      ((r0 >> disp_rl) << disp_rs) |
      ((g0 >> disp_gl) << disp_gs) |
      ((b0 >> disp_bl) << disp_bs);
}


//
//	Update a rectangle directly to the screen
//

void 
update(int xx, int yy, int sx, int sy) {
   if (!suspend_update) 
      SDL_UpdateRect(disp, xx, yy, sx, sy);
}


//
//      Force an update through even if updates are suspended -- only
//      used for status line.
//

void 
update_force(int xx, int yy, int sx, int sy) {
   SDL_UpdateRect(disp, xx, yy, sx, sy);
}


//
//	Update the whole screen, releasing any 'suspend'
//

void 
update_all() {
   suspend_update= 0;
   update(0, 0, disp_sx, disp_sy);
}

//
//	Show/hide mouse cursor
//

void 
mouse_pointer(int on) {
   SDL_ShowCursor(on ? SDL_ENABLE : SDL_DISABLE);
}

//
//	Clear a rectangle to the given colour
//

void 
clear_rect(int xx, int yy, int sx, int sy, int val) {
   int a;

   if (xx < 0) { sx += xx; xx= 0; }
   if (yy < 0) { sy += yy; yy= 0; }
   if (xx + sx > disp_sx) sx= disp_sx - xx;
   if (yy + sy > disp_sy) sy= disp_sy - yy;
   if (sx <= 0 || sy <= 0) return;

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (sy-- > 0) {
	 for (a= sx; a>0; a--) *dp++= val;
	 dp += disp_my - sx;
      }
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (sy-- > 0) {
	 for (a= sx; a>0; a--) *dp++= val;
	 dp += disp_my - sx;
      }
   }
}      

//
//      Apply a colour transparently to the given rectangle.  An
//      opacity of 100 is opaque, 0 means fully transparent (which
//      would be a pointless operation in this case).
//

void 
alpha_rect(int xx, int yy, int sx, int sy, int val, int opac) {
   int a;
   int r0, r1, g0, g1, b0, b1;

   if (xx < 0) { sx += xx; xx= 0; }
   if (yy < 0) { sy += yy; yy= 0; }
   if (xx + sx > disp_sx) sx= disp_sx - xx;
   if (yy + sy > disp_sy) sy= disp_sy - yy;
   if (sx <= 0 || sy <= 0) return;

   r0= ((val >> disp_rs) << disp_rl) & 255;
   g0= ((val >> disp_gs) << disp_gl) & 255;
   b0= ((val >> disp_bs) << disp_bl) & 255;

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (sy-- > 0) {
         for (a= sx; a>0; a--) {
            r1= ((dp[0] >> disp_rs) << disp_rl) & 255;
            g1= ((dp[0] >> disp_gs) << disp_gl) & 255;
            b1= ((dp[0] >> disp_bs) << disp_bl) & 255;
            r1 += (r0-r1) * opac / 100;
            g1 += (g0-g1) * opac / 100;
            b1 += (b0-b1) * opac / 100;
            *dp++= disp_am | 
               ((r1 >> disp_rl) << disp_rs) |
               ((g1 >> disp_gl) << disp_gs) |
               ((b1 >> disp_bl) << disp_bs);
         }
         dp += disp_my - sx;
      }
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (sy-- > 0) {
         for (a= sx; a>0; a--) {
            r1= ((dp[0] >> disp_rs) << disp_rl) & 255;
            g1= ((dp[0] >> disp_gs) << disp_gl) & 255;
            b1= ((dp[0] >> disp_bs) << disp_bl) & 255;
            r1 += (r0-r1) * opac / 100;
            g1 += (g0-g1) * opac / 100;
            b1 += (b0-b1) * opac / 100;
            *dp++= disp_am | 
               ((r1 >> disp_rl) << disp_rs) |
               ((g1 >> disp_gl) << disp_gs) |
               ((b1 >> disp_bl) << disp_bs);
         }
         dp += disp_my - sx;
      }
   }
}

//
//	Scroll a region.  The uncovered area will contain garbage --
//	either the previous contents, or wrapped-around contents.
//	(ox,oy) is the movement relative to the current position.
//

void 
scroll(int xx, int yy, int sx, int sy, int ox, int oy) {
   int sxx, syy;	// Source (relative to xx,yy)
   int dxx, dyy;	// Destination (relative to xx,yy)
   int cx, cy;		// Counts -- i.e. size of actual region to copy
   char *src, *dst;
   int llen, linc;	// Line length, line increment (bytes)

   if (xx < 0) { sx += xx; xx= 0; }
   if (yy < 0) { sy += yy; yy= 0; }
   if (xx + sx > disp_sx) sx= disp_sx - xx;
   if (yy + sy > disp_sy) sy= disp_sy - yy;
   if (sx <= 0 || sy <= 0) return;

   sxx= ox > 0 ? 0 : -ox;	// Source
   syy= oy > 0 ? 0 : -oy;
   dxx= ox > 0 ? ox : 0;	// Destination
   dyy= oy > 0 ? oy : 0;
   cx= sx - (sxx>dxx?sxx:dxx);
   cy= sy - (syy>dyy?syy:dyy);

   if (cx <= 0 || cy <= 0) return;

   if (disp_pix32) {
      src= (char*)(disp_pix32 + (xx+sxx) + (yy+syy) * disp_my);
      dst= (char*)(disp_pix32 + (xx+dxx) + (yy+dyy) * disp_my);
      llen= cx * sizeof(Uint32);
      linc= disp_my * sizeof(Uint32);
   } else if (disp_pix16) {
      src= (char*)(disp_pix16 + (xx+sxx) + (yy+syy) * disp_my);
      dst= (char*)(disp_pix16 + (xx+dxx) + (yy+dyy) * disp_my);
      llen= cx * sizeof(Uint16);
      linc= disp_my * sizeof(Uint16);
   }

   if (sx == disp_my && cx >= (sx / 2)) {
      // We can do this all in one big memmove; if there is a sideways
      // movement too, then we are doing some wasted copies.  However,
      // this may well be quicker because it is part of some tight
      // loop within memmove.  At the moment I'm effectively saying
      // that if more than 50% of the copies are wasted, we would be
      // better doing it line by line.  Really it would be better to
      // write assembler for each platform.  Later maybe.
      memmove(dst, src, linc * (cy-1) + llen);
   } else {
      // Otherwise, we need to do it line by line
      int cy= sy - (syy>dyy?syy:dyy);

      if (oy != 0) {
	 // We can use memcpy
	 if (src < dst) {
	    dst += linc * (cy-1);
	    src += linc * (cy-1);
	    linc= -linc;
	 }
	 while (cy-- > 0) {
	    memcpy(dst, src, llen);
	    dst += linc;
	    src += linc;
	 }
      } else {
	 // We have to use memmove (slower!)
	 while (cy-- > 0) {
	    memmove(dst, src, llen);
	    dst += linc;
	    src += linc;
	 }
      }
   }
}	 

//
//	Map all the pixels of one given colour to another colour
//	within the given rectangle (map col0->col1).
//	 

void 
mapcol(int xx, int yy, int sx, int sy, int col0, int col1) {
   int a;

   if (xx < 0) { sx += xx; xx= 0; }
   if (yy < 0) { sy += yy; yy= 0; }
   if (xx + sx > disp_sx) sx= disp_sx - xx;
   if (yy + sy > disp_sy) sy= disp_sy - yy;
   if (sx <= 0 || sy <= 0) return;

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (sy-- > 0) {
	 for (a= sx; a>0; a--, dp++)
	    if (*dp == col0) *dp= col1;
	 dp += disp_my - sx;
      }
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (sy-- > 0) {
	 for (a= sx; a>0; a--, dp++) 
	    if (*dp == col0) *dp= col1;
	 dp += disp_my - sx;
      }
   }
}


//
//	Vertical line
//

void 
vline(int xx, int yy, int sy, int val) {
   if (xx < 0) return;
   if (yy < 0) { sy += yy; yy= 0; }
   if (yy + sy > disp_sy) sy= disp_sy - yy;
   if (sy <= 0) return;

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (sy-- > 0) { *dp= val; dp += disp_my; }
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (sy-- > 0) { *dp= val; dp += disp_my; }
   }
}


//
//      Horizontal line
//

void 
hline(int xx, int yy, int sx, int val) {
   if (xx < 0) { sx += xx; xx= 0; }
   if (yy < 0) return;
   if (xx + sx > disp_sx) sx= disp_sx - xx;
   if (sx <= 0) return;

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (sx-- > 0) *dp++= val;
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (sx-- > 0) *dp++= val;
   }
}


//
//	Get the colour value of a point.  This is only useful for
//	replotting or comparing to colour[] values, as it might be in
//	any of several packed formats.
//

int 
get_point(int xx, int yy) {
   if (xx < 0 || 
       yy < 0 ||
       xx >= disp_sx ||
       yy >= disp_sy) return 0;

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      return *dp;
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      return *dp;
   }
   return 0;
}   
   

//
//	Plot a single point
//

void 
plot(int xx, int yy, int val) {
   if (xx < 0 || 
       yy < 0 ||
       xx >= disp_sx ||
       yy >= disp_sy) return;

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      *dp= val;
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      *dp= val;
   }
}   
   

//
//	Draw text on the current display (as specified in global
//	variables 'disp_*').  'font' is one of: font6x8, font8x16, or
//	font10x20 currently.  If the text will run off the right end
//	of the screen, it is truncated.  If the text is in any other
//	way off the edge of the screen, nothing is written at all.
//
//	The string can only use ASCII characters 32-127, and is
//	displayed as white on transparent by default.  However, a
//	character >= 128 allows a different colour-pair to be
//	selected.  The colour-pair is made up of colour[ch-128] for
//	background and colour[ch-128+1] for foreground.
//
//	A character in the range 1-31 has a special effect -- it
//	outputs an endless number of spaces (i.e. until the right edge
//	of the screen is reached).  That means a '\n' blanks to EOL.
//
//	Note the code for 16-bit versus 32-bit is identical except for
//	the type of 'dp'.
//

void 
drawtext(short *font, int xx, int yy, char *str) {
   int fg= colour[1];
   int bg= -1;
   short *fp;
   int val;
   int sx, sy;
   int ch0, n_ch;
   int cnt;
   int ch, a;

   sx= *font++;
   sy= *font++;
   ch0= *font++;
   n_ch= *font++;

   if (xx < 0 || yy < 0 || yy + sy > disp_sy) return;
   cnt= (disp_sx - xx) / sx;

   // 32-bit display
   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      
      while (cnt > 0 && (ch= (255 & *str++))) {
	 if (ch >= 128) {
	    bg= colour[ch-128];
	    fg= colour[ch-128+1];
	    continue;
	 }
	 ch -= ch0; cnt--;
	 if (ch < 0) { ch= 0; str--; }
	 if (ch >= n_ch) ch= n_ch-1;
	 
	 fp= font + sy * ch;
	 if (bg != -1)
	    for (a= sy; a>0; a--) {
	       for (val= *fp++; val != 1; val >>= 1)
		  *dp++= (val&1) ? fg : bg;
	       dp += disp_my - sx;
	    }
	 else 
	    for (a= sy; a>0; a--) {
	       for (val= *fp++; val != 1; val >>= 1, dp++) 
		  if (val&1) *dp= fg;
	       dp += disp_my - sx;
	    }
	 
	 dp += sx - disp_my * sy;
      }
   }

   // 16-bit display
   if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      
      while (cnt > 0 && (ch= (255 & *str++))) {
	 if (ch >= 128) {
	    bg= colour[ch-128];
	    fg= colour[ch-128+1];
	    continue;
	 }
	 ch -= ch0; cnt--;
	 if (ch < 0) { ch= 0; str--; }
	 if (ch >= n_ch) ch= n_ch-1;
	 
	 fp= font + sy * ch;
	 if (bg != -1)
	    for (a= sy; a>0; a--) {
	       for (val= *fp++; val != 1; val >>= 1)
		  *dp++= (val&1) ? fg : bg;
	       dp += disp_my - sx;
	    }
	 else 
	    for (a= sy; a>0; a--) {
	       for (val= *fp++; val != 1; val >>= 1, dp++) 
		  if (val&1) *dp= fg;
	       dp += disp_my - sx;
	    }
	 
	 dp += sx - disp_my * sy;
      }
   }
}

//
//	As drawtext() except the text is displayed double-size in both
//	directions.
//

void 
drawtext4(short *font, int xx, int yy, char *str) {
   int fg= colour[1];
   int bg= -1;
   short *fp;
   int val;
   int sx, sy;
   int ch0, n_ch;
   int cnt;
   int ch, a;

   sx= *font++;
   sy= *font++;
   ch0= *font++;
   n_ch= *font++;

   if (xx < 0 || yy < 0 || yy + sy*2 > disp_sy) return;
   cnt= (disp_sx - xx) / sx / 2;

   // 32-bit display
   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      
      while (cnt > 0 && (ch= (255 & *str++))) {
	 if (ch >= 128) {
	    bg= colour[ch-128];
	    fg= colour[ch-128+1];
	    continue;
	 }
	 ch -= ch0; cnt--;
	 if (ch < 0) { ch= 0; str--; }
	 if (ch >= n_ch) ch= n_ch-1;
	 
	 fp= font + sy * ch;
	 if (bg != -1)
	    for (a= sy; a>0; a--) {
	       for (val= *fp; val != 1; val >>= 1, dp += 2) 
		  dp[0]= dp[1]= dp[disp_my]= dp[disp_my+1]= (val&1) ? fg : bg;
	       dp += disp_my*2 - sx*2;
	    }
	 else 
	    for (a= sy; a>0; a--) {
	       for (val= *fp++; val != 1; val >>= 1, dp++) 
		  if (val&1) dp[0]= dp[1]= dp[disp_my]= dp[disp_my+1]= fg;
	       dp += disp_my*2 - sx*2;
	    }
	 
	 dp += sx*2 - disp_my * sy * 2;
      }
   }

   // 16-bit display
   if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      
      while (cnt > 0 && (ch= (255 & *str++))) {
	 if (ch >= 128) {
	    bg= colour[ch-128];
	    fg= colour[ch-128+1];
	    continue;
	 }
	 ch -= ch0; cnt--;
	 if (ch < 0) { ch= 0; str--; }
	 if (ch >= n_ch) ch= n_ch-1;
	 
	 fp= font + sy * ch;
	 if (bg != -1)
	    for (a= sy; a>0; a--) {
	       for (val= *fp; val != 1; val >>= 1, dp += 2) 
		  dp[0]= dp[1]= dp[disp_my]= dp[disp_my+1]= (val&1) ? fg : bg;
	       dp += disp_my*2 - sx*2;
	    }
	 else 
	    for (a= sy; a>0; a--) {
	       for (val= *fp++; val != 1; val >>= 1, dp++) 
		  if (val&1) dp[0]= dp[1]= dp[disp_my]= dp[disp_my+1]= fg;
	       dp += disp_my*2 - sx*2;
	    }
	 
	 dp += sx*2 - disp_my * sy * 2;
      }
   }
}

//
//	Find the drawable length of a string with embedded
//	colour-change characters.
//

int 
colstr_len(char *str) {
   int len= 0;
   int ch;
   while ((ch= *str++)) {
      if (!(ch & 128)) len++;
   }
   return len;
}

//
//      Generate a pure hues (red,yellow,green,cyan,blue,magenta) in
//      various intensities.
//
//	For hue 'hue' (0-6:red,yellow,green,cyan,blue,magenta,red),
//	and colour component 'rgb' (0-2:red,green,blue), and intensity
//	level 'lev' (0-255), pure_hue_dat[hue][rgb][lev] gives the RGB
//	component level required.  (hue==6 is added as a copy of
//	hue==0 to simplify some code later on).
//

int pure_hue_src[6][4]= {
   { 1, 0, 0, 30 * 255 / 100 },
   { 1, 1, 0, 89 * 255 / 100 },
   { 0, 1, 0, 59 * 255 / 100 },
   { 0, 1, 1, 70 * 255 / 100 },
   { 0, 0, 1, 11 * 255 / 100 },
   { 1, 0, 1, 41 * 255 / 100 }
};

Uint8 pure_hue_mem[6 * 2 * 256];
Uint8 *pure_hue_data[7][3];

void 
init_pure_hues() {
   Uint8 *dat= pure_hue_mem;
   Uint8 *p, *q;
   int a, b;
   for (a= 0; a<6; a++) {
      p= dat; dat += 256;
      q= dat; dat += 256;
      for (b= 0; b<3; b++)
	 pure_hue_data[a][b]= pure_hue_src[a][b] ? q : p;
      for (b= 0; b<256; b++) {
	 int sat= pure_hue_src[a][3];
	 if (b < sat) {
	    *p++= 0;
	    *q++= b * 255 / sat;
	 } else {
	    *p++= (b-sat) * 255 / (255-sat);
	    *q++= 255;
	 }
      }
   }

   // First hue repeated -- saves extra code
   pure_hue_data[6][0]= pure_hue_data[0][0];
   pure_hue_data[6][1]= pure_hue_data[0][1];
   pure_hue_data[6][2]= pure_hue_data[0][2];
}      

//
//	Plot a point using the given intensity and hue.  The hue is
//	taken modulo 1, i.e. 0-1 goes around the full colour circle,
//	same 1-2, etc.  Intensities >= 1 always give white.
//

void 
plot_hue(int xx, int yy, int sy, double ii, double hh) {
   double tmp;
   int hue, rat;
   int v0, v1, val;
   int r,g,b;
   int mag= (int)(ii * 255.0);

   if (mag < 0) mag= 0;
   if (mag > 255) mag= 255;
   rat= (int)floor(256 * modf(hh * 6.0, &tmp));
   hue= (int)tmp;

   if (hh < 0) { rat += 256.0; hue--; }
   hue %= 6;
   if (hue < 0) hue += 6;

   v0= pure_hue_data[hue][0][mag];
   v1= pure_hue_data[hue+1][0][mag];
   r= v0 + (((v1-v0) * rat) >> 8);

   v0= pure_hue_data[hue][1][mag];
   v1= pure_hue_data[hue+1][1][mag];
   g= v0 + (((v1-v0) * rat) >> 8);

   v0= pure_hue_data[hue][2][mag];
   v1= pure_hue_data[hue+1][2][mag];
   b= v0 + (((v1-v0) * rat) >> 8);

   val= disp_am | 
      ((r >> disp_rl) << disp_rs) |
      ((g >> disp_gl) << disp_gs) |
      ((b >> disp_bl) << disp_bs);
   
   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (sy-- > 0) { *dp= val; dp += disp_my; }
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (sy-- > 0) { *dp= val; dp += disp_my; }
   }
}

//
//	Prepare the colour intensity table for plotting using
//	plot_cint().  Note that first column gives intensity %age
//	mapped to 0-1024, and next three give RGB components.  Also
//	note that (30R+59G+11B)/255 gives the intensity %age.
//

static int cint_src[]= {
   0 * 1024/100, 	0, 0, 0,		// Black
   8 * 1024/100, 	0, 0, 192,		// Blue
   20 * 1024/100,	192, 0, 192,		// Magenta
   26 * 1024/100, 	224, 0, 0,		// Red
   45 * 1024/100,	224, 192, 0,		// Yellow
   59 * 1024/100, 	0, 255, 0,		// Green
   80 * 1024/100, 	0, 255, 255,		// Cyan
   100 * 1024/100, 	255, 255, 255,		// White

   //0 * 1024/100, 	0, 0, 0,		// Black
   //5 * 1024/100, 	0, 0, 128,		// Blue
   //15 * 1024/100, 	82, 0, 128,		// Magenta(ish)
   //30 * 1024/100, 	255, 0, 0,		// Red
   //45 * 1024/100, 	255, 65, 0, 		// Yellow(ish)
   //59 * 1024/100, 	0, 255, 0,		// Green
   //70 * 1024/100, 	0, 255, 255,		// Cyan
   //100 * 1024/100, 	255, 255, 255,		// White
};

int cint_table[1024];

void 
init_cint_table() {
   int a;
   int *ip= cint_src;
   int R, G, B;
   int mul1, mul2, div;

   for (a= 0; a<1024; a++) {
      if (a >= ip[4]) ip += 4;	// Move onto next pair
      div= ip[4]-ip[0];
      mul2= a - ip[0];
      mul1= div - mul2;
      R= (ip[1] * mul1 + ip[5] * mul2) / div;
      G= (ip[2] * mul1 + ip[6] * mul2) / div;
      B= (ip[3] * mul1 + ip[7] * mul2) / div;
      cint_table[a]= disp_am | 
	 ((R >> disp_rl) << disp_rs) |
	 ((G >> disp_gl) << disp_gs) |
	 ((B >> disp_bl) << disp_bs);
   }
}      
   

//
//	Plot a point with the given intensity (0->1), using a colour
//	table to map intensity to colours, allowing a wider range of
//	values to be visualised than by using a simple gray scale.
//	Intensities >=1 give white.  (cint == colour intensity)
//

void 
plot_cint(int xx, int yy, int sy, double ii) {
   int vv= (int)(1024.0 * ii);
   int val;

   if (vv >= 1024)
      val= colour[1];
   else if (vv < 0)
      val= colour[0];
   else 
      val= cint_table[vv];

   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (sy-- > 0) { *dp= val; dp += disp_my; }
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (sy-- > 0) { *dp= val; dp += disp_my; }
   }
}      

//
//	Plot a bar of colour-intensity, going from xx+0 to just before
//	xx+sx, at the very most (sx should be -ve for a left-leaning
//	bar).  Height of unit amplitude is 'unit' (unit being 1.0,
//	displayed as white), and the value to display is 'ii'.  The
//	last pixel of the resulting bar carries the accurate
//	colour-intensity for the value 'ii', so that no detail is
//	lost.
//

void 
plot_cint_bar(int xx, int yy, int sx, int sy, int unit, double ii) {
   int inc= (1024 * 256) / unit;
   int curr= 0;
   int target= (int)(ii * (1024 * 256));
   int dx= sx < 0 ? -1 : 1;
   int mx= xx + sx;
   int a;

   while (curr <= target && xx != mx) {
      int vv, val;
      if (curr + inc <= target)
	 vv= (curr + inc/2) >> 8;
      else 
	 vv= target >> 8;
      
      if (vv >= 1024)
	 val= colour[1];
      else if (vv < 0)
	 val= colour[0];
      else 
	 val= cint_table[vv];

      if (disp_pix32) {
	 Uint32 *dp= disp_pix32 + xx + yy * disp_my;
	 for (a= sy; a-- > 0; ) { *dp= val; dp += disp_my; }
      } else if (disp_pix16) {
	 Uint16 *dp= disp_pix16 + xx + yy * disp_my;
	 for (a= sy; a-- > 0; ) { *dp= val; dp += disp_my; }
      }
      
      xx += dx;
      curr += inc;
   }
}


//
//	Plot a point using the given intensity.  A gray-scale is used.
//	If the intensity is >= 1, white is plotted.
//

void 
plot_gray(int xx, int yy, int sy, double ii) {
   int val;
   int v= (int)(255.0 * ii);
   if (v > 255) v= 255;
   
   val= disp_am | 
      ((v >> disp_rl) << disp_rs) |
      ((v >> disp_gl) << disp_gs) |
      ((v >> disp_bl) << disp_bs);
   
   if (disp_pix32) {
      Uint32 *dp= disp_pix32 + xx + yy * disp_my;
      while (sy-- > 0) { *dp= val; dp += disp_my; }
   } else if (disp_pix16) {
      Uint16 *dp= disp_pix16 + xx + yy * disp_my;
      while (sy-- > 0) { *dp= val; dp += disp_my; }
   }
}

// END //


