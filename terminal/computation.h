///////////////////////////////////////////////////////////////////////////////
//  MATHEMATICAL BASE WHICH PERFORMS FRACTAL CALCULATION
///////////////////////////////////////////////////////////////////////////////

#ifndef __COMPUTATION_H__
#define __COMPUTATION_H__

#include <stdbool.h>
#include "message.h"

void abort_comp(void);
void enable_comp(void);
bool is_computing(void);
bool is_done(void);
void computation_init(void);
void computation_cleanup(void);
bool set_compute(message *msg);
void compute(message *msg);
bool is_abort(void);
void get_grid_size(int *width, int *height);
int grid_width();
int grid_height();
int number_of_chunks();
void update_data(const msg_compute_data *compute_data);
void update_image(int w, int h, unsigned char *img);
int cursor_height();
int cursor_width();
uint8_t compute_iter(double cx, double cy, double px, double py, uint8_t max_iteration);
void compute_cpu();
void decrease_parameter(msg_set_compute *set_compute);
void increase_parameter(msg_set_compute *set_compute);
void change_settings(char c);
void print_changed_settings();
bool correct_input();
void set_up_animation(msg_set_compute *set_compute, int animation_frames);
void animate();
void clear_grid();
void update_grid();

#endif
