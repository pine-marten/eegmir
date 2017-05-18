//
//	Serial device handling
//
//        Copyright (c) 2002-2003 Jim Peters <http://uazu.net/> and
//        others.  Released under the GNU GPL version 2 as published
//        by the Free Software Foundation.  See the file COPYING for
//        details, or visit <http://www.gnu.org/copyleft/gpl.html>.
//
//	Windows code in this file is based on cateeg.c provided by
//	Jack Spaar <jspaar@myrealbox.com>, a list member of the
//	OpenEEG project.
//

#ifdef HEADER

typedef struct Sample Sample;
struct Sample {
   int stamp;		// Timestamp -- the time at which this sample actually arrived (ms)
   int time;		// Estimated correct clock time for this sample (ms/65536)
   short err;		// Error flag -- set if there was a sync error around this point
   short flags;		// Sampled flag values
   short val[2];	// Sampled data values (structure expanded to the correct number)
};

typedef struct Device Device;
struct Device {
#ifdef WIN_SERIAL
   HANDLE hPort;	// File descriptor
#endif
   int fd;		// File descriptor (used for UNIX serial, and for sockets on Win/UNIX)
   FILE *file;		// Input file stream for 'file' command
   double file_cps;	// Incoming characters per second for 'file'
   double file_cc;	// Character counter
   FILE *rawdump;	// Stream to dump incoming bytes to, or 0 if not required

   int init_complete;	// Set when initialisation of device is complete
   int audio;		// Handle serial input in audio callback rather than separate thread
   
   char *ibuf;		// Circular buffer for incoming serial data
   int ilen, imask;	// Length of buffer (bytes) and mask for wrapping
   int ird, iwr;	// Buffer read/write circular offsets; ird==iwr means empty buffer
   int ierr;		// Input error count; number of bad bytes already absorbed from ird

   void (*handler)();	// Protocol-specific handler; converts incoming data into samples
   int n_chan;		// Number of input channels
   double rate;		// Sampling rate (theoretical)
   int min, max;	// Range for sample values; (min+max+1)/2 is taken as the 0-value
   int n_flag;		// Number of flag bits stored in ss->flags
   
   int n_smp;		// Number of input samples stored in circular buffer (power of 2)
   int mask;		// Counter mask (n_smp-1)
   int s_smp;		// Size of the Sample structure
   int wr;		// Current write-offset in circular buffer
   char *smp;		// Buffer itself, containing n_smp Sample structures, each 
   			//  s_smp bytes long

   int now;		// Current time in ms for handler routines, or 0 if not known
   Clock clock;		// ms/65536 clock for samples

   int hdata[8];	// Private data for handler() call
};

#define SAMPLE(nn) (Sample*)(dev->smp + (nn) * dev->s_smp)
#define SAMPLE_INC(nn) nn= (nn+1) & dev->mask
#define SAMPLE_DEC(nn) nn= (nn-1) & dev->mask
#define SAMPLE_WRAP(nn) nn &= dev->mask

#else

#ifndef NO_ALL_H
#include "all.h"
#endif

Device *dev= 0;		// Currently running serial device

//
//      Setup the device
//

