//
//	Configuration file handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

#ifdef HEADER

typedef struct Parse Parse;
struct Parse {
   char *txt;		// Entire file
   char *pos;		// Current read-position
   char *sect;		// Current section-name
   char *rew;		// 'Rewind' read-position; previous value of ->pos
   			//   after a successful parse()
};

#else

#ifndef NO_ALL_H
#include "all.h"
#endif

//
//      Load a file as a single string into memory
//

static char *
load_file(char *fnam) {
   int len;
   char *rv;
   FILE *in;

   if (!(in= fopen(fnam, "rb")))        // "b" binary for Windows
      error("Can't open file: %s", fnam);

   if (0 != fseek(in, 0, SEEK_END) ||
       0 > (len= ftell(in)))
      error("Can't find file length: %s", fnam);

   fseek(in, 0, SEEK_SET);

   rv= Alloc(len+1);
   if (1 != fread(rv, len, 1, in) ||
       0 != fclose(in))
      error("Read error on file: %s", fnam);

   rv[len]= 0;
   return rv;
}

//
//	Report an error on a particular line of the config file, and
//	return 1.
//

int 
line_error(Parse *pp, char *p, char *fmt, ...) {
   va_list ap;
   char buf[1024];
   int len;
   va_start(ap, fmt);

   len= vsnprintf(buf, sizeof(buf), fmt, ap);
   if (len < 0 || len >= sizeof(buf)-1) 
      error("line_error exceeded buffer");

   if (p && p > pp->txt) {
      int lin= 1;
      char *q= p, tmp;
      while (q >= pp->txt) lin += (*q-- == '\n');
      while (p > pp->txt && p[-1] != '\n') p--;
      for (q= p; *q && *q != '\n'; q++) ;
      tmp= *q; *q= 0;
      applog("Error in config, line %d: %s\n  %s", lin, buf, p);
      *q= tmp;
   } else {
      applog("Error in config: %s", buf);
   }

   return 1;
}

//
//	Load up the config file.  This is expected to be called as a
//	separate thread.
//

int  
load_config(void *vp) {
   char *fnam= vp;
   char *txt= load_file(fnam);
   char *p, *q, tmp;
   
   applog("Loading configuration ...");

   // Strip out all the comments
   for (p= q= txt; *p; ) {
      int ch= *q++= *p++;
      if (ch == '#') {
	 q--;
	 while (*p && *p != '\n') p++;
      }
   }
   *q= 0;
   
   // Work through in chunks between [xxx] section markers
   for (p= txt; *p; ) {
      char marker[128];
      Parse parse;
      int len;

      parse.txt= txt;
      while (isspace(*p)) p++;
      if (*p != '[') 
	 return line_error(&parse, p, "Expecting a [xxx] marker");
      for (q= p+1; isalnum(*q) || *q == '-' || *q == '_'; q++) ;
      if (*q != ']')
	 return line_error(&parse, p, "Bad [xxx] marker");
      p++;

      len= q-p;
      if (len+1 > sizeof(marker))
	 return line_error(&parse, p, "Bad [xxx] marker");

      memcpy(marker, p, len);
      marker[len]= 0;
      parse.sect= marker;
      
      p= q= q+1;
      while (*q) {
	 while (*q && *q != '\n') q++;
	 if (*q == '\n') {
	    while (isspace(*q)) q++;
	    if (*q == '[') break;
	 }
      }

      tmp= *q; *q= 0;
      parse.pos= p;
      if (handle_sect(&parse)) {
	 applog("Configuration FAILURE.");
	 return 1;
      }
      *q= tmp;
      p= q;
   }
   free(txt);
   applog("Configuration complete.");
   return 0;
}

//
//	Handle a section of the config file
//

