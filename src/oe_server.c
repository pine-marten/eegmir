//
//	Test OpenEEG server -- minimal fake just for testing; very
//	rough code.
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

#ifndef NO_ALL_H
#include "all.h"
#endif

#ifdef UNIX_SOCKETS
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#ifdef WIN_SOCKETS
#include <winsock.h>
#endif

static void writeData(int fd, char *dat, int len);

#define SERV_TCP_PORT 8336

void 
server_loop() {
   int fd, rv, tmp;
   struct sockaddr_in sv_addr;
   struct sockaddr_in cl_addr;
   int afd;

   // Create TCP socket
   fd= socket(PF_INET, SOCK_STREAM, 0);
   if (fd < 0) 
      error("socket() call failed, errno %d: %s", errno, strerror(errno));

   // Set SO_REUSEADDR
   tmp= 1;
   if (0 != setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)))
      error("setsockopt() call failed, errno %d: %s", errno, strerror(errno));

   // Bind to port 8336
   sv_addr.sin_family      = AF_INET;
   sv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   sv_addr.sin_port        = htons(SERV_TCP_PORT);
   if (0 > bind(fd, (struct sockaddr*)&sv_addr, sizeof(sv_addr)))
      error("bind() call failed, errno %d: %s", errno, strerror(errno));

   // Listen
   if (0 > listen(fd, 5))
      error("listen() call failed, errno %d: %s", errno, strerror(errno));

   applog("Minimal fake OpenEEG server listening on port %d", SERV_TCP_PORT);

   // Handle only a single client connection and then exit
   tmp= sizeof(cl_addr);
   afd= accept(fd, (struct sockaddr *)&cl_addr, &tmp);
   if (afd < 0)
      error("accept() call failed, errno %d: %s", errno, strerror(errno));

   // Close original socket
   close(fd);

   applog("Accepted connection");
 
   // Read commands a byte at a time; not the best/fastest way, but at
   // least guaranteed to work
   {
      char buf[256];
      char *end= buf + sizeof(buf) - 1;
      char *p= buf;

      //      sprintf(buf, "Minimal fake OpenEEG server hosted by EEGMIR version " VERSION "\r\n");
      //      writeData(afd, buf, strlen(buf));

      while (1) {
	 if (p >= end) {
	    end[0]= 0;
	    applog("Server input buffer overflow:\n%s", buf);
	    p= buf;
	 }

	 // Read one character
	 rv= read(afd, p, 1);
	 if (rv < 0) {
	    if (errno == EAGAIN || errno == EINTR) continue;
	    error("read() on socket failed, errno %d: %s", errno, strerror(errno));
	 }
	 if (rv != 1)
	    continue;

	 if (*p == '\r')	// Ignore \r characters
	    continue;

	 if (*p != '\n' && *p != 0) {
	    p++;
	    continue;
	 }

	 *p= 0; p= buf;

	 // Blank lines perhaps caused by \r\n EOL sequences
	 if (0 == strcmp(buf, "")) 
	    continue;

	 if (0 == strncmp(buf, "getheader", 9)) {
	    applog("Sending EDF header");
	    writeData(afd, "200 OKAY\r\n", 10);
	    send_edf_header(afd);
	    continue;
	 }

	 if (0 == strncmp(buf, "watch", 5)) {
	    applog("Sending Samples");
	    writeData(afd, "200 OKAY\r\n", 10);
	    server_out= afd;
	    continue;
	 }

	 if (0 == strncmp(buf, "display", 7)) {
	    writeData(afd, "200 OKAY\r\n", 10);
	    continue;
	 }

	 applog("Unknown command ignored: %s", buf);
	 writeData(afd, "400 NYI\r\n", 9);
      }
   }
}

//
//	Write a chunk of data
//

static void 
writeData(int fd, char *dat, int len) {
   int rv;

   while (len != 0) {
      rv= write(fd, dat, len);

      if (rv < 0) {
	 if (errno == EINTR || errno == EAGAIN)
	    continue;
	 error("client write() error, errno %d: %s", errno, strerror(errno));
      }
      
      len -= rv; dat += rv;
   }
}

//
//	Write to a field in the EDF header; takes into account the
//	weird interleaved field system used for the channel header
//	records.
//

static char *wrEDF_dat;
static int wrEDF_n_chan;

static void 
wrEDF(int chan, int off, int wid, char *fmt, ...) {
   va_list ap;
   char tmp[128];
   char *p;

   va_start(ap, fmt);
   vsnprintf(tmp, sizeof(tmp), fmt, ap);	// Doesn't matter if it is truncated
   p= strchr(tmp, 0);
   while (p-tmp < wid) *p++= 32;
   
   if (chan < 0)
      memcpy(wrEDF_dat + off, tmp, wid);
   else 
      memcpy(wrEDF_dat + 256 + off*wrEDF_n_chan + wid*chan, tmp, wid);
}

