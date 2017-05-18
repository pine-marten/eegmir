//
//	Common include file
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	One of the target macros should be defined before this point:
//	T_LINUX, T_MINGW, or T_MSVC.
//

#define VERSION "0.1.12"
#define PROGNAME "eegmir"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <math.h>

#include "fidlib/fidlib.h"

#ifndef T_MSVC
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef T_MSVC
#include <float.h>
#define NAN nan_global
#endif

#ifdef T_LINUX
#define UNIX_TIME
#define UNIX_SERIAL
#define UNIX_SOCKETS
#endif

#ifdef T_MINGW
#define WIN_TIME
#define WIN_SERIAL
#define WIN_SOCKETS
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef T_MSVC
#define WIN_TIME
#define WIN_SERIAL
#define WIN_SOCKETS
#define WIN32_LEAN_AND_MEAN
#endif


// I really don't know if this is portable, but it works in GCC both
// with and without optimisation (GCC precompiles it into a constant)
#ifndef NAN
#define NAN (0.0/0.0)
#endif

#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif

#ifndef M_LN10
#define M_LN10         2.30258509299404568402  /* log_e 10 */
#endif

#ifdef T_MINGW
#define isnan(val) _isnan(val)
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#endif

#ifdef T_MSVC
#define isnan(val) _isnan(val)
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#endif

#ifdef UNIX_TIME
 #include <sys/ioctl.h>
 #include <sys/times.h>
#endif

#ifdef WIN_TIME
 #include <windows.h>
 #include <mmsystem.h>
#endif

#ifdef UNIX_SERIAL
 #include <termios.h>
 #include <sys/ioctl.h>
#endif

#ifdef WIN_SERIAL
 #include <windows.h>
#endif

// With HEADER defined, these C files just give their header info
// (mainly structure-definitions).  Perhaps this is bit unusual --
// people normally seem to use separate header files -- but at least
// it keeps the structures together with their related functions.

#define HEADER 1
#include "common/page.c"
#include "common/scratch.c"
#include "common/strstream.c"
#include "common/util.c"
#include "audio.c"
#include "clock.c"
#include "settings.c"
#include "device.c"
#include "complex.c"
#include "config.c"
#include "page_audio.c"
#include "page_bands.c"
#include "page_timing.c"
#undef HEADER

#include "proto.h"

#ifndef DEBUG_ON
#define DEBUG_ON 0
#endif

// END //