int 
handle_sect(Parse *pp) {
   char dmy;
   int ival;

   applog("  setting up [%s] ...", pp->sect);

   if (1 == sscanf(pp->sect, "F%d %c", &ival, &dmy)) {
      if (ival < 1 || ival > 12)
	 return line_error(pp, 0, "Bad config file section name: %s", pp->sect);
      return !server && handle_page_setup(pp, ival-1);
   }

   if (0 == strcmp(pp->sect, "audio"))
      return !server && handle_audio_setup(pp);

#ifdef UNIX_SERIAL
   if (0 == strcmp(pp->sect, "unix-dev"))
      return handle_dev_setup(pp);
#endif

#ifdef WIN_SERIAL
   if (0 == strcmp(pp->sect, "win-dev"))
      return handle_dev_setup(pp);
#endif
   
   if (0 == strcmp(pp->sect, "unix-dev") ||
       0 == strcmp(pp->sect, "win-dev"))
      return 0;		// Ignore if it doesn't apply to this OS

   return line_error(pp, 0, "Unknown config file section name: %s", pp->sect);
}

//
//	Parsing stuff
//

//
//	Attempt to parse from the current position using the format
//	string given.  Tokens in the input are separated by white
//	space, or terminated by a ';' or ','.  Expects any text
//	provided in the formatting string to match tokens exactly.
//	Formatting string white space can match any amount of white
//	space in the input (including newlines characters).  Supported
//	formats:
//
//	%d decimal integer (int*)
//	%f floating-point number (double*)
//	%x hexadecimal integer (int*)
//	%t token string (char**,char**: start pointer, end+1 pointer)
//	%q quoted string (char**,char**: start pointer, end+1 pointer)
//	%r remainder of expression up to ';' (char**,char**: start, end+1)
//	%i identifier [a-zA-Z_][a-zA-Z0-9_]* (char**,char**: start, end+1)
//	%T token string (char**, strdup'd string)
//	%Q quoted string (char**, strdup'd string)
//	%R remainder of expression up to ';' (char**, strdup'd string)
//	%I identifier [a-zA-Z_][a-zA-Z0-9_]* (char**, strdup'd string)
//
//	Formats may be used within tokens (e.g. F%d/%d).  %t/%q/%r/%i
//	return pointers into the data being parsed.  %q does not
//	handle any embedded escape sequences nor escaped quote marks.
//	However it does handle both "" and '' strings.
//
//	%T/%Q/%R/%I work the same as the lowercase versions, except
//	that they put a strdup'd string in the given location; if the
//	location already contains a non-0 value, this is free'd first.
//
//	Data is written to the provided locations only if the entire
//	parse-string matches.  This means that it is safe to do many
//	'trial' calls to parse(), knowing that only a successful one
//	will update variables.  Also, the current position in the file
//	being parsed is only updated on a full match.
//
//	Returns: 0 not a complete match (pointer not advanced, no
//	variables updated), 1 complete match (pointer advanced,
//	variables filled in).
//

