//
//	Complex number handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

#ifdef HEADER

#else

#ifndef NO_ALL_H
#include "all.h"
#endif

//
//      Setup a sincos generator (i.e. e^ibt for suitable b)
//

inline void 
sincos_init(double *buf, double freq) {
   buf[0]= 1;
   buf[1]= 0;
   buf[2]= cos(freq * 2 * M_PI);
   buf[3]= sin(freq * 2 * M_PI);
}


//
//      Generate the next values from a sincos generator
//

inline void 
sincos_step(double *buf) {
   double v0, v1;
   v0= buf[0] * buf[2] - buf[1] * buf[3];
   v1= buf[0] * buf[3] + buf[1] * buf[2];
   buf[0]= v0;
   buf[1]= v1;
}

#endif

// END //
