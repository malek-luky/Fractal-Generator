///////////////////////////////////////////////////////////////////////////////
//  USER FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include <stdbool.h>
#include <stdlib.h>

void my_assert(bool r, const char *fcname, int line, const char *fname);
void *my_alloc(size_t size);
void call_termios(int reset);
void INFO(const char *str);
void WARN(const char *str);
void ERROR(const char *str);
void NUCLEO(char *msg);

#endif
