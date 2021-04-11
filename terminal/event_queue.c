///////////////////////////////////////////////////////////////////////////////
//  CIRCULAR BUFFER USED IN MAIN.C
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "event_queue.h"
#include "my_functions.h"

#ifndef QUEUE_CAPACITY
#define QUEUE_CAPACITY 16 // size for 16 messages
#endif

/* STRUCT FOR THE WHOLE QUEUE */
static struct
{
   event queue[QUEUE_CAPACITY]; // message queue
   volatile int in;             // pointer to the circular queue
   volatile int out;            // pointer to the circular queue
   bool quit;
   pthread_mutex_t mtx;
   pthread_cond_t cond;
} q = {.in = 0, .out = 0, .quit = false};

/* INICIALIZE THE QUEUE */
void queue_init(void)
{
   if (pthread_mutex_init(&q.mtx, NULL))
   {
      ERROR("Could not initialize mutex.\n");
      exit(100);
   }
   if (pthread_cond_init(&q.cond, NULL))
   {
      ERROR("Could not initialize condvar.\n");
      exit(100);
   }
}

/* FREE QUEUE */
void queue_cleanup(void)
{
   while (q.in != q.out)
   {
      event ev = queue_pop();
      if (ev.type == EV_SERIAL && ev.data.msg)
      {
         free(ev.data.msg);
      }
   }
}

/* RETURN THE INTEGER FROM KEYBOARD OR POINTER ON THE MESSAGE */
event queue_pop(void)
{
   event ret = {.source = EV_NUM, .type = EV_TYPE_NUM};
   pthread_mutex_lock(&(q.mtx));
   while (q.in == q.out)
   {
      pthread_cond_wait(&(q.cond), &(q.mtx)); // wait for next event
   }
   if (q.in != q.out)
   {
      ret = q.queue[q.out];                 // return value from the queue
      q.out = (q.out + 1) % QUEUE_CAPACITY; // moves the pointer
      pthread_cond_broadcast(&(q.cond));    // notify waiting threads
   }
   pthread_mutex_unlock(&(q.mtx));
   return ret;
}

/* PUSH THE INTERRUPT TO THE QUEUE AND SEND IT TO BOSS */
void queue_push(event ev)
{
   pthread_mutex_lock(&(q.mtx));
   while (((q.in + 1) % QUEUE_CAPACITY) == q.out)
   {                                          // queue is full wait for pop
      pthread_cond_wait(&(q.cond), &(q.mtx)); // wait for some event is popped
   }
   q.queue[q.in] = ev;
   q.in = (q.in + 1) % QUEUE_CAPACITY;
   pthread_cond_broadcast(&(q.cond)); // notify waiting threads
   pthread_mutex_unlock(&(q.mtx));
}

/* SET THE QUIT ON TRUE IN CRITICAL SECTION */
void set_quit()
{
   pthread_mutex_lock(&(q.mtx));
   q.quit = true;
   pthread_mutex_unlock(&(q.mtx));
}

/* RETURN TRUE IF QUIT IS TRUE*/
bool is_quit()
{
   bool ret;
   pthread_mutex_lock(&(q.mtx));
   ret = q.quit;
   pthread_mutex_unlock(&(q.mtx));
   return ret;
}
