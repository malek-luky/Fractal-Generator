///////////////////////////////////////////////////////////////////////////////
//  MATHEMATICAL BASE WHICH PERFORMS FRACTAL CALCULATION
///////////////////////////////////////////////////////////////////////////////

#include "computation.h"
#include "message.h"
#include "my_functions.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/* STRUCT HOLDING ALL VARIABLES NEEDED HERE */
static struct
{
	double c_re;			   // constants in real axis
	double c_im;			   // constants in imaginary axis
	int n;					   // number of iterations
	double range_re_min;	   // picture min_range in x-coords
	double range_re_max;	   // picture max_range in x-coords
	double range_im_min;	   // picture min_range in y-coords
	double range_im_max;	   // picture max_range in y-coords
	int grid_w;				   // resolution - width
	int grid_h;				   // resolution - width
	int cur_x;				   // actual position in the image
	int cur_y;				   // actual position in the image
	double d_re;			   // shift in real axis per one iteration
	double d_im;			   // shift in imaginary axis
	uint8_t nbr_chunks;		   // number of chunk - picuture is split to blocks
	uint8_t cid;			   // chunk id
	double chunk_re;		   // chunk left top corner
	double chunk_im;		   // chunk left top corner
	uint8_t chunk_n_re;		   // number of pixels in chunk in real axes
	uint8_t chunk_n_im;		   // number of pixels in chunk in imagianry axes
	uint8_t *grid;			   // grid array containing number of iterrations
	uint8_t *grid_computation; // necessary for 'p', stores only current comp.
	bool computing;			   // contains if we are computing or not
	bool done;				   // true when the current computation is done
	bool abort;				   // abort from keyboard or nucleo interrupt
} comp =					   //default values
	{.c_re = -0.4,
	 .c_im = 0.6,
	 .n = 60,
	 .range_re_min = -1.6,
	 .range_re_max = 1.6,
	 .range_im_min = -1.1,
	 .range_im_max = 1.1,
	 .grid_w = 640,
	 .grid_h = 480,
	 .chunk_n_re = 64,
	 .chunk_n_im = 48,
	 .grid = NULL,
	 .grid_computation = NULL,
	 .computing = false,
	 .done = false,
	 .abort = false};

/* INITIALIZE THE COMPUTATION */
void computation_init(void)
{
	comp.grid = my_alloc(comp.grid_w * comp.grid_h);
	comp.grid_computation = my_alloc(comp.grid_w * comp.grid_h);
	comp.d_re = (comp.range_re_max - comp.range_re_min) / (1. * comp.grid_w);
	comp.d_im = -(comp.range_im_max - comp.range_im_min) / (1. * comp.grid_h);
	comp.nbr_chunks = (comp.grid_w * comp.grid_h) /
					  (comp.chunk_n_re * comp.chunk_n_im);
}

/* CLEANUP ALL STORED DATA AFTER THE COMPUTATION */
void computation_cleanup(void)
{
	if (comp.grid)
	{
		free(comp.grid);
		free(comp.grid_computation);
	}
	comp.grid = NULL;
}

/* RETURN TRUE IF COMPUTING */
bool is_computing(void)
{
	return comp.computing;
}

/* RETURN TRUE IF COMPUTATION IS TOTALLY DONE */
bool is_done(void)
{
	return comp.done;
}

/* CANCEL THE COMPUTATION ABORT */
void enable_comp(void)
{
	comp.abort = false;
}

/* SET THE VALUES WE WANT TO COMPUTE */
bool set_compute(message *msg)
{
	memset(comp.grid_computation, 0,
		   comp.grid_w * comp.grid_h * sizeof(uint8_t)); // used for p cmd

	bool ret = !is_computing();
	if (ret)
	{
		msg->type = MSG_SET_COMPUTE;
		msg->data.set_compute.c_re = comp.c_re;
		msg->data.set_compute.c_im = comp.c_im;
		msg->data.set_compute.d_re = comp.d_re;
		msg->data.set_compute.d_im = comp.d_im;
		msg->data.set_compute.n = comp.n;
		comp.done = false;
	}
	return ret;
}

/* SEND THE CURRENT CHUNK POSITION TO NUCLEO TO HOLD THE CALCULATION */
void compute(message *msg)
{
	my_assert(msg != NULL, __func__, __LINE__, __FILE__);
	if (!is_computing()) //first chunk
	{
		comp.cid = 0;
		comp.computing = true;
		comp.cur_x = comp.cur_y = 0;
		comp.chunk_re = comp.range_re_min; //left
		comp.chunk_im = comp.range_im_max; //up
		msg->type = MSG_COMPUTE;
	}
	else //next chunks
	{
		comp.cid += 1;
		if (comp.cid < comp.nbr_chunks)
		{
			comp.cur_x += comp.chunk_n_re;
			comp.chunk_re += comp.chunk_n_re * comp.d_re;
			if (comp.cur_x >= comp.grid_w)
			{
				comp.cur_x = 0;
				comp.chunk_re = comp.range_re_min;
				comp.chunk_im += comp.chunk_n_im * comp.d_im;
				comp.cur_y += comp.chunk_n_im;
			}
			msg->type = MSG_COMPUTE;
		}
	}
	if (comp.computing && msg->type == MSG_COMPUTE) //calculation saved to msg
	{
		msg->data.compute.cid = comp.cid;
		msg->data.compute.re = comp.chunk_re;
		msg->data.compute.im = comp.chunk_im;
		msg->data.compute.n_re = comp.chunk_n_re;
		msg->data.compute.n_im = comp.chunk_n_im;
	}
}

