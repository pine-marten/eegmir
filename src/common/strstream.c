//
//	String stream
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

//
//	This sets up a FILE* which can be printed to normally, after
//	which the result can be converted into an allocated string.
//	This works by opening /dev/null and then changing the output
//	buffer to one large enough to contain the entire planned
//	output.  When output is complete, a final NUL is written, and
//	then strdup is used to generate the output result.  A couple
//	of bytes are written at the start of the buffer to catch the
//	case where the estimated size was not large enough.
//
//	Usage:
//
//	  StrStream *ss;
//	  FILE *out;
//	  char *txt;
//
//	  ss= strstream_open(max_output_size);
//	  out= ss->out;
//
//	  while (...) {
//	    fprintf(out, "...", ...);
//	  }
//
//	  txt= strstream_close(ss);
//	  if (!txt) error("Buffer was too small!");
//

#ifdef HEADER

typedef struct StrStream StrStream;
struct StrStream {
   FILE *out;
   char *buf;
};

#else

#ifndef NO_ALL_H
#include "../all.h"
#endif

//
//	Create a new StrStream.  'maxsiz' should be the maximum output
//	size expected (allowing for about 4 spare characters).
//

StrStream *
strstream_open(int maxsiz) {
#if defined(T_MINGW) || defined(T_MSVC)
   char *fnam= "dev_null.txt";
#else
   char *fnam= "/dev/null";
#endif
   StrStream *ss= ALLOC(StrStream);
   ss->out= fopen(fnam, "w");
   if (!ss->out) error("Can't open %s for output in strstream_open()", fnam);
   ss->buf= malloc(maxsiz);
   if (!ss->buf) error("Out of mem");
   
   setvbuf(ss->out, ss->buf, _IOFBF, maxsiz);
   fputs("\x01\xFE", ss->out);		// Check
   
   return ss;
}

//
//	Close an StrStream and return the output string, or 0 if we
//	overflowed the buffer.
//

char *
strstream_close(StrStream *ss) {
   char *rv;
   char buf[16];

   fputc(0, ss->out);

   // Check for overflow
   if (ss->buf[0] != '\x01' ||
       ss->buf[1] != '\xFE') {
      rv= 0;
   } else {
      rv= strdup(ss->buf+2);
      if (!rv) error("Out of mem");
   }

   setvbuf(ss->out, buf, _IOFBF, sizeof(buf));	// Attempt to zap the buffer contents
   fclose(ss->out);
   free(ss->buf);
   free(ss);

   return rv;
}

#endif

// END //
