//
//	EEG Mind Mirror prototype thingy
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

#ifndef NO_ALL_H
#include "all.h"
#endif

//
//	Globals
//

// Display-related
SDL_Surface *disp;	// Display
Uint32 *disp_pix32;	// Pixel data for 32-bit display, else 0
Uint16 *disp_pix16;	// Pixel data for 16-bit display, else 0
int disp_my;            // Display pitch, in pixels
int disp_sx, disp_sy;   // Display size

int disp_rl, disp_rs;	// Red component transformation
int disp_gl, disp_gs;	// Green component transformation
int disp_bl, disp_bs;	// Blue component transformation
int disp_am;		// Alpha mask

int *colour;		// Array of colours mapped to current display: see colour_data

// Main loop
Page *page;		// Currently displayed page
int tick_inc;		// TICK timer interval in ms
int tick_targ;		// TICK timer target time for next tick in ms
Uint32 main_threadid;	// ID of main thread

// Server
int server;		// Server mode? (0 no, 1 yes)
int server_rd;		// Server read position in sample buffer
int server_out= -1;	// Server output FD if active, or -1

// Hacks
double nan_global;	// Used on MSVC because 0.0/0.0 as a constant is not understood

// Pages
Page *p_fn[12];		// Pages associated with function keys F1-F12 (0-11)



//
//	Usage
//

#define NL "\n"

void
usage() {
   error("EEG Mind Mirror prototype/testing app, version " VERSION
	 NL "Copyright (c) 2003 Jim Peters, http://uazu.net/, all rights reserved,"
	 NL "  released under the GNU GPL v2.  See file COPYING."
	 NL "LibSDL code from the SDL project (http://libsdl.org/) is released under"
	 NL "  the GNU LGPL version 2 or later.  See file COPYING.LIB."
	 NL "fidlib filter design code released under the GNU LGPL version 2.1."
	 NL "  See file COPYING.LIB."
	 NL
	 NL "Usage: eegmir [options]"
	 NL
	 NL "Options:"
	 NL "  -F <mode>     Run full-screen with the given mode, <wid>x<hgt>x<bpp>"
	 NL "                <bpp> may be 16 or 32.  For example: 800x600x16"
	 NL "  -W <size>     Run as a window with the given size: <wid>x<hgt>"
	 NL
	 NL "Experimental:"
	 NL "  -S    Run in TCP server mode, faking a minimal 'OpenEEG server' on port"
	 NL "        8336 for just one client, and relaying samples to that client."
	 NL "        The config file should be called \"eegmir-server.cfg\"."
	 );
}


//
//	Main routine
//

