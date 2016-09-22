/*
    tc_client-gtk, a graphical user interface for tc_client
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
#include <string.h>
#include <stdlib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "postproc.h"

// TODO: Replace this img_* API with libcamera
struct img* img_load(const char* filename)
{
  struct img* this=malloc(sizeof(struct img));
  this->animation=gdk_pixbuf_animation_new_from_file(filename, 0);
  if(gdk_pixbuf_animation_is_static_image(this->animation))
  {
    this->img=gdk_pixbuf_animation_get_static_image(this->animation);
    this->iter=0;
    if(gdk_pixbuf_get_n_channels(this->img)==4)
    {
      unsigned char* pixels=gdk_pixbuf_get_pixels(this->img);
      unsigned int pixelcount=gdk_pixbuf_get_width(this->img)*gdk_pixbuf_get_height(this->img);
      unsigned int i;
      for(i=0; i<pixelcount; ++i)
      {
        memmove(&pixels[i*3], &pixels[i*4], 3);
      }
    }
  }else{
    this->iter=gdk_pixbuf_animation_get_iter(this->animation, 0);
    unsigned int w=gdk_pixbuf_animation_get_width(this->animation);
    unsigned int h=gdk_pixbuf_animation_get_height(this->animation);
    this->img=gdk_pixbuf_new(GDK_COLORSPACE_RGB, 0, 8, w, h);
  }
  return this;
}

GdkPixbuf* img_get(struct img* this)
{
  if(this->iter)
  {
    gdk_pixbuf_animation_iter_advance(this->iter, 0);
    GdkPixbuf* img=gdk_pixbuf_animation_iter_get_pixbuf(this->iter);
    if(gdk_pixbuf_get_n_channels(img)!=4){return img;}
    unsigned int w=gdk_pixbuf_get_width(img);
    unsigned int h=gdk_pixbuf_get_height(img);
    unsigned int dstw=gdk_pixbuf_get_width(this->img);
    unsigned int dsth=gdk_pixbuf_get_height(this->img);
    unsigned char* pixels=gdk_pixbuf_get_pixels(img);
    unsigned char* dstpixels=gdk_pixbuf_get_pixels(this->img);
    unsigned int x, y;
    for(y=0; y<h && y<dsth; ++y)
    for(x=0; x<w && x<dstw; ++x)
    {
      memmove(&dstpixels[(y*dstw+x)*3], &pixels[(y*w+x)*4], 3);
    }
  }
  return this->img;
}

void img_free(struct img* this)
{
  if(this->iter)
  {
    g_object_unref(this->iter);
    g_object_unref(this->animation);
    g_object_unref(this->img);
  }else{
    g_object_unref(this->animation);
  }
}

#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))

void rgb_to_hsv(int r, int g, int b, unsigned char* h, unsigned char* s, unsigned char* v)
{
  int minrgb=min(r, min(g, b));
  int maxrgb=max(r, max(g, b));
  *v=maxrgb;
  if(minrgb==maxrgb){*h=0; *s=0; return;} // Grayscale
  int diff=(r==minrgb)?g-b:((b==minrgb)?r-g:b-r);
  int huebase=(r==minrgb)?3:((b==minrgb)?1:5);
  int range=maxrgb-minrgb;
  *h=huebase*42-diff*42/range;
  *s=(unsigned int)(maxrgb-minrgb)*255/maxrgb;
}

void postproc_init(struct postproc_ctx* pp)
{
  pp->min_brightness=0;
  pp->max_brightness=255;
  pp->autoadjust=0;
  pp->flip_horizontal=0;
  pp->flip_vertical=0;
  pp->greenscreen=0;
  pp->greenscreen_tolerance[0]=0;
  pp->greenscreen_tolerance[1]=0;
  pp->greenscreen_tolerance[2]=0;
  pp->greenscreen_color[0]=0;
  pp->greenscreen_color[1]=255;
  pp->greenscreen_color[2]=0;
  pp->greenscreen_filename=0;
}

void postprocess(struct postproc_ctx* pp, unsigned char* buf, unsigned int width, unsigned int height)
{
  if(pp->min_brightness!=0 || pp->max_brightness!=255 || pp->autoadjust)
  {
    unsigned char min=255;
    unsigned char max=0;
    unsigned int count=width*height;
    unsigned int i;
    for(i=0; i<count*3; ++i)
    {
      if(pp->autoadjust)
      {
        if(buf[i]<min){min=buf[i];}
        if(buf[i]>max){max=buf[i];}
      }
      double v=((double)buf[i]-pp->min_brightness)*255/(pp->max_brightness-pp->min_brightness);
      if(v<0){v=0;}
      if(v>255){v=255;}
      buf[i]=v;
    }
    if(pp->autoadjust)
    {
      pp->min_brightness=min;
      pp->max_brightness=max;
    }
  }
  if(pp->flip_horizontal)
  {
    unsigned int x;
    unsigned int y;
    for(y=0; y<height; ++y)
    for(x=0; x<width/2; ++x)
    {
      unsigned char pixel[3];
      memcpy(pixel, &buf[(y*width+x)*3], 3);
      memcpy(&buf[(y*width+x)*3], &buf[(y*width+(width-x-1))*3], 3);
      memcpy(&buf[(y*width+(width-x-1))*3], pixel, 3);
    }
  }
  if(pp->flip_vertical)
  {
    unsigned int x;
    unsigned int y;
    for(y=0; y<height/2; ++y)
    for(x=0; x<width; ++x)
    {
      unsigned char pixel[3];
      memcpy(pixel, &buf[(y*width+x)*3], 3);
      memcpy(&buf[(y*width+x)*3], &buf[((height-y-1)*width+x)*3], 3);
      memcpy(&buf[((height-y-1)*width+x)*3], pixel, 3);
    }
  }
  if(pp->greenscreen)
  {
    GdkPixbuf* img=img_get(pp->greenscreen);
    guchar* pixels=gdk_pixbuf_get_pixels(img);
    unsigned int x;
    unsigned int y;
    unsigned int gswidth=gdk_pixbuf_get_width(img);
    unsigned int gsheight=gdk_pixbuf_get_height(img);
    unsigned int gsx, gsy;
    unsigned char* gscolor=pp->greenscreen_hsv;
    for(y=0; y<height; ++y)
    for(x=0; x<width; ++x)
    {
      unsigned char h,s,v;
      rgb_to_hsv(buf[(y*width+x)*3], buf[(y*width+x)*3+1], buf[(y*width+x)*3+2], &h, &s, &v);
      #define tolerance(a, b, margin) (abs(((int)(a))-((int)(b)))<=margin)
      if((tolerance(h, gscolor[0], pp->greenscreen_tolerance[0]) ||
          tolerance(((int)h+126)%252, ((int)gscolor[0]+126)%252, pp->greenscreen_tolerance[0])) && // For differences between red (beginning of spectrum) and purple (end of spectrum)
         tolerance(s, gscolor[1], pp->greenscreen_tolerance[1]) &&
         tolerance(v, gscolor[2], pp->greenscreen_tolerance[2]))
      {
        // TODO: Antialiasing?
        gsx=x*gswidth/width;
        gsy=y*gsheight/height;
        memcpy(&buf[(y*width+x)*3], &pixels[(gsy*gswidth+gsx)*3], 3);
      }
    }
  }
}

void postproc_free(struct postproc_ctx* pp)
{
  if(pp->greenscreen){img_free(pp->greenscreen);}
  free((void*)pp->greenscreen_filename);
}
