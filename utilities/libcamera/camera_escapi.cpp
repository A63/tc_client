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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "../../escapi/escapi.h"
extern "C"
{
#include "camera.h"

typedef struct CAM_t
{
  unsigned int type;
  struct SimpleCapParams capture;
  unsigned int device;
  unsigned char* buf;
  char capturing;
} CAM;

char cam_escapi_init=0;

char** cam_list_escapi(char** list, unsigned int* count)
{
  int escapicams;
  if(cam_escapi_init)
  {
    escapicams=countCaptureDevices();
  }else{
    cam_escapi_init=1;
    escapicams=setupESCAPI();
  }
  char buf[1024];
  unsigned int i;
  for(i=0; i<escapicams; ++i)
  {
    sprintf(buf, "escapi:%u:", i);
    getCaptureDeviceName(i, &buf[strlen(buf)], 1023-strlen(buf));
    ++*count;
    list=(char**)realloc(list, sizeof(char*)*(*count));
    list[(*count)-1]=strdup(buf);
  }
  return list;
}

CAM* cam_open_escapi(const char* name)
{
  if(!cam_escapi_init)
  {
    cam_escapi_init=1;
    setupESCAPI();
  }
  CAM* cam=(CAM*)malloc(sizeof(CAM));
  cam->type=CAMTYPE_ESCAPI;
  cam->capture.mWidth=640;
  cam->capture.mHeight=480;
  cam->buf=(unsigned char*)malloc(cam->capture.mWidth*cam->capture.mHeight*4);
  cam->capture.mTargetBuf=(int*)cam->buf;
  cam->device=strtoul(&name[7], 0, 10);
  cam->capturing=initCapture(cam->device, &cam->capture);
  return cam;
}

void cam_resolution_escapi(CAM* cam, unsigned int* width, unsigned int* height)
{
  if(cam->capturing){deinitCapture(cam->device);}
  cam->capture.mWidth=*width;
  cam->capture.mHeight=*height;
  free(cam->buf);
  cam->buf=(unsigned char*)malloc(cam->capture.mWidth*cam->capture.mHeight*4);
  cam->capture.mTargetBuf=(int*)cam->buf;
  cam->capturing=initCapture(cam->device, &cam->capture);
}

void cam_getframe_escapi(CAM* cam, void* pixmap)
{
  doCapture(0);
  while(!isCaptureDone(0)){usleep(100);}
  unsigned int pixels=cam->capture.mWidth*cam->capture.mHeight;
  unsigned int i;
  for(i=0; i<pixels; ++i)
  {
    // ABGR -> RGB
    memcpy((unsigned char*)pixmap+(i*3)+2, cam->buf+(i*4), 1);
    memcpy((unsigned char*)pixmap+(i*3)+1, cam->buf+(i*4)+1, 1);
    memcpy((unsigned char*)pixmap+(i*3), cam->buf+(i*4)+2, 1);
  }
}

void cam_close_escapi(CAM* cam)
{
  if(cam->capturing){deinitCapture(cam->device);}
  free(cam);
}
}