/* RETURN TRUE IF THE COMPUTATION IS ABORTED */
bool is_abort(void)
{
	return comp.abort;
}

/* UPDATE STRUCT WITH ACTUAL VALUES AND OCASIONALLY STOPS THE CALCULATION */
void update_data(const msg_compute_data *compute_data)
{
	my_assert(compute_data != NULL, __func__, __LINE__, __FILE__);
	if (compute_data->cid == comp.cid)
	{
		const int idx = comp.cur_x + compute_data->i_re +
						(comp.cur_y + compute_data->i_im) * comp.grid_w;
		if (idx > 0 && idx < comp.grid_h * comp.grid_w)
		{
			comp.grid[idx] = compute_data->iter;
			comp.grid_computation[idx] = compute_data->iter;
		}
		if ((comp.cid + 1) >= comp.nbr_chunks &&
			(compute_data->i_re + 1) == comp.chunk_n_re &&
			(compute_data->i_im + 1) == comp.chunk_n_im)
		{
			comp.done = true;
			comp.computing = false;
		}
	}
	else
	{
		ERROR("Recieved chunk with unexpected chunkid\n");
	}
}

/* UPDATES THE RGB IMAGE VALUES */
void update_image(int w, int h, unsigned char *img)
{
	my_assert(img && comp.grid && w == comp.grid_w && h == comp.grid_h,
			  __func__, __LINE__, __FILE__);
	for (int i = 0; i < w * h; ++i)
	{
		const double t = 1. * comp.grid[i] / (comp.n + 1.0);
		*(img++) = 9 * (1 - t) * t * t * t * 255;				//R
		*(img++) = 15 * (1 - t) * (1 - t) * t * t * 255;		//G
		*(img++) = 8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255; //B
	}
}

/* SET THE COMPUTATION ABORT ON TRUE */
void abort_comp(void)
{
	memset(comp.grid_computation, 0,
		   comp.grid_w * comp.grid_h * sizeof(uint8_t)); // used for p cmd
	comp.computing = false;
	comp.abort = true;
}

/* SAVE PICTURE RESOLUTION TO VARIABLES  */
void get_grid_size(int *width, int *height)
{
	*width = comp.grid_w;
	*height = comp.grid_h;
}

/* RETURNS PICTURE WIDTH */
int grid_width()
{
	return comp.grid_w;
}

/* RETURN PICTURE HEIGHT */
int grid_height()
{
	return comp.grid_h;
}

/* RETURN THE NUMBER OF CHUNKS */
int number_of_chunks()
{
	return comp.nbr_chunks;
}

/* RETURN THE POSITION ON REAL AXES */
int cursor_width()
{
	return comp.cur_x;
}

/* RETURN THE POSITION ON IMAGINARY AXES */
int cursor_height()
{
	return comp.cur_y;
}

/* RETURN TRUE IF THE USER INPUT IS DIVISIBLE - MODULO IS ZERO */
bool correct_input()
{
	return (comp.grid_w % comp.chunk_n_re == 0 &&
			comp.grid_h % comp.chunk_n_im == 0);
}

/* COMPUTE THE NUMBER OF ITERATIONS FOR PIXEL PX PY */
uint8_t compute_iter(double cx, double cy, double px,
					 double py, uint8_t max_iteration)
{
	uint8_t ret = 0;
	while (ret <= max_iteration && sqrt(px * px + py * py) < 2)
	{
		double temp = px * px - py * py + cx;
		py = 2 * px * py + cy;
		px = temp;
		ret++;
	}
	return ret;
}

/* CALCULATES THE FRACTAL USING CPU */
void compute_cpu()
{
	int i = 0;
	double py = comp.range_im_max;
	for (int height = 0; height < comp.grid_h; height++)
	{
		double px = comp.range_re_min;
		py += comp.d_im;
		for (int width = 0; width < comp.grid_w; width++)
		{
			px += comp.d_re;
			comp.grid[i++] = compute_iter(comp.c_re, comp.c_im, px, py, comp.n);
		}
	}
}

/* INCREASE THE PARAMETER DURING COMPUTATION */
void increase_parameter(msg_set_compute *set_compute)
{
	comp.c_re += 0.1;
	comp.c_im += 0.1;
	set_compute->c_re = comp.c_re;
	set_compute->c_im = comp.c_im;
}

/* DECREASE THE PARAMETER DURING COMPUTATION */
void decrease_parameter(msg_set_compute *set_compute)
{
	comp.c_re -= 0.1;
	comp.c_im -= 0.1;
	set_compute->c_re = comp.c_re;
	set_compute->c_im = comp.c_im;
}

