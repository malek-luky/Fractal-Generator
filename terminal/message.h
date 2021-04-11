///////////////////////////////////////////////////////////////////////////////
//  COMMUNICATION MESSAGES
///////////////////////////////////////////////////////////////////////////////

#ifndef __MESSAGE_H__ // prevents opening header file by multiple modules
#define __MESSAGE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define STARTUP_MSG_LEN 9 //magic number

   /* TYPES OF MESSAGES */
   typedef enum
   {
      MSG_OK,           // acknowledge of the received message
      MSG_ERROR,        // report error on the previously received command
      MSG_ABORT,        // abort received from user button or from serial port
      MSG_DONE,         // report that the requested work has been done
      MSG_GET_VERSION,  // request version of the firmware
      MSG_VERSION,      // send firmware version as major,minor and patch level
      MSG_STARTUP,      // startup message up to 8 bytes long sent by nucleo
      MSG_SET_COMPUTE,  // set computation parameters
      MSG_COMPUTE,      // request computation (chunk_id, nbr_tasks)
      MSG_COMPUTE_DATA, // computed result (chunk_id, result)
      MSG_NBR           // number of messages
   } message_type;

   /* MESSAGE VERSION */
   typedef struct
   {
      uint8_t major;
      uint8_t minor;
      uint8_t patch;
   } msg_version;

   /* STARTUP MESSAGE */
   typedef struct
   {
      uint8_t message[STARTUP_MSG_LEN];
   } msg_startup;

   /* MESSAGE THAT SET THE COMPUTATION VALUES */
   typedef struct
   {
      double c_re; // re (x) part of the c constant in recursive equation
      double c_im; // im (y) part of the c constant in recursive equation
      double d_re; // increment in the x-coords
      double d_im; // increment in the y-coords
      uint8_t n;   // number of iterations per each pixel
   } msg_set_compute;

   /* WE CAN'T CALCULATE THE WHOLE PICTURE AT ONCE, THIS SEND SMALL PARTS */
   typedef struct
   {
      uint8_t cid;  // chunk id
      double re;    // start of the x-coords (real)
      double im;    // start of the y-coords (imaginary)
      uint8_t n_re; // number of cells in x-coords
      uint8_t n_im; // number of cells in y-coords
   } msg_compute;

   /* COMPUTATION RESULT */
   typedef struct
   {
      uint8_t cid;  // chunk id
      uint8_t i_re; // x-coords
      uint8_t i_im; // y-coords
      uint8_t iter; // number of iterations
   } msg_compute_data;

   /* MESSAGE ALL IN ONE */
   typedef struct
   {
      uint8_t type; // message type
      union {
         msg_version version;
         msg_startup startup;
         msg_set_compute set_compute;
         msg_compute compute;
         msg_compute_data compute_data;
      } data;
      uint8_t cksum; // message command
   } message;

   /* FUNCTIONS DECLARATION */
   bool get_message_size(uint8_t msg_type, int *size);
   bool fill_message_buf(const message *msg, uint8_t *buf, int size, int *len);
   bool parse_message_buf(const uint8_t *buf, int size, message *msg);

#ifdef __cplusplus
}
#endif

#endif
