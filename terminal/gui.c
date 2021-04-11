///////////////////////////////////////////////////////////////////////////////
//  CREATES THE GRAPHICAL OUTPUT
///////////////////////////////////////////////////////////////////////////////

#include "gui.h"
#include "xwin_sdl.h"
#include "my_functions.h"
#include "computation.h"
#include <SDL.h>
#include "event_queue.h"
#define SDL_EVENT_POLL_WAIT_MS 10

/* CONTAINS WIDTH, HEIGHT AND POINTER TO DATA WITH IMAGE */
static struct
{
	int w;
	int h;
	unsigned char *img;
} gui = {.img = NULL};

/* INITIALIZE THE WINDOW WHERE THE FRACTAL WILL BE LOADED */
void gui_init(void)
{
	get_grid_size(&gui.w, &gui.h);
	gui.img = my_alloc(gui.w * gui.h * 3);
	my_assert(xwin_init(gui.w, gui.h) == 0, __func__, __LINE__, __FILE__);
}

/* FREE THE ALLOCATED MEMORY FOR IMAGE */
void gui_cleanup(void)
{
	if (gui.img)
	{
		free(gui.img);
		gui.img = NULL;
	}
	xwin_close();
}

/* UPDATE THE IMAGE DATA AND REDRAW THE FRACTAL IN GUI */
void gui_refresh(void)
{
	if (gui.img)
	{
		update_image(gui.w, gui.h, gui.img);
		xwin_redraw(gui.w, gui.h, gui.img);
		xwin_poll_events();
	}
}

/* PRINT THE WELCOME SCREEN WITH SETTINGS */
void print_gui(void)
{
	printf(
		"╔════════════════════════════════════════════════════════════════╗\n"
		"║                  WELCOME TO FRACTAL CALCULATOR                 ║\n"
		"║  ────────────────────────────────────────────────────────────  ║\n"
		"║ KEYBOARD SHORTCUTS:                                            ║\n"
		"║ g - requests the firmware version number of Nucleo program     ║\n"
		"║ s - prepare the calculation                                    ║\n"
		"║ 1 - start calculation                                          ║\n"
		"║ a - abort the current calculation                              ║\n"
		"║ r - resets the cid                                             ║\n"
		"║ l - clear the calculation buffer                               ║\n"
		"║ p - redraws the contents of the window with the current buffer ║\n"
		"║ c - compute fractal on PC                                      ║\n"
		"║ m - animate fractal                                            ║\n"
		"║ q - terminate threads and close the program                    ║\n"
		"║ + - increase the paramer c if it is not computing              ║\n"
		"║ - - decrease the paramer c if it is not computing              ║\n"
		"║                                                                ║\n"
		"║ INTERACTIVE SHORTCUTS:                                         ║\n"
		"║ ←/→/↓/↑  adjust resolution                                     ║\n"
		"║ 8/6/5/4  adjust chunk size                                     ║\n"
		"║ -/+      decrease / increase number of iterations              ║\n"
		"║ k/o      decrease / increase real part of parameter            ║\n"
		"║ j/i      decrease / increase imaginary part of parameter       ║\n"
		"║ 1/3      decrease / increase real axes min computation range   ║\n"
		"║ 7/9      decrease / increase real axes max computation range   ║\n"
		"║ h/u      decrease / increase imaginary min axes range          ║\n"
		"║ f/t      decrease / increase imaginary max axes range          ║\n"
		"║ y/n      enable / disable image download                       ║\n"
		"║                                                                ║\n"
		"║ ACTIVE SETTINGS:                                               ║\n"
		"║ resolution:                         640  x 480                 ║\n"
		"║ chunk size:                         64  x 48                   ║\n"
		"║ number of iterations:               60                         ║\n"
		"║ parameter real part:               -0.4                        ║\n"
		"║ parameter imaginary  part:         +0.6 i                      ║\n"
		"║ range of real axes:                -1.6 -> +1.6                ║\n"
		"║ range of imaginary axes:           -1.1 -> +1.1                ║\n"
		"║ download image:                     yes                        ║\n"
		"║                                                                ║\n"
		"║                                                                ║\n"
		"║            PRESS ENTER TO SAVE AND EXIT SETTINGS               ║\n"
		"╚════════════════════════════════════════════════════════════════╝\n\n");
}
