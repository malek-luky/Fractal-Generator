///////////////////////////////////////////////////////////////////////////////
//  NUCLEO PART OF THE APPLICATION
///////////////////////////////////////////////////////////////////////////////
#define VERSION_MAJOR 1
#define VERSION_MINOR 1
#define VERSION_PATCH 0

#include "mbed.h"
#include "message.h"
#include <math.h>
#define BUF_SIZE 255
#define MESSAGE_SIZE (sizeof(message))
DigitalOut led(LED1);
InterruptIn button_event(USER_BUTTON);
Serial serial(SERIAL_TX, SERIAL_RX);

///////////////////////////////////////////////////////////////////////////////
//  DECLARATION
///////////////////////////////////////////////////////////////////////////////

/* STRUCT WITH LOCAL VARIABLES */
static struct
{
    double cx;        // real constant part
    double cy;        // imaginary constant part
    double dx;        // shift in real axes
    double dy;        // shift in imaginary axes
    double sx;        // start of the x-coords (real)
    double sy;        // start of the y-coords (imaginary)
    double px;        // actual position of the x-coords (real)
    double py;        // actual position of the y-coords (imaginary)
    int task_id;      // index of current task, checks if we are done
    uint8_t nx;       // number of cells in x-coords
    uint8_t ny;       // number of cells in y-coords
    uint8_t cid;      // chunk id
    uint8_t max_iter; // maximum number of iterations
    int msg_len;
    float period;
    bool computing;
    bool abort_request;
} nucleo = {
    .task_id = 0,
    .period = 0.2,
    .computing = false,
    .abort_request = false,
};

void blink();
void Tx_interrupt();
void Rx_interrupt();
void button();
bool send_buffer(const uint8_t *msg, int size);
bool receive_message(uint8_t *msg_buf, int size, int *len);
bool fill_message_buf(const message *msg, uint8_t *buf, int size);
void tick();
bool set_compute(message *msg);
void save_values(message *msg);
void compute_iter(message *msg);
void move_cursor(message *msg);
char tx_buffer[BUF_SIZE];
char rx_buffer[BUF_SIZE];
volatile int tx_in = 0; // pointers to the circular buffers
volatile int tx_out = 0;
volatile int rx_in = 0;
volatile int rx_out = 0;
Ticker ticker;
msg_version VERSION = {.major = VERSION_MAJOR,
                       .minor = VERSION_MINOR,
                       .patch = VERSION_PATCH};

///////////////////////////////////////////////////////////////////////////////
//  MAIN
///////////////////////////////////////////////////////////////////////////////
/*
 * INPUT MESSAGE        -> OUTPUT MESSAGE
 * START                -> MSG_STARTUP
 * MSG_GET_VERSION      -> MSG_VERSION
 * MSG_SET_COMPUTE      -> MSG_ERROR / MSG_OK
 * MSG_COMPUTE          -> MSG_ERROR / MSG_OK + MSG_COMPUTE_DATA / MSG_DONE
 * COMPUTING            -> BLINK WITH LED + MSG_COMPUTE_DATA
 * COMPUTATION DONE     -> MSG_DONE
 * MSG_ABORT            -> MSG_OK
 * COMPUTATION ABORTED  -> MSG_ABORT
 * PRESSED USER BUTTON   = MSG_ABORT
 */