int
handle_dev_setup(Parse *pp) {
   char *devname;	// StrDup'd device name
   int devbaud;
   int devtype;		// 0 unset, 1 serial port, 2 file, 3 socket
   char *fmtname;	// StrDup'd format name
   int a;

   if (dev) return line_error(pp, pp->pos, "Duplicate [*-dev] section");

   dev= ALLOC(Device);

   // Defaults
   fmtname= StrDup("");
   devtype= 0;
#ifdef WIN_SERIAL
   devname= StrDup("COM1");
   devbaud= CBR_57600;
#endif
#ifdef UNIX_SERIAL
   devname= StrDup("/dev/ttyS0");
   devbaud= 57600;
#endif

   // Pick up any options
   while (1) {
      if (parse(pp, "port %T %d;", &devname, &devbaud)) {
	 if (devtype) 
	    return line_error(pp, pp->rew, "Duplicate or mixed 'port', 'file' and 'server' commands");
	 devtype= 1;
	 continue;
      }
      if (parse(pp, "file %T %f;", &devname, &dev->file_cps)) {
	 if (devtype) 
	    return line_error(pp, pp->rew, "Duplicate or mixed 'port', 'file' and 'server' commands");
	 devtype= 2;
	 continue;
      }
      if (parse(pp, "server %T;", &devname)) {
	 if (devtype) 
	    return line_error(pp, pp->rew, "Duplicate or mixed 'port', 'file' and 'server' commands");
	 devtype= 3;
	 continue;
      }
      if (parse(pp, "audio-sync;")) {
	 if (!audio)
	    applog("    \x98""audio-sync ignored as audio device is not active");
	 else 
	    dev->audio= 1;
	 continue;
      }
      if (parse(pp, "rawdump;")) {
	 if (!(dev->rawdump= fopen("dump.raw", "wb"))) 
	    applog("    \x98""rawdump ignored; failed to create 'dump.raw'");
	 continue;
      }
      if (parse(pp, "fmt %T;", &fmtname)) continue;
      if (parse(pp, "rate %f;", &dev->rate)) continue;
      if (parse(pp, "chan %d;", &dev->n_chan)) continue;
      if (parse(pp, "flags %d;", &dev->n_flag)) continue;
      break;
   }
   
   if (!parseEOF(pp))
      return line_error(pp, pp->pos, "Unrecognised trailing [*-dev] section entries");

   if (devtype == 0) devtype= 1;

   if (devtype == 3) {
      free(fmtname);
      fmtname= 0;		// Format name not required
   }

   //
   //	Setup the format-handler (except in the case of socket
   //	connection)
   //

   if (fmtname) {
      if (0 == strcmp(fmtname, "modEEGold") ||
	  0 == strcmp(fmtname, "modEEG-P2")) {
	 if (dev->n_chan <= 0 || dev->n_chan > 6) 
	    dev->n_chan= 6;
	 dev->rate= 256.0;
	 dev->handler= modEEGold_handler;
	 dev->min= 0;
	 dev->max= 1023;
	 if (dev->n_flag > 4) dev->n_flag= 4;
	 if (dev->n_flag < 0) dev->n_flag= 0;
      } else if (0 == strcmp(fmtname, "modEEG") ||
		 0 == strcmp(fmtname, "modEEG-P3")) {
	 if (dev->n_chan <= 0 || dev->n_chan > 6) 
	    dev->n_chan= 6;
	 dev->rate= 256.0;
	 dev->handler= modEEG_handler;
	 dev->min= 0;
	 dev->max= 1023;
	 dev->ierr= 1;		// Ignore the first partial packet
	 if (dev->n_flag > 4) dev->n_flag= 4;
	 if (dev->n_flag < 0) dev->n_flag= 0;
      } else if (0 == strcmp(fmtname, "jim-m")) {
	 if (dev->n_chan != 2 && dev->n_chan != 4)
	    return line_error(pp, 0, "Expecting 2 or 4 channels for jim-m format type");
	 if (!dev->rate) dev->rate= (dev->n_chan == 2) ? 140.0 : 130.0;
	 dev->handler= jm_handler;
	 dev->min= 0;
	 dev->max= 255;
	 dev->n_flag= 0;
      } else if (0 == strcmp(fmtname, "auto")) {
	 if (devtype != 3) 
	    return line_error(pp, 0, "Format 'auto' only possible with a server connection");
      } else if (0 == strcmp(fmtname, "jm2")) {
	 return line_error(pp, 0, "Type 'jm2' deprecated; please use 'fmt jim-m; chan 2;'");
      } else if (0 == strcmp(fmtname, "jm4")) {
	 return line_error(pp, 0, "Type 'jm4' deprecated; please use 'fmt jim-m; chan 4;'");
      } else  
	 return line_error(pp, 0, "Serial format '%s' not known", fmtname);
      free(fmtname); fmtname= 0;
   }

   //
   //	Setup the device
   //

   switch (devtype) {
    case 3:	// socket
       if (setup_server_connection(devname, 8336, pp))
	  return 1;
       break;
    case 2:	// file
       dev->file= fopen(devname, "rb");
       if (!dev->file)
	  return line_error(pp, 0, "Unable to open input file: %s", devname);
       break;
    case 1:	// serial port
#ifdef WIN_SERIAL
       applog("    setting up device %s", devname);
       dev->hPort= CreateFile(devname,
			      GENERIC_READ | GENERIC_WRITE,
			      0, NULL, OPEN_EXISTING,
			      FILE_ATTRIBUTE_NORMAL, NULL);
       if (dev->hPort == INVALID_HANDLE_VALUE)
	  error("Couldn't open serial port: %s", devname);
       
       if (!SetCommMask(dev->hPort, EV_RXCHAR)) 
	  error("Couldn't do a SetCommMask(): %s", devname);
       
       // Set read buffer == 10K, write buffer == 4K
       SetupComm(dev->hPort, 10240, 4096);
       
       {
	  DCB dcb;
	  
	  dcb.DCBlength= sizeof(DCB);
	  GetCommState(dev->hPort, &dcb);
	  
	  dcb.BaudRate= devbaud;
	  dcb.ByteSize= 8;
	  dcb.Parity= NOPARITY;
	  dcb.StopBits= ONESTOPBIT;
	  dcb.fOutxCtsFlow= FALSE;
	  dcb.fOutxDsrFlow= FALSE;
	  dcb.fInX= FALSE;
	  dcb.fOutX= FALSE;
	  dcb.fDtrControl= DTR_CONTROL_DISABLE;
	  dcb.fRtsControl= RTS_CONTROL_DISABLE;
	  dcb.fBinary= TRUE;
	  dcb.fParity= FALSE;
	  
	  if (!SetCommState(dev->hPort, &dcb)) 
	     error("Unable to set up serial port parameters");
       }
#endif
       
#ifdef UNIX_SERIAL
       applog("    setting up device %s", devname);
       dev->fd= open(devname, O_RDWR | (dev->audio ? O_NONBLOCK : 0));
       if (dev->fd < 0) error("Unable to open serial device: %s", devname);
       
       {
	  struct termios tt;
	  
	  if (0 != tcgetattr(dev->fd, &tt))
	     error("Can't get serial port termios settings: %s", devname);	 
	  
	  tt.c_cflag &= ~(HUPCL | CSIZE | CSTOPB | PARENB | CRTSCTS);
	  tt.c_cflag |= CLOCAL | CS8 | CREAD;
	  tt.c_iflag= IGNBRK | IGNPAR;
	  tt.c_oflag= 0;
	  tt.c_lflag= 0;
	  tt.c_cc[VMIN]= 1;
	  tt.c_cc[VTIME]= 0;
	  
	  if (0) {	// xon/xoff
	     tt.c_iflag |= IXON | IXOFF;
	     tt.c_cc[VSTOP]= 0x13;
	     tt.c_cc[VSTART]= 0x11;
	  }
	  
	  switch (devbaud) {
	   case 50: a= B50; break;
	   case 75: a= B75; break;
	   case 110: a= B110; break;
	   case 134: a= B134; break;
	   case 150: a= B150; break;
	   case 200: a= B200; break;
	   case 300: a= B300; break;
	   case 600: a= B600; break;
	   case 1200: a= B1200; break;
	   case 1800: a= B1800; break;
	   case 2400: a= B2400; break;
	   case 4800: a= B4800; break;
	   case 9600: a= B9600; break;
	   case 19200: a= B19200; break;
	   case 38400: a= B38400; break;
	   case 57600: a= B57600; break;
	   case 115200: a= B115200; break;
	   case 230400: a= B230400; break;
	   default: error("Serial port baud rate %d not supported", devbaud);
	  }
	  
	  if (cfsetispeed(&tt, a) ||
	      cfsetospeed(&tt, a) ||
	      tcsetattr(dev->fd, TCSAFLUSH, &tt))
	     error("Problem setting up serial port using termios: %s", devname);
       }      
#endif
       break;
   }
   
   free(devname); devname= 0;
   
   //
   //	Setup the sample clock
   //	
   
   clock_setup(&dev->clock, dev->rate, time_now_ms());
   
   //
   //	Setup the buffers
   //

   // Incoming buffer
   dev->ilen= 1024;
   dev->imask= dev->ilen-1;
   dev->ibuf= Alloc(dev->ilen);
   dev->ird= dev->iwr= 0;

   // Sample buffer
   {
      // Length is smallest power of two to contain 10 seconds' worth of data
      int n_smp= (int)(dev->rate * 10);		// 10 seconds' worth of data
      n_smp= n_smp*2-1; 
      while (n_smp & (n_smp-1)) n_smp &= n_smp-1;

      dev->s_smp= sizeof(Sample) 
	 - 2 * sizeof(short)
	 + sizeof(short) * ((dev->n_chan + 1) & ~1);
      dev->s_smp= (dev->s_smp + (sizeof(int)-1)) & ~(sizeof(int)-1);
      dev->n_smp= n_smp;
      dev->mask= n_smp-1;
      dev->smp= Alloc(dev->n_smp * dev->s_smp);
      dev->wr= 0;
   }

   // Fill in reasonable time values as a safety-net for searching code
   for (a= 1; a<=dev->n_smp; a++) {
      Sample *ss= SAMPLE(dev->n_smp-a);
      ss->time= dev->clock.clock - a * dev->clock.clockinc;
   }

   //
   //	Start a thread to handle serial input from now on, unless we
   //	will be handling serial from the audio callback.
   // 

   if (dev->file || !dev->audio) {
      if (!SDL_CreateThread(devtype == 2 ? file_thread : serial_thread, 0))
	 errorSDL("Problem starting serial thread off");
   }

   // All done
   dev->init_complete= 1;
   return 0;
}

