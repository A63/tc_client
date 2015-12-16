/*
    libcamera, a camera access abstraction library
    Copyright (C) 2015  alicia@ion.nu

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
#include "camera.h"
#include "camera_v4l2.h"
#include "camera_img.h"

struct CAM_t
{
  unsigned int type;
};

char** cam_list(unsigned int* count)
{
  char** list=0;
  *count=0;
  #ifdef HAVE_V4L2
  list=cam_list_v4l2(list, count);
  #endif
  list=cam_list_img(list, count);
  return list;
}

CAM* cam_open(const char* name)
{
  #ifdef HAVE_V4L2
  if(!strncmp(name, "v4l2:", 5)){return cam_open_v4l2(name);}
  #endif
  if(!strcmp(name, "Image")){return cam_open_img();}
  return 0;
}

void cam_resolution(CAM* cam, unsigned int* width, unsigned int* height)
{
  switch(cam->type)
  {
    #ifdef HAVE_V4L2
    case CAMTYPE_V4L2: cam_resolution_v4l2(cam, width, height); break;
    #endif
    case CAMTYPE_IMG: cam_resolution_img(cam, width, height); break;
  }
}

void cam_getframe(CAM* cam, void* pixmap)
{
  switch(cam->type)
  {
    #ifdef HAVE_V4L2
    case CAMTYPE_V4L2: cam_getframe_v4l2(cam, pixmap); break;
    #endif
    case CAMTYPE_IMG: cam_getframe_img(cam, pixmap); break;
  }
}

void cam_close(CAM* cam)
{
  switch(cam->type)
  {
    #ifdef HAVE_V4L2
    case CAMTYPE_V4L2: cam_close_v4l2(cam); break;
    #endif
    case CAMTYPE_IMG: cam_close_img(cam); break;
  }
}
