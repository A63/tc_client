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

void postproc_init(struct postproc_ctx* pp)
{
  pp->min_brightness=0;
  pp->max_brightness=255;
  pp->autoadjust=0;
  pp->flip_horizontal=0;
  pp->flip_vertical=0;
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
}

void postproc_free(struct postproc_ctx* pp)
{
}