//
//	Read and process all outstanding samples on the serial port.
//	'now' is the current time, or 0 if a time_now_ms() call should
//	be made.
//

static void process_input(char *buf, int len);

#ifdef WIN_SERIAL
static void 
serial_read(int now) {
   BYTE buf[512];
   BOOL rv;
   COMSTAT comStat;
   DWORD errorFlags;
   DWORD len;
   
   dev->now= now;

   // Only try to read number of bytes in queue
   ClearCommError(dev->hPort, &errorFlags, &comStat);
   len= comStat.cbInQue;
   if (len > sizeof(buf)) len= sizeof(buf);
   
   if (len > 0) {
      rv= ReadFile(dev->hPort, (LPSTR)buf, len, &len, NULL);
      if (!rv) {
	 len= 0;
	 ClearCommError(dev->hPort, &errorFlags, &comStat);
	 if (errorFlags > 0) 
	    error("Serial read error");
      }
   }
   
   if (len) process_input(buf, len);
}
#endif

#ifdef UNIX_SERIAL
static void 
serial_read(int now) {
   char buf[512];
   int len;

   dev->now= now;
   
   len= read(dev->fd, buf, sizeof(buf));
   if (len > 0) {
      process_input(buf, len);
   } else if (len < 0) {
      if (errno != EAGAIN && errno != EINTR)
	 error("Serial port read error: %s", strerror(errno));
   }
}
#endif


