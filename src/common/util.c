//
//      Utility functions
//

#ifdef HEADER

#define DEBUG if (DEBUG_ON) warn
#define ALLOC(type) ((type*)Alloc(sizeof(type)))
#define ALLOC_ARR(cnt, type) ((type*)Alloc((cnt) * sizeof(type)))
#define XGROW(var,siz) if ((var)+(siz) > ((void**)&var)[1]) XGrow(&(var),(var)+(siz))

#else

void 
error(char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   fprintf(stderr, PROGNAME ": ");
   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n");
   exit(1);
}

void
errorSDL(char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   fprintf(stderr, PROGNAME ": ");
   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n  %s\n", SDL_GetError());
   exit(1);
}

void 
warn(char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n");
}

void *
Alloc(int size) {
   void *vp= calloc(1, size);
   if (!vp) error("Out of memory");
   return vp;
}

void *
StrDup(char *str) {
   char *cp= strdup(str);
   if (!cp) error("Out of memory");
   return cp;
}

void *
StrDupRange(char *str, char *end) {
   int size= (char*)end-(char*)str;
   char *cp= malloc(size+1);
   if (!cp) error("Out of memory");
   memcpy(cp, str, size);
   cp[size]= 0;
   return cp;
}   

void *
MemDup(void *p0, void *p1) {
   int size= (char*)p1-(char*)p0;
   char *cp= malloc(size);
   if (!cp) error("Out of memory");
   memcpy(cp, p0, size);
   return cp;
}

//
//	Expandable allocator.  Requires two consecutive pointers to be
//	stored in a structure, and a pointer to these to be passed as
//	the first argument.  The first points to the start of the
//	allocated region, and the second the end+1.  These can be used
//	to easily test if there is enough space.  If there isn't, a
//	call to XGrow doubles the space until it is enough.  The macro
//	XGROW makes checks very efficient.  A typical XGrow call would
//	be: XGrow(&ss->arr, ss->arr+50), and the equivalent call to
//	XGROW is: XGROW(ss->arr, 50).  Note that 50 is in whatever
//	units the type ss->arr contains.
//

void 
XAlloc(void **var, int siz) {
   var[0]= Alloc(siz);
   var[1]= ((char*)var[0]) + siz;
}

void 
XGrow(void **var, void *pos) {
   if (pos > var[1]) {
      char *v0= var[0];
      char *v1= var[1];
      int siz= (char *)pos - v0;
      int len, len2;
      char *new;

      len= v1-v0;
      len2= len * 2;
      while (len2 < siz) len2 *= 2;

      new= realloc(v0, len2);
      if (!new) error("Out of memory");	

      memset(new + len, 0, len2-len);
      var[0]= new;
      var[1]= new + len2;
   }
}

#endif

// END //
