///////////////////////////////////////////////////////////////////////////////
//  CONTAINS ALL NECESERITIES TO OPERATE NON-BLOCK TERMINAL
///////////////////////////////////////////////////////////////////////////////

#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <poll.h>
#include <fcntl.h>
#include "serial_nonblock.h"

#ifndef BAUD_RATE
#define BAUD_RATE B115200
#endif

/* CLOSE THE SERIAL PORT */
int serial_close(int fd)
{
   return close(fd);
}

/* WRITE CHAT TO SERIAL PORT */
int serial_putc(int fd, char c)
{
   return write(fd, &c, 1);
}

/* GET CHAR FROM SERIAL PORT */
int serial_getc(int fd)
{
   char c;
   int r = read(fd, &c, 1);
   return r == 1 ? c : -1;
}

/* OPENS THE SERIAL PORT */
int serial_open(const char *fname)
{
   int fd = open(fname, O_RDWR | O_NOCTTY | O_SYNC);
   assert(fd != -1);
   struct termios term;
   assert(tcgetattr(fd, &term) >= 0);
   cfmakeraw(&term);
   term.c_cc[VTIME] = 2; //set vtime
   term.c_cc[VMIN] = 1;
   cfsetispeed(&term, BAUD_RATE);
   cfsetospeed(&term, BAUD_RATE);
   assert(tcsetattr(fd, TCSADRAIN, &term) >= 0);
   assert(fcntl(fd, F_GETFL) >= 0);
   assert(tcsetattr(fd, TCSADRAIN, &term) >= 0);
   assert(fcntl(fd, F_GETFL) >= 0);
   tcflush(fd, TCIFLUSH);
   tcsetattr(fd, TCSANOW, &term);
   return fd;
}

/* IF NO ANSWER IS RECEIVED, THE PROGRAM IS CLOSED */
int serial_getc_timeout(int fd, int timeout_ms, unsigned char *c)
{
   struct pollfd ufdr[1];
   int r = 0;
   ufdr[0].fd = fd;
   ufdr[0].events = POLLIN | POLLRDNORM;
   if ((poll(&ufdr[0], 1, timeout_ms) > 0) &&
       (ufdr[0].revents & (POLLIN | POLLRDNORM)))
   {
      r = read(fd, c, 1);
   }
   return r;
}
