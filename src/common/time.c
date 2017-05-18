//
//      Get the current time in milliseconds (without any specific
//      reference for '0' time)
//

#ifdef WIN_TIME
int 
time_now_ms() {
   return SDL_GetTicks();
}
#endif

#ifdef UNIX_TIME
int 
time_now_ms() {
   static int start= 0;
   int now;
   struct timeval tv;

   gettimeofday(&tv, NULL);
   now= tv.tv_sec*1000+tv.tv_usec/1000;

   if (!start) start= now;
   return now-start;
}
#endif

//
//	Get the current time of day and return it as a HH:MM:SS string
//	at the given location.  Space for 9 characters is required.
//

#ifdef UNIX_TIME
void 
time_hhmmss(char *dst) {
   time_t now;
   struct tm tm;
   time(&now);
   localtime_r(&now, &tm);
   sprintf(dst, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
}
#endif

#ifdef WIN_TIME
void 
time_hhmmss(char *dst) {
   SYSTEMTIME st;
   GetLocalTime(&st);
   sprintf(dst, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
}
#endif

// END //
