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
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <libv4l2.h>
#include <linux/videodev2.h>
#include "camera.h"

typedef struct CAM_t
{
  unsigned int type;
  int fd;
  unsigned int framesize;
} CAM;

char** cam_list_v4l2(char** list, unsigned int* count)
{
  DIR* dir=opendir("/dev");
  struct dirent* entry;
  while((entry=readdir(dir)))
  {
    if(!strncmp(entry->d_name, "video", 5))
    {
      ++*count;
      list=realloc(list, sizeof(char*)*(*count));
      char* path=malloc(strlen("v4l2:/dev/0")+strlen(entry->d_name));
      sprintf(path, "v4l2:/dev/%s", entry->d_name);
      list[(*count)-1]=path;
    }
  }
  return list;
}

CAM* cam_open_v4l2(const char* name)
{
  int fd=v4l2_open(&name[5], O_RDWR);
  if(fd<0){return 0;}
  CAM* cam=malloc(sizeof(CAM));
  cam->type=CAMTYPE_V4L2;
  cam->fd=fd;
  return cam;
}

void cam_resolution_v4l2(CAM* cam, unsigned int* width, unsigned int* height)
{
  struct v4l2_format fmt;
  fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width=*width;
  fmt.fmt.pix.height=*height;
  fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_RGB24;
  fmt.fmt.pix.field=V4L2_FIELD_NONE;
  fmt.fmt.pix.bytesperline=fmt.fmt.pix.width*3;
  fmt.fmt.pix.sizeimage=fmt.fmt.pix.bytesperline*fmt.fmt.pix.height;
  v4l2_ioctl(cam->fd, VIDIOC_S_FMT, &fmt);
  v4l2_ioctl(cam->fd, VIDIOC_G_FMT, &fmt);
  *width=fmt.fmt.pix.width;
  *height=fmt.fmt.pix.height;
  cam->framesize=(*width)*(*height)*3;
}

void cam_getframe_v4l2(CAM* cam, void* pixmap)
{
  if(v4l2_read(cam->fd, pixmap, cam->framesize)<0)
  {
    memset(pixmap, 0x7f, cam->framesize);
  }
}

void cam_close_v4l2(CAM* cam)
{
  v4l2_close(cam->fd);
  free(cam);
}
