///////////////////////////////////////////////////////////////////////////////
//  FUNCTIONS FOR VISUALIZING THE FRACTAL IN GUI
///////////////////////////////////////////////////////////////////////////////

#ifndef __XWIN_SDL_H__
#define __XWIN_SDL_H__

int xwin_init(int w, int h);
void xwin_close();
void xwin_redraw(int w, int h, unsigned char *img);
void xwin_poll_events(void);
void save_image_png();
void save_image_bmp();
void save_image_jpg();

#endif