//
//	Main serial thread
//

int 
serial_thread(void *vp) {

#ifdef WIN_SERIAL
   while (1) {
      DWORD dwEvtMask= 0;
      
      WaitCommEvent(dev->hPort, &dwEvtMask, NULL);
      
      if ((dwEvtMask & EV_RXCHAR) == EV_RXCHAR)
	 serial_read(0);
   }
#endif

#ifdef UNIX_SERIAL
   // The device will have been set to blocking reads, so there is no
   // need to have separate wait/read code
   while (1) 
      serial_read(0);
#endif
   
   return 0;
}

//
//	Main thread for file input
//

int 
file_thread(void *vp) {
   int then, now;
   int cc0, cc1;

   then= time_now_ms();
   while (1) {
      SDL_Delay(10);
      now= time_now_ms();
      cc0= (int)dev->file_cc;
      dev->file_cc += dev->file_cps * (now-then) * 0.001;
      cc1= (int)dev->file_cc;
      then= now;
      dev->now= now;

      for (; cc0 != cc1; cc0++) {
	 char buf[1];
	 int ch= fgetc(dev->file);
	 if (ch < 0) {
	    applog("\x82 EOF \x86  Reached end of input file");
	    return 0;		// EOF -- nothing more to do
	 }
	 buf[0]= ch;
	 process_input(buf, 1);
      }
   }
   
   return 0;
}

//
//	Audio thread callback.  Does nothing if we have our own serial
//	thread, otherwise fetches everything we can.
//