//
//	Send the EDF header corresponding to the current device
//

void 
send_edf_header(int fd) {
   char buf[260];
   char *p;
   int a;
   int n_chan= dev->n_chan;
   char *dat;
   int len;

   if (n_chan > 240) 
      error("Too many channels for server to handle: %d", n_chan);
   
   wrEDF_n_chan= n_chan;
   len= 256*(1 + wrEDF_n_chan);	// +1 for overall header
   wrEDF_dat= dat= ALLOC_ARR(len, char);

   // Fill with fake info
   wrEDF(-1, 0, 8, "%d", 0);			// Version
   wrEDF(-1, 8, 80, "%s", "Santa Claus");	// Patient
   wrEDF(-1, 88, 80, "%s", "NFB stress relief session");	// Recording ID
   wrEDF(-1, 168, 8, "%02d.%02d.%02d", 24, 12, 3);		// Start date
   wrEDF(-1, 176, 8, "%02d.%02d.%02d", 19, 10, 0);		// Start time
   wrEDF(-1, 184, 8, "%d", 256 * (n_chan + 1));		// Bytes in header record (+1 header, +1 error channel)
   wrEDF(-1, 192, 44, "");			// Reserved
   wrEDF(-1, 236, 8, "%d", -1);			// Number of data records
   wrEDF(-1, 244, 8, "0.%06d", (int)(1e6 / dev->rate));	// Length of sample in s
   wrEDF(-1, 252, 4, "%d", n_chan);

   //   // Write error/flag channel data
   //   wrEDF(0, 0, 16, "%s", "OpenEEG flags");
   //   wrEDF(0, 16, 80, "%s", "b0: data/sync error, b1-b15: undefined (0)");
   //   wrEDF(0, 96, 8, "%s", "none");
   //   wrEDF(0, 104, 8, "%d", 0);
   //   wrEDF(0, 112, 8, "%d", 1);
   //   wrEDF(0, 120, 8, "%d", 0);
   //   wrEDF(0, 128, 8, "%d", 1);
   //   wrEDF(0, 136, 80, "%s", "");
   //   wrEDF(0, 216, 8, "%d", 1);
   //   wrEDF(0, 224, 32, "");

   // Write main channel data
   for (a= 0; a<n_chan; a++) {
      wrEDF(a, 0, 16, "EEG %d", a);
      wrEDF(a, 16, 80, "%s", "electrode");
      wrEDF(a, 96, 8, "%s", "~V");
      wrEDF(a, 104, 8, "%d", dev->min);
      wrEDF(a, 112, 8, "%d", dev->max);
      wrEDF(a, 120, 8, "%d", dev->min);
      wrEDF(a, 128, 8, "%d", dev->max);
      wrEDF(a, 136, 80, "%s", "");
      wrEDF(a, 216, 8, "%d", 1);
      wrEDF(a, 224, 32, "");
   }

   // Check that all space is filled
   for (a= 0; a<len; a++) 
      if (!dat[a])
	 error("Internal error; some empty space in generated EDF headers, offset %d:\n%s", a, dat);
   
   writeData(fd, dat, len);
   writeData(fd, "\r\n", 2);
   free(dat);

   // All done
}      
   

//
//	Handle any new incoming data samples
//

//#define SAMPLE(nn) (Sample*)(dev->smp + (nn) * dev->s_smp)
//#define SAMPLE_INC(nn) nn= (nn+1) & dev->mask

void 
server_handler() {
   static int counter= 0;
   
   // Ignore if we're not ready to output yet
   if (server_out < 0) {
      server_rd= dev->wr;
      return;
   }

   // Output all we can
   {
      char buf[512];
      char *end= &buf[512];
      char *p;

      int n_chan= dev->n_chan;
      int a;

      while (server_rd != dev->wr) {
	 // Add given sample to buffer
	 Sample *ss= SAMPLE(server_rd);

	 counter++;
	 counter &= 255;

	 if (!ss->err) {
	    p= buf;
	    p += snprintf(p, end-p, "! 0 %d %d", counter, n_chan);
	    for (a= 0; a<n_chan; a++) 
	       p += snprintf(p, end-p, " %d", ss->val[a]);
	    p += snprintf(p, end-p, "\r\n");
	    writeData(server_out, (void*)buf, (p-buf));
	 }
	 SAMPLE_INC(server_rd);
      }
   }
}

// END //


