//
//	Jim's C pre-processor
//
//        Copyright (c) 2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	Don't worry; it is not as bad as you think.  I've not yet been
//	tempted into modifying C to be a completely unportable
//	jim_pp-dependent sub-language.  
//
//	All this does is rewrite multiple-character constants into hex
//	constants, so 'ABCD' becomes 0x41424344, which cuts down on
//	all the warnings from GCC.
//
//	Whilst I'm doing this, I also convert // comments to /* */
//	comments, which is more ANSI-C compatible, although again GCC
//	handles these fine without this conversion.
//
//	The tool preprocesses STDIN to STDOUT, and leaves all
//	line-breaks intact, permitting accurate debugging.  It should
//	work correctly on all valid C code.
//

#include <stdio.h>
#include <string.h>

int 
main(int ac, char **av) {
   int in_comm1= 0;	// Within /* */ comments ?
   int in_comm2= 0;	// Within //... comments ?
   int in_str1= 0;	// Within a "" quoted string?
   int in_str2= 0;	// Within a '' quoted string?
   int ch0, ch1, ch2;	// Input look-ahead buffer
   int wr, rd;		// Number to copy over, number to skip 
   char buf[128];	// Buffer for '' strings
   int slen;		// Length of '' string read into buf so far
   char *p;
   int a;

#define SHIFT { ch0= ch1; ch1= ch2; ch2= getchar(); }

   rd= 3;
   wr= 0;
   while (1) {
      while (wr-- > 0) { putchar(ch0); SHIFT; }
      while (rd-- > 0) { SHIFT; }
      
      if (ch0 < 0) break;
      
      if (in_comm1) {
	 if (ch0 == '*' && ch1 == '/') {
	    wr= 2; in_comm1= 0; continue;
	 }
	 wr= 1; continue;
      }

      if (in_comm2) {
	 if (ch0 == '\n') {
	    putchar(' ');
	    putchar('*');
	    putchar('/');
	    wr= 1; in_comm2= 0; continue;
	 }
	 wr= 1; continue;
      }

      if (in_str1) {
	 if (ch0 == '\\') {
	    wr= 2; continue;
	 }
	 if (ch0 == '"') {
	    wr= 1; in_str1= 0; continue;
	 }
	 wr= 1; continue;
      }

      if (in_str2) {
	 if (ch0 == '\'') {
	    rd= 1; in_str2= 0;
	    buf[slen]= 0;
	    if (slen <= 1 || strchr(buf, '\\')) {
	       putchar('\'');
	       fputs(buf, stdout);
	       putchar('\'');
	       continue;
	    }
	    for (a= 0, p= buf; *p; p++) a= (a<<8) | (*p & 255);
	    printf("0x%08X", a);
	    continue;
	 }

	 if (slen + 3 > sizeof(buf)) {
	    fprintf(stderr, "jim_pp: Bad '' string -- too long\n");
	    exit(1);
	 }

	 if (ch0 == '\\') {
	    buf[slen++]= ch0;
	    buf[slen++]= ch1;
	    rd= 2; continue;
	 }
	 buf[slen++]= ch0;
	 rd= 1; continue;
      }

      if (ch0 == '/' && ch1 == '*') {
	 wr= 2; in_comm1= 1; continue;
      }

      if (ch0 == '/' && ch1 == '/') {
	 putchar('/'); putchar('*');
	 rd= 2; in_comm2= 1; continue;
      }

      if (ch0 == '"') {
	 wr= 1; in_str1= 1; continue;
      }

      if (ch0 == '\'') {
	 rd= 1; slen= 0; in_str2= 1; continue;
      }
      
      wr= 1; continue;
   }

   if (in_comm1) {
      fprintf(stderr, "Unterminated /* */ comment at end of file\n");
      exit(1);
   }

   if (in_comm2) {
      printf(" */");
   }

   if (in_str1) {
      fprintf(stderr, "Unterminated \"\" string at end of file\n");
      exit(1);
   }

   if (in_str2) {
      fprintf(stderr, "Unterminated '' string at end of file\n");
      exit(1);
   }

   return 0;
}

// END //

