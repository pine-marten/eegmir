//
//	OpenEEG client, i.e. socket device handling; quickly hacked
//	together to test how well this works.  Rough code, could be
//	improved a lot.
//
//        Copyright (c) 2002-2004 Jim Peters <http://uazu.net/>.
//        Released under the GNU GPL version 2 as published by the
//        Free Software Foundation.  See the file COPYING for details,
//        or visit <http://www.gnu.org/copyleft/gpl.html>.
//

#ifndef NO_ALL_H
#include "all.h"
#endif

#ifdef UNIX_SOCKETS
#include <sys/socket.h>
#include <netdb.h>
#endif

#ifdef WIN_SOCKETS
#include <winsock.h>
char *
hstrerror(int err) {
   static char buf[40];
   sprintf(buf, "h_errno %d", err);
   return buf;
}
#endif

static void writeData(int fd, char *dat, int len) ;
static void readData(char *dat, int len) ;
static void readLine(char *buf, int len) ;
static int get_8i(char *dat);
static double get_8f(char *dat);

static int rb_fd;		// FD to read from
static char readbuf[256];	// Internal cache
static int rb_len= 0;		//
static char *rb_pos= 0;		//


// Trim spaces off a short string, and return a pointer to a copy of it
static char *
trim(char *p, int len) {
   static char buf[128];
   if (len > sizeof(buf)-1) return "";
   memcpy(buf, p, len);
   p= buf + len;
   while (p > buf && isspace(p[-1])) p--;
   *p= 0;
   return buf;
}

//
//	Make the connection, put the server into sample-sending mode
//	and setup the dev-> structure ready to handle the data stream
//

int 
setup_server_connection(char *serv, int port, Parse *pp) {
   int fd;
   char buf[260], *tmp;
   struct sockaddr_in sv_addr;
   struct hostent *he;
   int client_num;
   int a;

   // Lookup the host
   applog("    looking up server %s ...", serv);
   he= gethostbyname(serv);
   if (!he) 
      return applog("Can't find server %s: %s", serv, hstrerror(h_errno));

   if (he->h_length != 4) 
      return applog("Panic!  gethostbyname() returned an address longer than 4 bytes");

   memset((void *)&sv_addr, 0, sizeof(sv_addr));
   sv_addr.sin_family= AF_INET;
   memcpy(&sv_addr.sin_addr.s_addr, he->h_addr, he->h_length);
   sv_addr.sin_port= htons(port);

   // Connect
   applog("    connecting to server %s ...", serv);

   fd= socket(AF_INET, SOCK_STREAM, 0);
   if (fd < 0) 
      return applog("Can't create socket, errno %d: %s", errno, strerror(errno));

   if (0 > connect(fd, (struct sockaddr *)&sv_addr, sizeof(sv_addr)))
      return applog("Can't connect to server, errno %d: %s", errno, strerror(errno));

   // Setup the FD for read routines
   rb_fd= fd;

   // Enter display mode
   tmp= "display\n";
   writeData(fd, tmp, strlen(tmp));
   readLine(buf, 256);
   if (buf[0] != '2') 
      return applog("Can't set display mode:\n %s", buf);
   
   // Look for a device that is willing to send us a header
   {
      client_num= -1;
      for (a= 0; a<16; a++) {
	 sprintf(buf, "getheader %d\n", a);
	 writeData(fd, buf, strlen(buf));
	 readLine(buf, 256);
	 if (buf[0] == '2') {
	    client_num= a;
	    break;
	 }
	 if (buf[0] != '4')
	    return applog("Unexpected response to 'getheader %d' command:\n %s", 
			  a, buf);
      }
      if (client_num < 0)
	 return applog("No NeuroServer client (0..15) was willing to send an EDF header");
   }
   
   // Read in the EDF headers
   {
      int n_chan;
      int cnt;
      int a;
      int first= 1;
      char *dat;
      int spr;		// Samples per record

      buf[256]= 0;
      readData(buf, 256);

      for (a= 0; a<256; a++)
	 if ((buf[a] & 255) < 32 || (buf[a] & 255) > 126)
	    applog("Bad character in EDF header, offset %d: %d", a, buf[a]&255);

      applog("    EDF header:");
      applog("      Version:  %s", trim(buf, 8));
      applog("      Patient:  %s", trim(buf+8, 80));
      applog("      Info:     %s", trim(buf+88, 80));
      applog("      Start date: %-8.8s, time: %-8.8s", buf+168, buf+176);
      applog("      Reserved: %s", trim(buf+192, 44));
      applog("      Records:  %s", trim(buf+236, 8));
      applog("      Duration: %s seconds per record", trim(buf+244, 8));
      applog("      Channels: %s", trim(buf+252, 4));

      dev->rate= 1.0/get_8f(buf + 244);

      n_chan= atoi(buf + 252);
      cnt= get_8i(buf + 184);
      if (cnt != (n_chan+1)*256) 
	 return applog("EDF header byte count disagrees with channel "
		       "count: '%-8.8s' vs '%-4.4s'", buf+184, buf+252);

      dat= ALLOC_ARR(256*n_chan, char);
      readData(dat, 256*n_chan);

      for (a= 0; a<256*n_chan; a++)
	 if ((dat[a] & 255) < 32 || (dat[a] & 255) > 126)
	    applog("Bad character in EDF header, offset %d: %d", a+256, buf[a]&255);

      // Macro to convert offset and field width into a pointer to the
      // referenced field in the current channel record (even though
      // that record is interleaved with all the others in the EDF
      // format).
      #define DAT(off,wid) (dat + off*n_chan + wid*a)

      for (a= 0; a<n_chan; a++) {
	 int min, max;
	 int cnt;

	 {
	    char t1[16], t2[16], t3[16];
	    strcpy(t1, trim(DAT(120,8), 8));
	    strcpy(t2, trim(DAT(128,8), 8));
	    strcpy(t3, trim(DAT(216,8), 8));
	    applog("      Chan %d: range %s to %s, %s samples per record",
		   a, t1, t2, t3);
	 }

	 min= get_8i(DAT(120,8));
	 max= get_8i(DAT(128,8));
	 cnt= get_8i(DAT(216,8));
	 if (min >= max)
	    return applog("Bad min/max values on EDF channel %d: '%-8.8s' to "
			  "'%-8.8s'", a, DAT(120,8), DAT(128,8));
	 
	 if (first) {
	    dev->min= min;
	    dev->max= min;
	    spr= cnt;
	    first= 0;
	 } else {
	    // Make it one big range; best we can do without more coding
	    if (min < dev->min) dev->min= min;
	    if (max > dev->max) dev->max= max;
	    if (spr != cnt) 
	       return applog("All channels must have same samples per record;\n"
			     " channel %d reports %d instead of %d",
			     a, cnt, spr);
	 }
      }

      dev->rate *= spr;
      if (dev->rate < 1 || dev->rate > 10000)		// Rough check for validity
	 return applog("EDF header has bad sampling rate: %g", dev->rate);

      // Release channel header data
      free(dat);

      dev->n_chan= n_chan;
      dev->handler= nsd_handler;
      dev->n_flag= 0;
      dev->fd= fd;
   }

   // Read the \r\n at the end of the EDF header
   readLine(buf, 256);
   if (buf[0])
      applog("Unexpected garbage at end of EDF header:\n %s", 
	     buf);

   // Start the samples coming
   sprintf(buf, "watch %d\n", client_num);
   writeData(fd, buf, strlen(buf));
   readLine(buf, 256);
   if (buf[0] != '2') 
      return applog("Unexpected failure to 'watch %d' command:\n %s", 
		    client_num, buf);

   // Setup handler variables
   dev->hdata[0]= -1;		// Current EOL scanning position
   dev->hdata[1]= 0;		// Previous packet-counter value
   dev->hdata[2]= 1;		// Minimum packet-counter value
   dev->hdata[3]= 0;		// Maximum packet-counter value, or < min if unset
   
   return 0;
}