void 
serial_audio_callback(int now) {
   if (dev && 
       dev->init_complete && 
       dev->audio && 
       !dev->file)
      serial_read(now);
}


//
//	Process incoming serial data.  This is added to a circular
//	buffer, and the format-type handler is given a chance to
//	update its state.
//

static void 
process_input(char *buf, int len) {
   char *buf0= buf;
   int len0= len;
   while (len > 0) {
      int free= (dev->ird-1 - dev->iwr) & dev->imask;
      int cnt= (dev->ilen-dev->iwr);
      if (cnt > free) cnt= free;
      if (cnt > len) cnt= len;

      memcpy(dev->ibuf+dev->iwr, buf, cnt);
      dev->iwr += cnt;
      dev->iwr &= dev->imask;
      buf += cnt;
      len -= cnt;
   }
   dev->handler();

   // Relay new data to client if in server mode
   if (server && dev->wr != server_rd)
      server_handler();

   // Leave raw dump output until the end for timing reasons
   if (dev->rawdump) {
      if (1 != fwrite(buf0, len0, 1, dev->rawdump))
	 applog("Write error on 'dump.raw'");
   }
}

//
//	Format handlers.  Note that format handlers should use the
//	dev->now value for timestamps if it is non-0, else call
//	time_now_ms() and fill dev->now themselves if/when they find
//	that they need a timestamp value.  This saves the overhead of
//	multiple calls to time_now_ms() for long packets, and also
//	wasted single calls to it when only a couple of bytes have
//	arrived.
//

//
//	Handler for old modularEEG data format (P2)
//
   
void 
modEEGold_handler() {
   int avail;
   int rd= dev->ird;
   int wr= dev->iwr;
   char *buf= dev->ibuf;
   int mask= dev->imask;
   int err= 0;
   int err0;
   int a;
   unsigned char tmp[15];
   Sample *ss;

#define INC_rd { rd++; rd &= mask; }

   while (rd != wr) { 
      avail= (wr-rd) & mask;
      if (avail < 17) return;
      if (!dev->now) dev->now= time_now_ms();

      if (buf[rd] != '\xA5') {
	 INC_rd; dev->ird= rd;		// Byte definitely read
	 dev->ierr++; 
	 if (dev->ierr >= 17) {	
	    // If we've scanned 17 bytes and still not found anything,
	    // write out a sync error.  That way if data is coming
	    // through but it is all bad, then something will at least
	    // appear on the screen, even though it is all marked with
	    // errors.
	    ss= SAMPLE(dev->wr);
	    memset(ss, 0, dev->s_smp);
	    ss->stamp= dev->now;
	    ss->time= clock_inc(&dev->clock, dev->now);
	    ss->err= 1;
	    SAMPLE_INC(dev->wr);
	    applog("\x82 Sync error \x86  Sync bytes missing from serial input stream");
	    dev->ierr= 0;
	 }
	 continue;
      }
      INC_rd;

      if (buf[rd] != '\x5A') {
	 INC_rd; dev->ird= rd;		// Bad bytes definitely read
	 dev->ierr += 2; 
	 continue;
      }
      INC_rd;

      for (a= 0; a<15; a++) {
	 tmp[a]= buf[rd];
	 INC_rd;
      }
      dev->ird= rd;	// We have definitely read these bytes now
      err0= err= dev->ierr;
      dev->ierr= 0;

      // @@@ Check version number (tmp[0] == 2) ?

      // Check for counter sync errors
      if (dev->hdata[0] != tmp[1] && 
	  dev->hdata[1]) {
	    applog("\x82 Sync error \x86  Packet counter indicates %d missing packets",
		   (tmp[1] - dev->hdata[0]) & 255);
	    while (dev->hdata[0] != tmp[1]) {
	       ss= SAMPLE(dev->wr);
	       memset(ss, 0, dev->s_smp);
	       ss->stamp= dev->now;
	       ss->time= clock_inc(&dev->clock, dev->now);
	       ss->err= 1;
	       SAMPLE_INC(dev->wr);
	       dev->hdata[0]++;
	       dev->hdata[0] &= 255;
	    }
      }
      dev->hdata[0]= ++tmp[1];

      // Write a new Sample entry
      ss= SAMPLE(dev->wr);
      memset(ss, 0, dev->s_smp);
      ss->stamp= dev->now;
      ss->time= clock_inc(&dev->clock, dev->now);

      // Grab the channel data.  Bad data gives error marks
      for (a= 0; a<dev->n_chan; a++) {
         int val= (tmp[2*a+2]<<8) + tmp[2*a+3];
         if (val < 0 || val >= 1024) { err++; val= 512; }
         ss->val[a]= val;
      }

      // Grab the flags
      ss->flags= buf[14] & 15;

      // Close off
      if (!dev->hdata[1]) { 
	 // Ignore errors before the very first packet when booting up
	 err= 0; dev->hdata[1]= 1; 
      }
      ss->err= err;
      SAMPLE_INC(dev->wr);
      if (err) applog(err0 ? 
		      "\x82 Sync error \x86  Serial data loss" :
		      "\x82 Data error \x86  Serial data errors");
      err= 0;
   }

#undef INC_rd
}      