int main(void)
{
    /* VARIABLES */
    message msg;
    uint8_t msg_buf[MESSAGE_SIZE];
    led = false; // cant'be in local struct with other variables

    /* INTERRUPTS */
    serial.attach(&Rx_interrupt, Serial::RxIrq); // receive data
    serial.attach(&Tx_interrupt, Serial::TxIrq); // send data
    button_event.rise(&button);
    serial.baud(115200);

    /* CLEAR BUFFER */
    while (serial.readable())
    {
        serial.getc();
    }

    /* START THE PROGRAM */
    for (int i = 0; i < 10; ++i) // blink to say hello
    {
        led = !led;
        wait(0.05);
    }

    /* INIT MESSAGE */
    char welcome_msg[] = "ツツツ";
    memcpy(msg.data.startup.message, welcome_msg, strlen(welcome_msg));
    msg.type = MSG_STARTUP;
    fill_message_buf(&msg, msg_buf, MESSAGE_SIZE, &nucleo.msg_len);
    send_buffer(msg_buf, nucleo.msg_len);

    /* MAIN LOOP */
    while (1)
    {
        if (nucleo.abort_request)
        {
            if (nucleo.computing) //abort computing
            {
                msg.type = MSG_ABORT;
                fill_message_buf(&msg, msg_buf, MESSAGE_SIZE, &nucleo.msg_len);
                send_buffer(msg_buf, nucleo.msg_len);
                nucleo.computing = false;
                nucleo.abort_request = false;
                ticker.detach();
                led = false;
            }
            else
            {
                nucleo.abort_request = false;
            }
        }

        if (rx_in != rx_out) // something is in the receive buffer
        {
            if (receive_message(msg_buf, MESSAGE_SIZE,
                                &nucleo.msg_len)) //is it whole message?
            {
                if (parse_message_buf(msg_buf, nucleo.msg_len,
                                      &msg)) //decodes what was received
                {
                    switch (msg.type)
                    {
                    case MSG_GET_VERSION:
                        msg.type = MSG_VERSION;
                        msg.data.version = VERSION;
                        fill_message_buf(&msg, msg_buf, MESSAGE_SIZE,
                                         &nucleo.msg_len);
                        send_buffer(msg_buf, nucleo.msg_len);
                        break;
                    case MSG_ABORT:
                        msg.type = MSG_OK;
                        nucleo.task_id = 0;
                        fill_message_buf(&msg, msg_buf, MESSAGE_SIZE,
                                         &nucleo.msg_len);
                        send_buffer(msg_buf, nucleo.msg_len);
                        nucleo.abort_request = true;
                        ticker.detach();
                        led = false;
                        break;
                    case MSG_SET_COMPUTE:
                        nucleo.task_id = 0;
                        set_compute(&msg) ? msg.type = MSG_OK
                                          : msg.type = MSG_ERROR;
                        fill_message_buf(&msg, msg_buf, MESSAGE_SIZE,
                                         &nucleo.msg_len);
                        send_buffer(msg_buf, nucleo.msg_len);
                        break;
                    case MSG_COMPUTE:
                        save_values(&msg);
                        nucleo.computing = true;
                        ticker.attach(tick, nucleo.period);
                        msg.type = MSG_OK;
                        fill_message_buf(&msg, msg_buf, MESSAGE_SIZE,
                                         &nucleo.msg_len);
                        send_buffer(msg_buf, nucleo.msg_len);
                        break;
                    default:
                        msg.type = MSG_ERROR;
                        fill_message_buf(&msg, msg_buf, MESSAGE_SIZE,
                                         &nucleo.msg_len);
                        send_buffer(msg_buf, nucleo.msg_len);
                        break;
                    }
                }
                else // message has not been parsed, send error
                {
                    msg.type = MSG_ERROR;
                    fill_message_buf(&msg, msg_buf, MESSAGE_SIZE,
                                     &nucleo.msg_len);
                    send_buffer(msg_buf, nucleo.msg_len);
                }
            }
        }
        else if (nucleo.computing && !nucleo.abort_request)
        {
            if (nucleo.task_id < nucleo.nx * nucleo.ny)
            {
                compute_iter(&msg);
                nucleo.task_id++;
                move_cursor(&msg);
                msg.type = MSG_COMPUTE_DATA;
                fill_message_buf(&msg, msg_buf, MESSAGE_SIZE,
                                 &nucleo.msg_len);
                send_buffer(msg_buf, nucleo.msg_len);
            }
            else //computation done
            {
                nucleo.task_id = 0;
                msg.type = MSG_DONE;
                ticker.detach();
                led = 0;
                fill_message_buf(&msg, msg_buf, MESSAGE_SIZE, &nucleo.msg_len);
                send_buffer(msg_buf, nucleo.msg_len);
                nucleo.computing = false;
            }
        }
        else
        {
            sleep(); // put the cpu to sleep mode, wakeup on interrupt
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//  IRQ
///////////////////////////////////////////////////////////////////////////////

/* WHEN WE PRESS THE BUTTON WE ABORT THE CURRENT CALCUALTIONS */
void button()
{
    nucleo.abort_request = true;
}

/* TURN THE LED ON OR OFF */
void tick()
{
    led = !led;
}

/* SEND A SINGLE BYTE AS THE INTERRUPT IS TRIGGERED ON EMPTY OUT BUFFER */
void Tx_interrupt()
{
    if (tx_in != tx_out)
    {
        serial.putc(tx_buffer[tx_out]);
        tx_out = (tx_out + 1) % BUF_SIZE;
    }
    else
    {                                    // buffer sent out
        USART2->CR1 &= ~USART_CR1_TXEIE; // disable Tx interrupt
    }
    return;
}

/* RECEIVE BYTES AND STOP IF RX_BUFFER IS FULL */
void Rx_interrupt()
{
    while ((serial.readable()) && (((rx_in + 1) % BUF_SIZE) != rx_out))
    {
        rx_buffer[rx_in] = serial.getc();
        rx_in = (rx_in + 1) % BUF_SIZE;
    }
    return;
}

/* WHEN THE BUFFER IS FULL WE NEED TO FREE THE BUFFER AND SEND THE CONTENT */
bool send_buffer(const uint8_t *msg, int size)
{
    if (!msg && size == 0) //size must be > 0
    {
        return false;
    }
    int i = 0;
    NVIC_DisableIRQ(USART2_IRQn);   // start critical section, dissable IRQ
    USART2->CR1 |= USART_CR1_TXEIE; // enable Tx interrupt on empty out buffer
    while ((i == 0) || i < size)    // end reading when message has been read
    {
        if (((tx_in + 1) % BUF_SIZE) == tx_out) // needs buffer space
        {
            NVIC_EnableIRQ(USART2_IRQn); // enable interrupts to send buffer
            while (((tx_in + 1) % BUF_SIZE) == tx_out)
            {
                // let interrupt routine empty the buffer
            }
            NVIC_DisableIRQ(USART2_IRQn); // disable interrupts
        }
        tx_buffer[tx_in] = msg[i];
        i += 1;
        tx_in = (tx_in + 1) % BUF_SIZE;
    }                               // buffer has been put to tx buffer
    USART2->CR1 |= USART_CR1_TXEIE; // enable Tx interrupt for sending it out
    NVIC_EnableIRQ(USART2_IRQn);    // end critical section
    return true;
}

/* MESSAGE RECEIVE AND UNMARSHALLING WHAT WAS SENT, RETURNS TRUE ON SUCCESS */
bool receive_message(uint8_t *msg_buf, int size, int *len)
{
    bool ret = false;
    int i = 0;                                    // possition in msg we decode
    *len = 0;                                     // message size
    NVIC_DisableIRQ(USART2_IRQn);                 // start critical section
    while (((i == 0) || (i != *len)) && i < size) //start to empty buffer
    {
        if (rx_in == rx_out)
        {                                // wait if buffer is empty
            NVIC_EnableIRQ(USART2_IRQn); // enable IRQ for receiving buffer
            while (rx_in == rx_out)      // wait to next character
            {
            }
            NVIC_DisableIRQ(USART2_IRQn); // disable interrupts
        }
        uint8_t c = rx_buffer[rx_out];
        if (i == 0) // the first byte is checked and it indicates message type
        {
            if (get_message_size(c, len)) // message type recognized
            {
                msg_buf[i++] = c;
                ret = *len <= size; // msg_buffer must be large enough
            }
            else
            {
                ret = false;
                break; // unknown message
            }
        }
        else // now we know what message it is and we copy it to a msg_buffer
        {
            msg_buf[i++] = c;
        }
        rx_out = (rx_out + 1) % BUF_SIZE;
    }
    NVIC_EnableIRQ(USART2_IRQn); // end critical section
    return ret;
}

/* SET THE VALUES WE WANT TO COMPUTE */
bool set_compute(message *msg)
{
    bool ret = !nucleo.computing;
    if (ret)
    {
        nucleo.cx = msg->data.set_compute.c_re;
        nucleo.cy = msg->data.set_compute.c_im;
        nucleo.dx = msg->data.set_compute.d_re;
        nucleo.dy = msg->data.set_compute.d_im;
        nucleo.max_iter = msg->data.set_compute.n;
    }
    return ret;
}

/* SAVE THE INPUT VALUES */
void save_values(message *msg)
{
    nucleo.computing = true;
    nucleo.sx = nucleo.px = msg->data.compute.re;
    nucleo.sy = nucleo.py = msg->data.compute.im;
    nucleo.nx = msg->data.compute.n_re;
    nucleo.ny = msg->data.compute.n_im;
    nucleo.cid = msg->data.compute.cid;
}

/* RETURN THE POSSITIONS AND RIGHT ITERATION NUMBER */
void compute_iter(message *msg)
{
    uint8_t ret = 0;
    while (ret <= nucleo.max_iter &&
           (nucleo.px * nucleo.px + nucleo.py * nucleo.py) < 4)
    {
        double temp = nucleo.px * nucleo.px - nucleo.py * nucleo.py + nucleo.cx;
        nucleo.py = 2 * nucleo.px * nucleo.py + nucleo.cy;
        nucleo.px = temp;
        ret++;
    }
    msg->data.compute_data.cid = nucleo.cid; // this will be send to boss
    msg->data.compute_data.i_re = nucleo.task_id % nucleo.nx;
    msg->data.compute_data.i_im = nucleo.task_id / nucleo.nx;
    msg->data.compute_data.iter = ret;
}

/* MOVE WITH THE SX AND SY VALUES TO NEXT POINT */
void move_cursor(message *msg)
{
    nucleo.px = nucleo.sx + nucleo.task_id % nucleo.nx * nucleo.dx;
    nucleo.py = nucleo.sy + nucleo.task_id / nucleo.nx * nucleo.dy;
}
