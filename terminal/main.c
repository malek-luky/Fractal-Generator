///////////////////////////////////////////////////////////////////////////////
//  MAIN - CONTROLING PROGRAM
///////////////////////////////////////////////////////////////////////////////

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "event_queue.h"
#include "message.h"
#include "serial_nonblock.h"
#include "my_functions.h"
#include "computation.h"
#include "gui.h"
#include "xwin_sdl.h"

#define SERIAL_TIMEOUT 500 // timeout for reading from serial port
#define EXIT_SUCCESS 0
#define ANIMATION_FRAMES 500 //number of frames in animation

///////////////////////////////////////////////////////////////////////////////
//  DECLARATION
///////////////////////////////////////////////////////////////////////////////

/* LOCAL VARIABLES */
typedef struct
{
   int fd;          // file descriptor
   bool save_im;    // check whether the image will be saved or not
   char user_input; // input character to control the gui settings
} data_t;

void call_termios(int reset);
void *boss_thread(void *);
void *input_thread(void *);
void *serial_rx_thread(void *); // serial receive buffer
bool send_message(int fd, message *msg);

///////////////////////////////////////////////////////////////////////////////
//  MAIN
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
   const char *serial = argc > 1 ? argv[1] : "/dev/ttyACM0";
   data_t data;
   data.fd = serial_open(serial);
   data.save_im = true;

   if (data.fd == -1)
   {
      ERROR("Cannot open device ");
      fprintf(stderr, "%s\n", serial);
      exit(100);
   }

   /* CHANGE STDIN AS NONBLOCKING */
   fcntl(fileno(stdin), F_SETFL, fcntl(fileno(stdin), F_GETFL) | O_NONBLOCK);
   call_termios(0); // raw mode

   /* PRINT THE WELCOME SCREEN AND INTERACTIVELY CHANGE THE DEFAULT VALUES */
   print_gui();
   while ((data.user_input = getchar()) && data.user_input != 13)
   {
      switch (data.user_input)
      {
      case -1:
         //discard
         break;
      case 'y':
         data.save_im = true;
         printf("\033[6A");
         printf("\033[38C");
         printf("yes");
         printf("\033[6B");
         printf("\033[41D");
         break;
      case 'n':
         data.save_im = false;
         printf("\033[6A");
         printf("\033[38C");
         printf("no ");
         printf("\033[6B");
         printf("\033[41D");
         break;
      default:
         change_settings(data.user_input); // values changed in computation
         print_changed_settings();
         break;
      }
   }

   /* CHECK IF THE VALUES WERE ENTERED CORECTLY */
   if (!correct_input())
   {
      ERROR("The resolutoin or part size aren't set corectly, quiting\n");
      call_termios(1); // restore terminal settings (cooked mode)
      exit(100);
   }

   ////////////////////////////////////////////////////////////////////////////
   //  THREAD INICIALIZATION
   ////////////////////////////////////////////////////////////////////////////

   /* CREATE THREADS */
   enum
   {
      BOSS,
      INPUT,
      SERIAL_RX,
      NUM_THREADS
   };
   const char *thread_names[] = {"Boss", "Input", "Serial In"};
   void *(*thr_functions[])(void *) = {boss_thread,
                                       input_thread,
                                       serial_rx_thread};
   pthread_t threads[NUM_THREADS]; // number of threads

   /* START THREADS */
   for (int i = 0; i < NUM_THREADS; ++i)
   {
      int check = pthread_create(&threads[i], NULL, thr_functions[i], &data);
      fprintf(stderr, "\033[1;34mINFO:\033[0m   %s Thread start: %s\n",
              thread_names[i], check ? "FAIL" : "\033[1;92mOK\033[0m");
      if (check == true)
      {
         ERROR("Fatal error occured, quiting.");
         exit(100);
      }
   }

   /* JOIN THREAD */
   for (int i = 0; i < NUM_THREADS; i++)
   {
      int check = pthread_join(threads[i], NULL);
      fprintf(stderr, "\033[1;34mINFO:\033[0m   %s Thread joined: %s\n",
              thread_names[i], check ? "FAIL" : "\033[1;92mOK\033[0m");
   }

   /* RESTORE EVERYTHING TO DEFAULT */
   queue_cleanup(); // cleanup all events and allocated memory for messages
   gui_cleanup();
   computation_cleanup();
   serial_close(data.fd);
   call_termios(1); // cooked mode - restore terminal settings
   return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//  BOSS THREAD
