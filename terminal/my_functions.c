///////////////////////////////////////////////////////////////////////////////
//  USER FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/* ADVANCED ASSERT */
void my_assert(bool r, const char *fcname, int line, const char *fname)
{
	if (!r)
	{
		fprintf(stderr, "ERROR : My_assert failed: %s() line %d in %s!\n",
				fcname, line, fname);
		exit(105);
	}
}

/* ALLOC WITH ALLOCATION CHECK */
void *my_alloc(size_t size)
{
	void *ret = malloc(size);
	if (!ret)
	{
		fprintf(stderr, "ERROR: Cannot allocate!\n");
		exit(101);
	}
	return ret;
}

/* SWITCH THE TERMINAL FROM COOKED MODE INTO RAW MODE AN VICE VERSA */
void call_termios(int reset)
{
	static struct termios tio, tioOld;
	tcgetattr(STDIN_FILENO, &tio);
	if (reset)
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &tioOld);
	}
	else
	{
		tioOld = tio; // backup
		cfmakeraw(&tio);
		tio.c_oflag |= OPOST;
		tcsetattr(STDIN_FILENO, TCSANOW, &tio);
	}
}

/* MACRO FOR INFO PRINT */
void INFO(char *msg)
{
	fprintf(stderr, "\033[1;34mINFO:\033[0m   %s", msg);
}

/* MACRO FOR WARN PRINT */
void WARN(char *msg)
{
	fprintf(stderr, "\033[1;33mWARN:\033[0m   %s", msg);
}

/* MACRO FOR ERROR PRINT */
void ERROR(char *msg)
{
	fprintf(stderr, "\033[1;31mERROR:\033[0m  %s", msg);
}

/* MACRO FOR NUCLEO PRINT */
void NUCLEO(char *msg)
{
	fprintf(stderr, "\033[1;34mNUCLEO:\033[0m %s", msg);
}