//
//	Handler for new modularEEG data format (P3): 
//
//	New ModularEEG packet format:
//
//	0ppppppx		packet header
//	0xxxxxxx
//	
//	0aaaaaaa		channel 0 LSB
//	0bbbbbbb		channel 1 LSB
//	0aaa-bbb		channel 0 and 1 MSB
//	
//	0ccccccc		channel 2 LSB
//	0ddddddd		channel 3 LSB
//	0ccc-ddd		channel 2 and 3 MSB
//	
//	0eeeeeee		channel 4 LSB
//	0fffffff		channel 5 LSB
//	1eee-fff		channel 4 and 5 MSB
//	
//	Key:
//	
//	1 and 0 = sync bits; sync bit is set on final byte of packet
//	p = 6-bit packet counter
//	x = auxillary channel byte
//	a - f = 10-bit samples from ADC channels 0 - 5
//	- = unused, must be zero
//
//	There may be 2, 4 or 6 channels transmitted (packet length of
//	5, 8 or 11).  The sync bit is always set on the final byte of
//	the packet.
//	
//	There are 8 auxillary channels that are transmitted in sequence.
//	The 3 least significant bits of the packet counter determine what
//	channel is transmitted in the current packet.
//	
//	Aux Channel Allocations:
//	
//	0: Zero-terminated ID-string (ASCII encoded).
//	1: 
//	2:
//	3:
//	4: Port D status bits
//	5:
//	6:
//	7:
//
   