///////////////////////////////////////////////////////////////////////////////

/* RECEIVE MESSAGES FROM SLAVES AND COMMUNICATE WITH NUCLEO */
void *boss_thread(void *d)
{
   data_t *data = (data_t *)d;
   message msg;
   msg.data.compute.cid = 0;
   msg.data.set_compute.c_re = -0.4;
   msg.data.set_compute.c_im = 0.6;
   queue_init();
   computation_init(); //HERE
   gui_init();
   while (!is_quit())
   {
      event ev = queue_pop();

      /////////////////////////////////////////////////////////////////////////
      //  HANDLE KEYBOARD EVENTS
      /////////////////////////////////////////////////////////////////////////
      if (ev.source == EV_KEYBOARD)
      {
         msg.type = MSG_NBR;
         switch (ev.type)
         {

         case EV_GET_VERSION:
         {
            msg.type = MSG_GET_VERSION;
            INFO("Get version requested\n");
            break;
         }

         case EV_SET_COMPUTE:
            if (number_of_chunks() > 255)
            {
               WARN("The number of chunks ");
               fprintf(stderr, "%d will overflow 8-bit integer!\n",
                       number_of_chunks());
            }
            else
            {
               set_compute(&msg)
                   ? fprintf(stderr, "\033[1;34mINFO:\033[0m   Set new "
                                     "computation resolution %dx%d no. of "
                                     "chunks: %d\n",
                             grid_width(), grid_height(), number_of_chunks())
                   : WARN("New set up discarded due on ongoing computation\n");
            }
            break;

         case EV_COMPUTE:
            enable_comp();
            compute(&msg);
            if (msg.data.compute.cid == 0)
            {
               INFO("New computation started for part ");
               fprintf(stderr, "%d x %d\n",
                       msg.data.compute.n_re,
                       msg.data.compute.n_im);
            }

            else
            {
               INFO("Prepare new chunk of data ");
               fprintf(stderr, "%d for the position %d x %d\n",
                       msg.data.compute.cid,
                       cursor_width(),
                       cursor_height());
            }
            break;

         case EV_ABORT:
            if (is_computing())
            {
               msg.type = MSG_ABORT;
               INFO("Abort request received, waiting for Nucleo response\n");
               abort_comp();
            }
            else
            {
               WARN("Abort requested but it is not computing\n");
            }
            break;

         case EV_RESET_CHUNK:
            if (is_computing())
            {
               WARN("Chunk reset request discarded, "
                    "it is currently computing\n");
            }
            else
            {
               msg.data.compute.cid = 0;
               INFO("Chunk reset request received\n");
            }
            break;

         case EV_QUIT:
            if (is_computing())
            {
               msg.type = MSG_ABORT;
            }
            set_quit();
            break;

         case EV_CPU:
            compute_cpu();
            gui_refresh();
            INFO("The CPU computation is done, jolly good\n");
            if (data->save_im)
            {
               save_image_png();
               save_image_jpg();
               save_image_bmp();
            }
            else
            {
               INFO("Downloading is disabled, image was not saved\n");
            }
            msg.type = MSG_ABORT; // end the nucleo computation
            abort_comp();
            INFO("Reseting Nucleo to default state\n");
            break;

         case EV_ANIMATE:;
            if (is_computing())
            {
               WARN("Stop the current calculation before animation\n");
            }
            else
            {
               INFO("The animation started, enjoy!\n");
               set_up_animation(&msg.data.set_compute, ANIMATION_FRAMES);
               for (int i = 0; i < ANIMATION_FRAMES; i++)
               {
                  animate();
                  compute_cpu();
                  gui_refresh();
               }
               INFO("The animation is done, press 'm' to repeat\n");
               if (data->save_im)
               {
                  save_image_png();
                  save_image_jpg();
                  save_image_bmp();
               }
               else
               {
                  INFO("Downloading is disabled, image was not saved\n");
               }
            }
            break;

         case EV_INCREASE:
            increase_parameter(&msg.data.set_compute);
            INFO("Parameter increased to value ");
            fprintf(stderr, "%+-1.1f %+-1.1fi\n", msg.data.set_compute.c_re,
                    msg.data.set_compute.c_im);
            break;

         case EV_DECREASE:
            decrease_parameter(&msg.data.set_compute);
            INFO("Parameter decreased to value ");
            fprintf(stderr, "%+-1.1f %+-1.1fi\n", msg.data.set_compute.c_re,
                    msg.data.set_compute.c_im);
            break;

         case EV_CLEAR_GRID:
            clear_grid();
            gui_refresh();
            break;

         case EV_UPDATE_GRID:
            update_grid();
            gui_refresh();
            break;

         default:
            break;
         }
         if (msg.type != MSG_NBR) // message received
         {
            if (!send_message(data->fd, &msg))
            {
               ERROR("send_message() didn't send all bytes of the message!\n");
            }
         }
      }

      /////////////////////////////////////////////////////////////////////////
      //  HANDLE NUCLEO EVENTS
      /////////////////////////////////////////////////////////////////////////
      else if (ev.source == EV_NUCLEO)
      {
         if (ev.type == EV_SERIAL)
         {
            message *msg = ev.data.msg;
            switch (msg->type)
            {
            case MSG_STARTUP:
            {
               char str[STARTUP_MSG_LEN + 1];
               for (int i = 0; i < STARTUP_MSG_LEN; ++i)
               {
                  str[i] = msg->data.startup.message[i];
               }
               str[STARTUP_MSG_LEN] = '\0';
               INFO("Nucleo wish you a beatiful day ");
               fprintf(stderr, "%s\n", str);
               break;
            }

            case MSG_VERSION:
               if (msg->data.version.patch > 0)
               {
                  INFO("Nucleo firmware ver. ");
                  fprintf(stderr, "%d.%d-p%d\n",
                          msg->data.version.major,
                          msg->data.version.minor,
                          msg->data.version.patch);
               }
               else
               {
                  INFO("Nucleo firmware ver. ");
                  fprintf(stderr, "%d.%d\n",
                          msg->data.version.major,
                          msg->data.version.minor);
               }
               break;

            case MSG_DONE:
               gui_refresh();
               if (is_done())
               {
                  INFO("Nucleo reports the computation is done, jolly good\n");
                  if (data->save_im)
                  {
                     save_image_png();
                     save_image_jpg();
                     save_image_bmp();
                  }
                  else
                  {
                     INFO("Downloading is disabled, image was not saved\n");
                  }
               }
               else
               {
                  event ev = {.source = EV_KEYBOARD};
                  ev.type = EV_COMPUTE;
                  queue_push(ev);
               }
               break;

            case MSG_ABORT:
               abort_comp();
               INFO("Abort from Nucleo\r\n");
               break;

            case MSG_ERROR:
               ERROR("Receive error from Nucleo\r\n");
               break;

            case MSG_OK:
               INFO("Receive from Nucleo \033[1;92mOK\033[0m\n");
               break;

            case MSG_COMPUTE_DATA:
               if (!is_abort())
               {
                  update_data(&(msg->data.compute_data));
               }
               break;

            default:
               WARN("Unhandled pipe message type");
               fprintf(stderr, "%d\n", msg->type);
               break;
            }
            if (msg) // free the message allocated space
            {
               free(msg);
            }
         }
         else if (ev.type == EV_QUIT)
         {
            set_quit();
         }
      }
      else
      {
         ERROR("Unknown source of the event");
      }
   }
   return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  1 KEYBOARD INPUT
///////////////////////////////////////////////////////////////////////////////

/*
 * KEYBOARD INPUT -> OUTPUT MESSAGE
 * 'g' -> MSG_GET_VERSION     print the Nucleo firmware version
 * 's' -> MSG_SET_COMPUTE     set the computation
 * '1' -> MSG_COMPUTE         start computation
 * 'a' -> MSG_ABORT           pause the current calcualtion
 * 'r' -> reset cid           reset the chunk id
 * 'l' -> delete_buffer       delete active buffer withlocal_data
 * 'p' -> redraw image        redraw the image with currentlocal_data in buffer
 * 'c' -> instafracral        calulculates the fractal on CPU
 * 'q' -> quit                quit program, while computing MSG_ABORT is sent
 * '+' -> increase c          increase parameter while copmuting
 * '-' -> decrease c          decrease parameter while copmuting
 */

/* RECEIVE THE USER INPUT AND SEND THE MESSAGE TO THE QUEUE */
void *input_thread(void *arg)
{
   int c;
   event ev = {.source = EV_KEYBOARD};
   while (!is_quit() && (c = getchar()))
   {
      ev.type = EV_TYPE_NUM;
      switch (c)
      {
      case 'g': // get version
         ev.type = EV_GET_VERSION;
         break;
      case '1': // compute (send the request only if we aren't computing)
         if (is_computing())
         {
            WARN("New computation discarded due on ongoing computation\n\r");
         }
         else
         {
            ev.type = EV_COMPUTE;
         }
         break;
      case 's': // set parameters
         ev.type = EV_SET_COMPUTE;
         break;
      case 'a': // abort
         ev.type = EV_ABORT;
         break;
      case 'r': // reset chunk number
         ev.type = EV_RESET_CHUNK;
         break;
      case 'q': // quit
         ev.type = EV_QUIT;
         set_quit();
         break;
      case 'c': // use cpu
         ev.type = EV_CPU;
         break;
      case 'l': // clear the grid
         ev.type = EV_CLEAR_GRID;
         break;
      case 'p': // update the grid with current computation
         ev.type = EV_UPDATE_GRID;
         break;
      case 'm': // animate
         ev.type = EV_ANIMATE;
         break;
      case '+': //increase c while copmuting
         if (!is_computing())
         {
            ev.type = EV_INCREASE;
         }
         break;
      case '-': //decrease c while computing
         if (!is_computing())
         {
            ev.type = EV_DECREASE;
         }
         break;
      default: // discard all other keys

         break;
      }
      if (ev.type != EV_TYPE_NUM) // new event correctly received
      {
         queue_push(ev);
      }
   }
   ev.type = EV_QUIT;
   queue_push(ev);
   INFO("Exit input thead ");
   fprintf(stderr, "%p \033[1;92mOK\033[0m\n", (void *)pthread_self());
   return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  2 SERIAL PORT
///////////////////////////////////////////////////////////////////////////////

/* RECEIVE MESSAGE FROM SERIAL PORT AND PUTS IT TO THE QUEUE */
void *serial_rx_thread(void *d)
{
   data_t *data = (data_t *)d;
   uint8_t msg_buf[sizeof(message)]; // buffer for all possible messages
   event ev = {.source = EV_NUCLEO, .type = EV_SERIAL, .data.msg = NULL};
   int len = -1;
   int index = 0;
   unsigned char c;

   while (serial_getc_timeout(data->fd, SERIAL_TIMEOUT, &c))
   {
      //clear buffer
   }

   while (!is_quit())
   {
      int r = serial_getc_timeout(data->fd, SERIAL_TIMEOUT, &c);
      if (r > 0) // character has been read
      {
         if (index == 0 && get_message_size(c, &len)) // message recognized
         {
            msg_buf[index++] = c;
         }
         else if (index == 0)
         {
            ERROR("Unknown message type has been received ");
            fprintf(stderr, "0x%x\n - '%c'\r", c, c);
         }
         else
         {
            msg_buf[index++] = c;
         }
      }
      else if (r == 0) //read but nothing has been received
      {
         if (index > 0)
         {
            index = 0;
            WARN("The packet hasn't been received, "
                 "discard what has been read\n\r");
         }
      }
      else
      {
         ERROR("Cannot receive data from the serial port\n");
         set_quit();
      }
      if (len == index) // whole message received
      {
         message *msg = (message *)malloc(sizeof(message));
         if (!msg)
         {
            ERROR("Cannot allocate memory!\n");
            set_quit();
            break;
         }
         if (!parse_message_buf(msg_buf, len, msg))
         {
            ERROR("Cannot parse message type ");
            fprintf(stderr, "%d\n\r", msg_buf[0]);
         }
         ev.data.msg = msg;
         queue_push(ev); // push pointer to the queue
         index = 0;      // reset
      }
   }
   ev.type = EV_QUIT;
   queue_push(ev);
   INFO("Exit serial_rx_thread ");
   fprintf(stderr, "%p \033[1;92mOK\033[0m\n", (void *)pthread_self());
   return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

/* SEND MESSAGE FROM BOSS TO NUCLEO */
bool send_message(int fd, message *msg)
{
   int len;
   uint8_t buf[sizeof(message)];
   uint8_t *msg_buf = buf;
   fill_message_buf(msg, msg_buf, sizeof(message), &len); // marshalling
   int ret = write(fd, msg_buf, len);
   return (ret > 1);
}