int 
main(int ac, char **av) {
   char dmy;
   int sx= 800, sy= 600, bpp= 0;	// Default is 800x600 resizable window
   SDL_Event ev;
   Event event;		// Our event structure
   int xx, yy;
   char keymap[SDLK_LAST];

   // Process arguments
   ac--; av++;
   while (ac > 0 && av[0][0] == '-' && av[0][1]) {
      char ch, *p= *av++ + 1; 
      ac--;
      while ((ch= *p++)) switch (ch) {
       case 'F':
	  if (ac-- < 1) usage();
	  if (3 != sscanf(*av++, "%dx%dx%d %c", &sx, &sy, &bpp, &dmy) ||
	      (bpp != 16 && bpp != 32))
	     error("Bad mode-spec: %s", av[-1]);
	  break;
       case 'W':
	  if (ac-- < 1) usage();
	  if (2 != sscanf(*av++, "%dx%d %c", &sx, &sy, &dmy))
	     error("Bad window size: %s", av[-1]);
	  break;
       case 'S':
	  server= 1;
	  break;
       default:	
	  error("Unknown option '%c'", ch);
      }
   }

   // Server is weird, no GUI, no SDL, no events, no pages.  applog()
   // calls still work though, going to STDERR.
   if (server) {
      applog("=== Welcome to EEGMIR minimal testing OpenEEG server " VERSION " ===");
      load_config("eegmir-server.cfg");
      server_loop();
      return 0;
   }

   // Initialize SDL
   if (0 > SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | 
		    SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE)) 
      errorSDL("Couldn't initialize SDL");
   atexit(SDL_Quit);
   //SDL_EnableUNICODE(1);		// So we get translated keypresses
   SDL_EnableKeyRepeat(200, 40);
   SDL_WM_SetCaption(PROGNAME " " VERSION, PROGNAME);
   main_threadid= SDL_ThreadID();

   // Initialise graphics
   graphics_init(sx, sy, bpp);

   // Setup pages
   status("+");
   p_fn[0]= p_console_init();
   page_switch(p_fn[0]);

   // Now we have got the console page up, we can switch all fatal
   // errors to use our routine. @@@ NYI

   // Load config file; used to be in a separate thread, but this gave
   // problems with audio due to the audio thread dying when it saw
   // its parent go away.
   applog("\x82 Welcome to eegmir Mind Mirror prototype version " VERSION " \x80");
   applog_force_update= 1;
   load_config("eegmir.cfg");
   applog_force_update= 0;

   // Main loop
   memset(keymap, 0, sizeof(keymap));
   event.now= time_now_ms();
   while (1) {
      int tick;

      // Show that we are about to wait
      event.typ= 'PAUS';
      page->event(&event);

      // Wait for an event.  Rather than using SDL_WaitEvent, I've
      // copied the code from that routine and I've patched in my own
      // timer handler.
      tick= 0;
      while (1) {
	 int rv, now;
	 SDL_PumpEvents();
	 now= time_now_ms();
	 //warn("NOW == %d", now);
	 event.now= now;		// All events in this batch get this timestamp
	 if (tick_inc && now - tick_targ >= 0) {
	    tick= 1;
	    tick_targ= now + tick_inc;
	    break;
	 }
	 if (0 > (rv= SDL_PeepEvents(0, 1, SDL_GETEVENT, SDL_ALLEVENTS)))
	    errorSDL("Unexpected error from SDL_PeepEvents");
	 if (rv) break;
	 SDL_Delay(5);		// Wait of 5ms or more according to OS load
      } 

      // Process all outstanding events
      while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_ALLEVENTS)) {
	 switch (ev.type) {
	  case SDL_KEYDOWN:
	     status("");	// Use keypress to clear temporary messages from status line
	     event.sym= ev.key.keysym.sym;
	     event.mod= ev.key.keysym.mod;
	     event.uc= ev.key.keysym.unicode;
	     event.typ= keymap[event.sym] ? 'KR' : 'KD';
	     keymap[event.sym]= 1;
	     // Catch FN keys for switching pages
	     if (event.sym >= SDLK_F1 && event.sym <= SDLK_F12) {
		int fn= event.sym - SDLK_F1;
		if (event.typ == 'KD' && p_fn[fn]) 
		   page_switch(p_fn[fn]);
	     } else if (event.sym == SDLK_ESCAPE) {
		event.typ= 'QUIT';
		page->event(&event);
	     } else {
		page->event(&event);
	     }
	     break;
	  case SDL_KEYUP:
	     event.sym= ev.key.keysym.sym;
	     event.mod= ev.key.keysym.mod;
	     event.uc= ev.key.keysym.unicode;
	     event.typ= 'KU';
	     keymap[event.sym]= 0;
	     page->event(&event);
	     break;
	  case SDL_MOUSEMOTION:
	     xx= ev.motion.x;
	     yy= ev.motion.y;
	     event.dx= ev.motion.xrel;
	     event.dy= ev.motion.yrel;
	     event.typ= 'MM';
	     goto mouse_event;
	  case SDL_MOUSEBUTTONDOWN:
	  case SDL_MOUSEBUTTONUP:
	     if (ev.button.button <= 33) {
		int bit= 1<<(ev.button.button-1);	
		if (ev.type == SDL_MOUSEBUTTONDOWN)
		   event.mbm ^= bit;
		else 
		   event.mbm &= ~bit;
	     }
	     xx= ev.button.x;
	     yy= ev.button.y;
	     event.but= ev.button.button;
	     event.typ= (ev.type == SDL_MOUSEBUTTONDOWN) ? 'MD' : 'MU';
	 mouse_event:
	     {
		Region *rr;
		for (rr= page->reg; rr; rr= rr->nxt) {
		   int in= (xx >= rr->x0 && xx < rr->x1 &&
			    yy >= rr->y0 && yy < rr->y1);
		   if (in) {
		      event.xx= xx - rr->x0;
		      event.yy= yy - rr->y0;
		   }
		   if (in != rr->inside) {
		      rr->inside= in;
		      if (in) {
			 event.typ= 'ENTR';
			 page->event(&event);
		      } else {
			 event.typ= 'EXIT';
			 page->event(&event);
		      }
		   }
		   if (in) {
		      page->event(&event);
		   }
		}
	     }
	     break;
	  case SDL_VIDEORESIZE:
	     event.sx= ev.resize.w;
	     event.sy= ev.resize.h;
	     graphics_init(event.sx, event.sy, 0);
	     page_switch(page);		// HIDE, RESZ, SHOW, DRAW
	     break;
	  case SDL_QUIT:
	     event.typ= 'QUIT';
	     page->event(&event);
	     break;
	 }
      }

      // Deliver the TICK if required
      if (tick) {		
	 event.typ= 'TICK';
	 page->event(&event);
      }
   }

   return 0;
}

// END //