//
//	Decode an integer encoded as 8 characters, without worrying
//	about trailing garbage.
//

static int 
get_8i(char *dat) {
   char buf[9];
   memcpy(buf, dat, 8);
   buf[8]= 0;
   return atoi(buf);
}

//
//	Decode a floating point number encoded as 8 characters,
//	without worrying about trailing garbage.
//

static double 
get_8f(char *dat) {
   char buf[9];
   memcpy(buf, dat, 8);
   buf[8]= 0;
   return atof(buf);
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
//	Read a chunk of data
//

static void 
readData(char *dat, int len) {
   int rv;

   while (len != 0) {
      if (rb_len) {
	 int rv= rb_len < len ? rb_len : len;
	 memcpy(dat, rb_pos, rv);
	 dat += rv; len -= rv;
	 rb_pos += rv; rb_len -= rv;
	 continue;
      }

      rv= read(rb_fd, dat, len);

      if (rv < 0) {
	 if (errno == EINTR || errno == EAGAIN)
	    continue;
	 error("client read() error, errno %d: %s", errno, strerror(errno));
      }
      
      len -= rv; dat += rv;
   }
}

//
//	Read a line of data into the given area.  Returns with the
//	\r\n or \n removed, and the string NUL-terminated.  If it
//	overflows the buffer, then 'len-1' characters will have been
//	read.
//

static void 
readLine(char *buf, int len) {
   int rv;

   while (len > 1) {
      if (!rb_len) {
	 rv= read(rb_fd, readbuf, sizeof(readbuf));
	 if (rv == 0) continue;
	 if (rv < 0) {
	    if (errno == EINTR || errno == EAGAIN) continue;
	    error("client read() error, errno %d: %s", errno, strerror(errno));
	 }
	 rb_pos= readbuf;
	 rb_len= rv;
      }

      *buf= *rb_pos++;
      rb_len--;

      // Ignore '\r'
      if (buf[0] == '\r') continue;

      // End on '\n'
      if (buf[0] == '\n') break;
      
      buf++; len--;
   }

   // Terminate the string
   *buf= 0;
}


//
//	Handler for the NeuroServer data stream
//

void 
nsd_handler() {
   int avail;
   int rd= dev->ird;
   int wr= dev->iwr;
   int pp= dev->hdata[0];
   int pplim, end, cnt;
   char *buf= dev->ibuf;
   int mask= dev->imask;
   int a;
   char line[256+8];
   char *p;

#define INC_pp { pp++; pp &= mask; }

   pplim= (rd + sizeof(line)-8) & mask;
   if (pp < 0) pp= rd;

   while (pp != wr) {
      if (pp == pplim) {
	 // Line too long
	 char *p= line;
	 while (rd != pplim) { *p++= buf[rd]; rd++; rd &= mask; }
	 *p= 0;
	 applog("Incoming NeuroServer line-length exceeded %d bytes; skipping:\n %s", 
		sizeof(line)-8, line);

	 // Completely discard all buffered data
	 dev->hdata[0]= dev->ird= dev->iwr;
	 return;
      }
      if (buf[pp] != '\n') {
	 INC_pp;
	 continue;
      }

      // Okay, we have a newline.  Read a whole line of data into our
      // local buffer after stripping off any \r
      end= pp; INC_pp;
      if (end != rd &&
	  buf[(end-1)&mask] == '\r') {
	 end--; end &= mask;
      }
      p= line;
      if (rd > end) {
	 cnt= mask+1-rd;
	 memcpy(p, buf+rd, cnt);
	 p += cnt; rd= 0;
      }
      cnt= end-rd;
      memcpy(p, buf+rd, cnt);
      p[cnt]= 0;
      
      // Release that space in the buffer now we have definitely read it
      dev->ird= rd= pp;
      pplim= (rd + sizeof(line)-8) & mask;

      // Handle the line
      nsd_line(line);
   }

   // Save the scanning position
   dev->hdata[0]= pp;

#undef INC_pp
}


//
//	Handle a line of data read from the NeuroServer
//

void 
nsd_line(char *line) {
   Sample *ss;
   int cn, pc, nc, okay;
   int prev= dev->hdata[1];
   int min= dev->hdata[2];
   int max= dev->hdata[3];
   int next;
   char *p, *q;
   int a, cnt;

   if (line[0] != '!') {
      applog("Unknown type of NeuroServer data received:\n %s", line);
      return;
   }

   // Write a new Sample entry
   if (!dev->now) dev->now= time_now_ms();
   ss= SAMPLE(dev->wr);
   memset(ss, 0, dev->s_smp);
   ss->stamp= dev->now;
   ss->time= clock_inc(&dev->clock, dev->now);

#define VALID (p!=q)
   
   okay= 0;
   cn= strtol(p= line+1, &q, 10);	// Client number (ignored)
   if (VALID) {
      pc= strtol(p= q, &q, 10);
      if (VALID) {
	 nc= strtol(p= q, &q, 10);
	 if (VALID) {
	    if (nc != dev->n_chan) {
	       applog("NeuroServer input line with wrong number of channels:\n %s", line);
	       return;
	    }
	    for (a= 0; a<nc; a++) {
	       ss->val[a]= strtol(p= q, &q, 10);
	       if (!VALID) break;
	    }
	    if (a == nc) {
	       okay= 1;
	       
	       // Check for trailing junk
	       for (p= q; *p && isspace(*p); p++) ;
	       if (*p) applog("Warning: trailing junk on NeuroServer sample line:\n %s", line);
	    }
	 }
      }
   }

   if (!okay) {
      applog("Badly formatted NeuroServer sample line:\n %s", line);
      return;
   }

#undef VALID

   // Check for packet-count errors
   if (max < min) prev= max= min= pc;
   if (pc < min) min= pc;
   if (pc > max) max= pc;
   next= (prev == max) ? min : prev+1;
   
   // Missing packets converted into error packets
   cnt= 0;
   while (pc != next) {
      // Duplicate the current packet data into the next slot
      Sample *ss2;
      int wr2= ((dev->wr + 1) & dev->mask);
      ss2= SAMPLE(wr2);
      memcpy(ss2, ss, dev->s_smp);

      // Set up an error packet and output
      ss->err= 1;
      for (a= 0; a<dev->n_chan; a++) ss->val[a]= 0;
      SAMPLE_INC(dev->wr);

      ss= ss2;
      prev= next;
      next= (prev == max) ? min : prev+1;
      cnt++;
   }
   if (cnt) 
      applog("NeuroServer packet counter indicates %d missing packets", cnt);

   // Output the original packet
   SAMPLE_INC(dev->wr);
   prev= pc;

   dev->hdata[1]= prev;
   dev->hdata[2]= min;
   dev->hdata[3]= max;
}
   
// END //