/* DECREASE THE CONSTANT TO END ANIMATION AT DEFAULT VALUE FROM SETTINGS */
void set_up_animation(msg_set_compute *set_compute, int animation_frames)
{
	comp.c_re -= animation_frames * 0.005;
	comp.c_im -= animation_frames * 0.005;
}

/* DECREASE LOCAL VALUE OF CONSTANT TO ANIAMTE THE FRACTAL */
void animate()
{
	comp.c_re += 0.005;
	comp.c_im += 0.005;
	usleep(150); //it is too quick when the resolution is small
}

/* CLEAR THE CURRENT GRID COMPUTATION */
void clear_grid()
{
	memset(comp.grid, 0, comp.grid_w * comp.grid_h * sizeof(uint8_t));
}

/* COPY THE CURRENT CALCULATION TO DEFAULT GRID WHICH IS SHOWN IN SDL */
void update_grid()
{
	for (int i = 0; i < comp.grid_w * comp.grid_h; i++)
	{
		comp.grid[i] = comp.grid_computation[i];
	}
}

///////////////////////////////////////////////////////////////////////////////
//  INTERACTIVE SETTINGS
///////////////////////////////////////////////////////////////////////////////
void change_settings(char c)
{
	switch (c)
	{
	case 65: // uparrow
		comp.grid_h += 10;
		break;
	case 66: // downarrow
		comp.grid_h -= 10;
		comp.grid_h = MAX(comp.grid_h, 10);
		break;
	case 67: // rightarrow
		comp.grid_w += 10;
		break;
	case 68: // leftarrow
		comp.grid_w -= 10;
		comp.grid_w = MAX(comp.grid_w, 10);
		break;
	case '8':
		comp.chunk_n_im += 1;
		break;
	case '5':
		comp.chunk_n_im -= 1;
		comp.chunk_n_im = MAX(comp.chunk_n_im, 1);
		break;
	case '6':
		comp.chunk_n_re += 1;
		break;
	case '4':
		comp.chunk_n_re -= 1;
		comp.chunk_n_re = MAX(comp.chunk_n_re, 1);
		break;
	case '+':
		comp.n++;
		break;
	case '-':
		comp.n--;
		comp.n = MAX(comp.n, 1);
		break;
	case 'k':
		comp.c_re -= 0.1;
		break;
	case 'o':
		comp.c_re += 0.1;
		break;
	case 'j':
		comp.c_im -= 0.1;
		break;
	case 'i':
		comp.c_im += 0.1;
		break;
	case '1':
		comp.range_re_min -= 0.1;
		break;
	case '3':
		comp.range_re_min += 0.1;
		comp.range_re_min = MIN(comp.range_re_max, comp.range_re_min);
		break;
	case '7':
		comp.range_re_max -= 0.1;
		comp.range_re_max = MAX(comp.range_re_max, comp.range_re_min);
		break;
	case '9':
		comp.range_re_max += 0.1;
		break;
	case 'h':
		comp.range_im_min -= 0.1;
		break;
	case 'u':
		comp.range_im_min += 0.1;
		comp.range_im_min = MIN(comp.range_im_max, comp.range_im_min);
		break;
	case 'f':
		comp.range_im_max -= 0.1;
		comp.range_im_max = MAX(comp.range_im_max, comp.range_im_min);
		break;
	case 't':
		comp.range_im_max += 0.1;
		break;
	case 'q':
		call_termios(1); // cooked mode - restore terminal settings
		exit(0);
		break;
	default: // discard all other keys
		break;
	}
}

/* REDRAW THE BOTTOM OF THE GUI INTERFACE */
void print_changed_settings()
{

	printf("\033[14A");
	printf(
		"║ ACTIVE SETTINGS:                                               ║\n"
		"║ resolution:                         %-4d x %-4d                ║\n",
		comp.grid_w,
		comp.grid_h);

	if (comp.grid_w % comp.chunk_n_re == 0 && comp.grid_h % comp.chunk_n_im == 0)
	{
		printf(
			"║ chunk size:                         %-3d x %-3d                  ║\n",
			comp.chunk_n_re,
			comp.chunk_n_im);
	}
	else //print red if the input isn't correct
	{
		printf(
			"║ chunk size:                         \033[1;31m%-3d x %-3d\033[0m  "
			"                ║\n",
			comp.chunk_n_re,
			comp.chunk_n_im);
	}

	printf(
		"║ number of iterations:               %-3d                        ║\n"
		"║ parameter real part:               %+-3.1f                        ║\n"
		"║ parameter imaginary  part:         %+-3.1f i                      ║\n"
		"║ range of real axes:                %+-3.1f -> %+-3.1f                ║\n"
		"║ range of imaginary axes:           %+-3.1f -> %+-3.1f                ║\n"
		"║ download image:                     yes                        ║\n"
		"║                                                                ║\n"
		"║                                                                ║\n"
		"║            PRESS ENTER TO SAVE AND EXIT SETTINGS               ║\n"
		"╚════════════════════════════════════════════════════════════════╝\n\n",
		comp.n,
		comp.c_re,
		comp.c_im,
		comp.range_re_min,
		comp.range_re_max,
		comp.range_im_min,
		comp.range_im_max);
}
