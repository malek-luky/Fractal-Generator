///////////////////////////////////////////////////////////////////////////////
//  CIRCULAR BUFFER USED IN MAIN.C
///////////////////////////////////////////////////////////////////////////////

#ifndef __EVENT_QUEUE_H__
#define __EVENT_QUEUE_H__
#include "message.h"

/* SOURCE OF THE EVENT */
typedef enum
{
   EV_NUCLEO,
   EV_KEYBOARD,
   EV_NUM // number of possible events
} event_source;

/* TYPE OF THE EVENT */
typedef enum
{
   EV_COMPUTE,     // nucleo starts computation with defined nbr_tasks
   EV_SET_COMPUTE, // set the computation, start computing when '1' is pressed
   EV_RESET_CHUNK, // reset the chunk id
   EV_ABORT,       // stop the computation
   EV_GET_VERSION, // print the software version
   EV_THREAD_EXIT, // send the boss info that the thread terminated correctly
   EV_QUIT,        // info 'q' from keyboard thread to quit the program
   EV_SERIAL,      // the message that the nucleo button was pressed?
   EV_TYPE_NUM,    // number of event types
   EV_CPU,         // calcualte the fractal using cpu
   EV_INCREASE,    // increase the parameter c
   EV_DECREASE,    // decrease the parameter c
   EV_ANIMATE,     // animate fractal to default values
   EV_CLEAR_GRID,  // inicialize the grid to zeros
   EV_UPDATE_GRID  // copy the atual computation to default grid
} event_type;

/* KEYBOARD MESSAGE */
typedef struct
{
   int param; // which command we received
} event_keyboard;

/* NUCLEO MESSAGE */
typedef struct
{
   message *msg; // pointer to allocated message sent by nucleo
} event_serial;

/* ALL ABOVE IN ONE STRUCT */
typedef struct event
{
   event_source source;
   event_type type;
   union {
      int param;
      message *msg; // decrease the memory usage, only one union can be used
   } data;
} event;

/* FUNTIONS */
void queue_init(void);
void queue_cleanup(void);
event queue_pop(void);
void queue_push(event ev);
bool is_quit();
void set_quit();

#endif