int 
parse(Parse *pp, char *fmt0, ...) {
   va_list ap;
   char *p, *q;
   double *dp;
   int *ip;
   long lval;
   double dval;
   char **cpp;
   int ch, qu;
   char *fmt;
   int upd;

   va_start(ap, fmt0);
   p= pp->pos;
   while (isspace(*p)) p++;
   pp->rew= pp->pos= p;

   // Makes two passes.  First is to check it all matches, second is
   // to fill in the variables.
   for (upd= 0; upd < 2; upd++) {
      p= pp->pos;
      fmt= fmt0;
      while ((ch= *fmt++)) {
	 if (isspace(ch)) {
	    // This marks a token break, so check that it is valid
	    if (!(*p == ';' || *p == ',' || *p == 0 || isspace(*p)))
	       return 0;
	    while (isspace(*p)) p++;
	    while (isspace(*fmt)) fmt++;
	    continue;
	 }
	 
	 if (ch == ';' || ch == ',') {
	    while (isspace(*p)) p++;
	    while (isspace(*fmt)) fmt++;
	    if (*p++ != ch)
	       return 0;
	    continue;
	 }
	 
	 if (ch == '%') {
	    switch (*fmt++) {
	     case 'd':
		if (upd) ip= va_arg(ap, int*);
		lval= strtol(p, &q, 10);
		if (q == p) return 0;
		if (upd) *ip= lval; 
		p= q; break;
	     case 'f':
		if (upd) dp= va_arg(ap, double*);
		dval= strtod(p, &q);
		if (q == p) return 0;
		if (upd) *dp= dval;
		p= q; break;
	     case 'x':
		if (upd) ip= va_arg(ap, int*);
		lval= strtol(p, &q, 16);
		if (q == p) return 0;
		if (upd) *ip= lval;
		p= q; break;
	     case 't':
		if (upd) { cpp= va_arg(ap, char**); *cpp= p; }
		for (q= p; !(isspace(*p) || *p == ';'  || *p == ',' || *p == 0); p++) ;
		if (q == p) return 0;
		if (upd) { cpp= va_arg(ap, char**); *cpp= p; }
		break;
	     case 'i':
		if (upd) { cpp= va_arg(ap, char**); *cpp= p; }
		if (!(isalpha(*p) || *p == '_')) return 0;
		for (q= p+1; isalnum(*p) || *p == '_'; p++) ;
		if (upd) { cpp= va_arg(ap, char**); *cpp= p; }
		break;
	     case 'q':
		qu= *p++;
		if (qu != '"' && qu != '\'') return 0;
		if (upd) { cpp= va_arg(ap, char**); *cpp= p; }
		while (!(*p == qu || *p == 0)) p++;
		if (upd) { cpp= va_arg(ap, char**); *cpp= p; }
		if (*p++ != qu) return 0;
		break;
	     case 'r':
		if (upd) { cpp= va_arg(ap, char**); *cpp= p; }
		while (!(*p == ';' || *p == ',' || *p == 0)) p++;
		if (upd) { cpp= va_arg(ap, char**); *cpp= p; }
		break;
	     case 'T':
		for (q= p; !(isspace(*p) || *p == ';' || *p == ',' || *p == 0); p++) ;
		if (q == p) return 0;
		if (upd) { cpp= va_arg(ap, char**); free(*cpp); *cpp= StrDupRange(q, p); }
		break;
	     case 'I':
		if (!(isalpha(*p) || *p == '_')) return 0;
		for (q= p+1; isalnum(*p) || *p == '_'; p++) ;
		if (upd) { cpp= va_arg(ap, char**); free(*cpp); *cpp= StrDupRange(q, p); }
		break;
	     case 'Q':
		qu= *p++;
		if (qu != '"' && qu != '\'') return 0;
		for (q= p; !(*p == qu || *p == 0); p++) ;
		if (upd) { cpp= va_arg(ap, char**); free(*cpp); *cpp= StrDupRange(q, p); }
		if (*p++ != qu) return 0;
		break;
	     case 'R':
		for (q= p; !(*p == ';' || *p == ',' || *p == 0); p++) ;
		if (upd) { cpp= va_arg(ap, char**); free(*cpp); *cpp= StrDupRange(q, p); }
		break;
	     default:
		error("Bad format in parse(): unknown sequence %%%c", fmt[-1]);
	    }
	    continue;
	 }
	 
	 if (*p++ != ch)
	    return 0;
      }
   }

   pp->pos= p;
   return 1;
}

//
//	Check that parse is complete, i.e. it has reached the end of
//	the input string.
//

int 
parseEOF(Parse *pp) {
   while (isspace(*pp->pos)) pp->pos++;
   return *pp->pos == 0;
}

//
//	Handle setup of a page
//

int 
handle_page_setup(Parse *pp, int fn) {
   if (parse(pp, "bands;")) {
      p_fn[fn]= p_bands_init(pp);
      return (p_fn[fn] == 0);
   }
   
   if (parse(pp, "timing;")) {
      p_fn[fn]= p_timing_init(pp);
      return (p_fn[fn] == 0);
   }
   
   if (parse(pp, "audio;")) {
      p_fn[fn]= p_audio_init(pp);
      return (p_fn[fn] == 0);
   }
   
   return line_error(pp, pp->pos, "Unknown page type");
}	 

#endif

// END //
