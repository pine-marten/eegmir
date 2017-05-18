//
//	Simple application to test speed of filter implementations
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.  This
//        file is released under the GNU General Public License (GPL)
//        version 2 as published by the Free Software Foundation.  See
//        the file COPYING for details, or visit
//        <http://www.gnu.org/copyleft/gpl.html>.
//

// We're including it to make compiling different versions easier
#include "fidlib.c"

#define NL "\n"

void 
usage() {
   error(NL "test: Simple application to test speed of filter implementations"
	 NL "      by calculating an impulse response."
	 NL 
	 NL "Usage:  test [-cd] <count> <immediate-filter-spec> ..."
	 NL "Option '-c' combines all the sub-filters into just one filter."
	 NL "Option '-d' dumps output values to STDERR"
	 );
}

int 
main(int ac, char **av) {
   char dmy;
   int f_comb= 0;
   int f_dump= 0;
   int cnt;
   FidFilter *filt;
   void *run;
   void *buf;
   double (*funcp)(void*, double);
   char *desc;

   // Process arguments
   ac--; av++;
   while (ac > 0 && av[0][0] == '-' && av[0][1]) {
      char ch, *p= *av++ + 1; 
      ac--;
      while ((ch= *p++)) switch (ch) {
       case 'c':
	  f_comb= 1;
	  break;
       case 'd':
	  f_dump= 1;
	  break;
       default:
	  usage();
      }
   }

   if (ac != 2) usage();
   if (1 != sscanf(av[0], "%d %c", &cnt, &dmy)) usage();

   // Create the filter
   filt= fid_design(av[1], 1.0, -1.0, -1.0, 0, 0);
   if (f_comb) filt= fid_flatten(filt);
   run= fid_run_new(filt, &funcp);
   buf= fid_run_newbuf(run);

   // Do the impulse response
   if (!f_dump) {
      funcp(buf, 1.0);
      while (cnt-- > 1)
	 funcp(buf, 0.0);
   } else {
      fprintf(stderr, "%g\n", funcp(buf, 1.0));
      while (cnt-- > 1)
	 fprintf(stderr, "%g\n", funcp(buf, 0.0));
#ifdef RF_JIT
      {
	 FILE *out= fopen("jit_dump.s", "w");
	 fid_run_dump(out);
	 fclose(out);
      }
#endif
   }
      
   // All done
   return 0;
}

// END //
   
