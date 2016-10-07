/*
    libcamera, a camera access abstraction library
    Copyright (C) 2016  alicia@ion.nu

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "camera.h"
typedef struct CAM_t
{
  unsigned int type;
  Display* display;
  int screen;
  Window rootwindow;
  int width;
  int height;
} CAM;

char** cam_list_x11(char** list, unsigned int* count)
{
  ++*count;
  list=realloc(list, sizeof(char*)*(*count));
  list[(*count)-1]=strdup("X11");
  return list;
}

CAM* cam_open_x11(void) // const char* name)
{
  Display* display=XOpenDisplay(getenv("DISPLAY"));
  if(!display){return 0;}
  CAM* cam=malloc(sizeof(CAM));
  cam->type=CAMTYPE_X11;
  cam->display=display;
  cam->screen=XDefaultScreen(display);
  cam->width=DisplayWidth(cam->display, cam->screen);
  cam->height=DisplayHeight(cam->display, cam->screen);
  cam->rootwindow=RootWindow(cam->display, cam->screen);
  return cam;
}

void cam_resolution_x11(CAM* cam, unsigned int* width, unsigned int* height)
{
  *width=cam->width;
  *height=cam->height;
}

void cam_getframe_x11(CAM* cam, void* pixmap)
{
  XImage* img=XGetImage(cam->display, cam->rootwindow, 0, 0, cam->width, cam->height, AllPlanes, ZPixmap);
  unsigned int bpp=img->bits_per_pixel/8;
  unsigned int x, y;
  for(y=0; y<cam->height; ++y)
  {
    for(x=0; x<cam->width; ++x)
    {
      if(img->blue_mask>img->red_mask)
      {
        memcpy(pixmap+(y*cam->width+x)*3, img->data+y*img->bytes_per_line+x*bpp, 3);
      }else{
        memcpy(pixmap+(y*cam->width+x)*3+2, img->data+y*img->bytes_per_line+x*bpp, 1);
        memcpy(pixmap+(y*cam->width+x)*3+1, img->data+y*img->bytes_per_line+x*bpp+1, 1);
        memcpy(pixmap+(y*cam->width+x)*3, img->data+y*img->bytes_per_line+x*bpp+2, 1);
      }
    }
  }
  img->f.destroy_image(img);
}

void cam_close_x11(CAM* cam)
{
  XCloseDisplay(cam->display);
  free(cam);
}
