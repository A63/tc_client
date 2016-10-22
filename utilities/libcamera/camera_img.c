/*
    libcamera, a camera access abstraction library
    Copyright (C) 2015-2016  alicia@ion.nu

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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "camera.h"

struct CAM_t
{
  unsigned int type;
  GdkPixbufAnimation* anim;
  GdkPixbuf* staticimg;
  GdkPixbufAnimationIter* iter;
};

const char*(*cam_img_filepicker)(void)=0;

void cam_img_fixpixels_raw(unsigned char* pixels, unsigned char* from, unsigned int pixelcount)
{
  unsigned int i;
  for(i=0; i<pixelcount; ++i)
  {
    memmove(&pixels[i*3], &from[i*4], 3);
  }
}

void cam_img_fixpixels(GdkPixbuf* img)
{
  if(gdk_pixbuf_get_n_channels(img)==4)
  {
    unsigned char* pixels=gdk_pixbuf_get_pixels(img);
    unsigned int pixelcount=gdk_pixbuf_get_width(img)*gdk_pixbuf_get_height(img);
    cam_img_fixpixels_raw(pixels, pixels, pixelcount);
  }
}

char** cam_list_img(char** list, unsigned int* count)
{
  if(!cam_img_filepicker){return list;} // Don't give the option if we don't have the required callback (toolkit-agnostic)
  ++*count;
  list=realloc(list, sizeof(char*)*(*count));
  list[(*count)-1]=strdup("Image");
  return list;
}

CAM* cam_open_img(void)
{
  if(!cam_img_filepicker){return 0;}
  const char* file=cam_img_filepicker();
  if(!file){return 0;}
  CAM* cam=malloc(sizeof(CAM));
  cam->type=CAMTYPE_IMG;
  cam->anim=gdk_pixbuf_animation_new_from_file(file, 0);
  if(gdk_pixbuf_animation_is_static_image(cam->anim))
  {
    cam->staticimg=gdk_pixbuf_animation_get_static_image(cam->anim);
    cam_img_fixpixels(cam->staticimg);
    cam->iter=0;
  }else{
    cam->staticimg=0;
    cam->iter=gdk_pixbuf_animation_get_iter(cam->anim, 0);
  }
  return cam;
}

void cam_resolution_img(CAM* cam, unsigned int* width, unsigned int* height)
{
  // TODO: Implement scaling of images?
  GdkPixbuf* img;
  if(cam->staticimg){img=cam->staticimg;}
  else{img=gdk_pixbuf_animation_iter_get_pixbuf(cam->iter);}
  *width=gdk_pixbuf_get_width(img);
  *height=gdk_pixbuf_get_height(img);
}

void cam_getframe_img(CAM* cam, void* pixmap)
{
  GdkPixbuf* img;
  if(cam->staticimg)
  {
    img=cam->staticimg;
  }else{
    gdk_pixbuf_animation_iter_advance(cam->iter, 0);
    img=gdk_pixbuf_animation_iter_get_pixbuf(cam->iter);
  }
  void* pixels=gdk_pixbuf_get_pixels(img);
  if(cam->iter && gdk_pixbuf_get_n_channels(img)==4) // Inefficient, but we don't get to fix the pixbuf itself and mark it as fixed, at least not with any of the ways I've tried
  {
    cam_img_fixpixels_raw(pixmap, pixels, gdk_pixbuf_get_width(img)*gdk_pixbuf_get_height(img));
  }else{
    memcpy(pixmap, pixels, gdk_pixbuf_get_width(img)*3*gdk_pixbuf_get_height(img));
  }
}

void cam_close_img(CAM* cam)
{
  g_object_unref(G_OBJECT(cam->anim));
  free(cam);
}
