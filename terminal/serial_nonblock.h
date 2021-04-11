///////////////////////////////////////////////////////////////////////////////
//  CONTAINS ALL NECESERITIES TO OPERATE NON-BLOCK TERMINAL
///////////////////////////////////////////////////////////////////////////////

#ifndef __PRG_SERIAL_NONBLOCK_H__
#define __PRG_SERIAL_NONBLOCK_H__

int serial_open(const char *fname);
int serial_close(int fd);
int serial_putc(int fd, char c);
int serial_getc(int fd);
int serial_getc_timeout(int fd, int timeout_ms, unsigned char *c);

#endif