void 
modEEG_handler() {
   int rd= dev->ird;
   int wr= dev->iwr;
   char *buf= dev->ibuf;
   int mask= dev->imask;
   int a, len;	
   int p_cnt;		// Packet counter
   int p_aux;		// Auxiliary channel data byte
   int p_chan;		// Number of channels present in current packet
   unsigned char tmp[15];
   int sync;
   Sample *ss;

#define INC_rd { rd++; rd &= mask; }

   while (rd != wr) { 
      for (len= sync= 0; rd != wr && len < 11 && !sync; len++) {
	 tmp[len]= buf[rd];
	 sync= (tmp[len] & 128);
	 INC_rd;
      }

      // Incomplete packet, so give up for now
      if (!sync && len < 11) return;

      if (!dev->now) dev->now= time_now_ms();
      dev->ird= rd;		// Bytes definitely read

      // If we've scanned 11 bytes and not found a sync bit, write
      // out a sync error.  That way if data is coming through but
      // it is all bad, then something will at least appear on the
      // screen, even though it is all marked with errors.
      if (!sync) {
	 ss= SAMPLE(dev->wr);
	 memset(ss, 0, dev->s_smp);
	 ss->stamp= dev->now;
	 ss->time= clock_inc(&dev->clock, dev->now);
	 ss->err= 1;
	 SAMPLE_INC(dev->wr);
	 applog("\x82 Sync error \x86  Sync byte missing from serial input stream");
	 dev->ierr= 1;
	 dev->hdata[0]++;	// Expecting one after
	 continue;
      }

      // We have a packet
      if (dev->ierr) {
	 // If this is the trailing part of a packet which is way too
	 // long, drop it, and look at the next one instead
	 dev->ierr= 0;
	 continue;
      }

      if (len != 5 && len != 8 && len != 11) {
	 // Bad length for our packet
	 ss= SAMPLE(dev->wr);
	 memset(ss, 0, dev->s_smp);
	 ss->stamp= dev->now;
	 ss->time= clock_inc(&dev->clock, dev->now);
	 ss->err= 1;
	 SAMPLE_INC(dev->wr);
	 applog("\x82 Data error \x86  Packet of bad length (%d)", len);
	 dev->hdata[0]++;	// Expecting one after
	 continue;
      }

      // Process packet
      p_cnt= tmp[0] >> 1;
      p_aux= tmp[1] + ((tmp[0] & 1) ? 128 : 0);
      p_chan= ((len-2) / 3) * 2;

      // Check for counter sync errors
      if ((dev->hdata[0]&63) != p_cnt && 
	  dev->hdata[1]) {
	 int cnt= (p_cnt - dev->hdata[0]) & 63;
	 applog("\x82 Sync error \x86  Packet counter indicates %d missing packets", cnt);
	 while (cnt-- > 0) {
	    ss= SAMPLE(dev->wr);
	    memset(ss, 0, dev->s_smp);
	    ss->stamp= dev->now;
	    ss->time= clock_inc(&dev->clock, dev->now);
	    ss->err= 1;
	    SAMPLE_INC(dev->wr);
	 }
      }
      dev->hdata[0]= p_cnt + 1;		// Next expected value

      // Write a new Sample entry
      ss= SAMPLE(dev->wr);
      memset(ss, 0, dev->s_smp);
      ss->stamp= dev->now;
      ss->time= clock_inc(&dev->clock, dev->now);

      // Grab the channel data
      for (a= 0; a<dev->n_chan && a < p_chan; a++) {
	 int off= 2+3*(a>>1);
	 int val= (!(a&1) ? 
		   tmp[off] + ((tmp[off+2] & 0x70) << 3) :
		   tmp[off+1] + ((tmp[off+2] & 0x7) << 7));
         ss->val[a]= val;
      }

      // Grab the flags
      if ((p_cnt & 7) == 4) dev->hdata[4]= p_aux;
      ss->flags= dev->hdata[4] & 15;

      // Close off
      dev->hdata[1]= 1; 	// (ignore errors before first one)
      ss->err= 0;
      SAMPLE_INC(dev->wr);
   }

#undef INC_rd
}      

//
//	Handler for Jim-Meissner data 
//
   
void 
jm_handler() {
   int avail;
   int rd= dev->ird;
   int wr= dev->iwr;
   char *buf= dev->ibuf;
   int mask= dev->imask;
   int a;
   Sample *ss;
   int pktsiz= dev->n_chan + 1;

#define INC_rd { rd++; rd &= mask; }

   while (rd != wr) { 
      avail= (wr-rd) & mask;
      if (avail < pktsiz) return;
      if (!dev->now) dev->now= time_now_ms();
      
      if (buf[rd] != 3) {
	 INC_rd; dev->ird= rd;		// Byte definitely read
	 dev->ierr++; 
	 if (dev->ierr >= pktsiz) {	
	    // If we've scanned a packet-worth and still not found anything,
	    // write out a sync error.
	    ss= SAMPLE(dev->wr);
	    memset(ss, 0, dev->s_smp);
	    ss->stamp= dev->now;
	    ss->time= clock_inc(&dev->clock, dev->now);
	    ss->err= dev->ierr;
	    SAMPLE_INC(dev->wr);
	    applog("\x82 Sync error \x86  Sync bytes missing from serial input stream");
	    dev->ierr= 0;
	 }
	 continue;
      }
      INC_rd;

      // Write a new Sample entry
      ss= SAMPLE(dev->wr);
      memset(ss, 0, dev->s_smp);
      ss->stamp= dev->now;
      ss->time= clock_inc(&dev->clock, dev->now);

      // Grab the channel data.  Bad data gives error marks
      for (a= 0; a<dev->n_chan; a++) {
         ss->val[a]= 255 & buf[rd];
	 INC_rd;
      }
      dev->ird= rd;	// We have definitely read these bytes now

      // Close off
      ss->err= dev->ierr;
      SAMPLE_INC(dev->wr);
      if (dev->ierr) applog("\x82 Sync error \x86  Serial data loss");
      dev->ierr= 0;
   }

#undef INC_rd
}      

#endif

// END //

