//
//	Jim's C pre-processor
//
//        Copyright (c) 2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	This tool rewrites multiple-character constants into hex
//	constants, so 'ABCD' becomes 0x41424344, which cuts down on
//	all the warnings from GCC.
//
//	I also convert // comments to /* */ comments, which is more
//	ANSI-C compatible, although again GCC handles these fine
//	without this conversion.
//
//	I also provide a shorthand for 'this' in the form of '$'.  In
//	any routine you can mark a variable at the top level (i.e. in
//	the function arguments or top-level function body) with a
//	trailing '$' to indicate that it is the 'this' value for this
//	routine.  From that point on, any mention of '$' is converted
//	into that variable name, and any identifier prefixed with '$'
//	maps to this->identifier (e.g. after "con$", "$xx" maps to
//	"con->xx").
//
//	The tool preprocesses STDIN to STDOUT, and leaves all
//	line-breaks intact, permitting accurate debugging.  It should
//	work correctly on all valid C code.
//

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

void 
error(char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n");
   exit(1);
}

int 
main(int ac, char **av) {
   int in_comm1= 0;	// Within /* */ comments ?
   int in_comm2= 0;	// Within //... comments ?
   int in_str1= 0;	// Within a "" quoted string?
   int in_str2= 0;	// Within a '' quoted string?
   int in_id= 0;	// Within an identifier?
   int ch0, ch1, ch2;	// Input look-ahead buffer
   int wr, rd;		// Number to copy over, number to skip 
   char buf[128];	// Buffer for '' strings and identifiers
   int slen;		// Length of '' string read into buf so far
   int lev= 0;		// Nesting level within parentheses () and braces {}
   char this[128];	// Current this ($) name, or empty string
   int lin= 1;		// Current line number
   char *p;
   int a;

#define SHIFT { if (ch0 == '\n') lin++; ch0= ch1; ch1= ch2; ch2= getchar(); }

   this[0]= 0;

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
	 if (ch0 == '/' && ch1 == '*') ch1= '+';	// Map /* */ to /+ +/ within comment
	 if (ch0 == '*' && ch1 == '/') ch0= '+';
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

	 if (slen + 3 > sizeof(buf)) 
	    error("Bad '' string, too long, at line %d: '%s...", lin, buf);

	 if (ch0 == '\\') {
	    buf[slen++]= ch0;
	    buf[slen++]= ch1;
	    rd= 2; continue;
	 }
	 buf[slen++]= ch0;
	 rd= 1; continue;
      }

      if (in_id) {
	 if (isalnum(ch0) || ch0 == '_' || ch0 == '$') {
	    if (slen+2 == sizeof(buf)) 
	       error("Identifier too long, line %d: %s", lin, buf);
	    buf[slen++]= ch0; buf[slen]= 0; rd= 1; continue;
	 }
	 in_id= 0;
	 if (buf[0] == '$') {			// Insert this or this-> reference
	    if (!this[0]) error("'this' value ($) used without being set, line %d: %s", lin, buf);
	    fputs(this, stdout);
	    if (slen > 1) fputs("->", stdout); 
	    fputs(buf+1, stdout);
	 } else if (buf[slen-1] == '$') {	// Set 'this' value 
	    if (lev != 1) error("Not valid to set 'this' ($) value above or below first (){} nesting level, line %d", lin); 
	    if (this[0]) error("Not valid to redefine 'this' ($) value without a '}' first, line %d", lin); 
	    buf[--slen]= 0;
	    strcpy(this, buf);
	    fputs(buf, stdout);
	 } else {				// Normal indentifier
	    fputs(buf, stdout);
	 }
	 // Drops through to interpret 'ch0' ...
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

      if (isalpha(ch0) || ch0 == '_' || ch0 == '$') {
	 buf[0]= ch0; buf[1]= 0; slen= 1; rd= 1; in_id= 1; continue;
      }

      switch (ch0) {
       case '(': lev++; break;
       case ')': lev--; break;
       case '{': lev++; break;
       case '}': lev--; if (lev == 0) this[0]= 0; break;
      }
      
      wr= 1; continue;
   }

   if (in_comm1) 
      error("Unterminated /* */ comment at end of file");

   if (in_comm2) {
      printf(" */");
   }

   if (in_str1) 
      error("Unterminated \"\" string at end of file");

   if (in_str2) 
      error("Unterminated '' string at end of file");

   if (lev > 0) 
      error("Unclosed (){} parentheses/braces at end of file");
   if (lev < 0) 
      error("Too many closed (){} parentheses/braces at end of file");

   return 0;
}

// END //

