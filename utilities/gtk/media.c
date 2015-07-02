/*
    tc_client-gtk, a graphical user interface for tc_client
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
#include <gtk/gtk.h>
#include <libavcodec/avcodec.h>
#ifdef HAVE_CAM
  #include <sys/prctl.h>
  #include <libswscale/swscale.h>
  #if LIBAVUTIL_VERSION_MAJOR>50 || (LIBAVUTIL_VERSION_MAJOR==50 && LIBAVUTIL_VERSION_MINOR>37)
    #include <libavutil/imgutils.h>
  #else
    #include <libavcore/imgutils.h>
  #endif
  #include "../libcamera/camera.h"
#endif
#include "../compat.h"
#include "compat.h"
#include "gui.h"
#include "media.h"
struct camera campreview;
struct camera* cams=0;
unsigned int camcount=0;
pid_t camproc=0;

#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
// Experimental mixer, not sure if it really works
void camera_playsnd(int audiopipe, struct camera* cam, short* samples, unsigned int samplecount)
{
  if(cam->samples)
  {
// int sources=1;
    unsigned int i;
    for(i=0; i<camcount; ++i)
    {
      if(!cams[i].samples){continue;}
      if(cam==&cams[i]){continue;}
      unsigned j;
      for(j=0; j<cam->samplecount && j<cams[i].samplecount; ++j)
      {
        cam->samples[j]+=cams[i].samples[j];
      }
      free(cams[i].samples);
      cams[i].samples=0;
// ++sources;
    }
    write(audiopipe, cam->samples, cam->samplecount*sizeof(short));
    free(cam->samples);
// printf("Mixed sound from %i sources (cam: %p)\n", sources, cam);
  }
  cam->samples=malloc(samplecount*sizeof(short));
  memcpy(cam->samples, samples, samplecount*sizeof(short));
  cam->samplecount=samplecount;
}
#endif

void camera_remove(const char* id)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    if(!strcmp(cams[i].id, id))
    {
      gtk_widget_destroy(cams[i].box);
      av_frame_free(&cams[i].frame);
      avcodec_free_context(&cams[i].vctx);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
      avcodec_free_context(&cams[i].actx);
#endif
      free(cams[i].id);
      free(cams[i].nick);
      --camcount;
      memmove(&cams[i], &cams[i+1], (camcount-i)*sizeof(struct camera));
      break;
    }
  }
}

void camera_removebynick(const char* nick)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    if(!strcmp(cams[i].nick, nick))
    {
      gtk_widget_destroy(cams[i].box);
      av_frame_free(&cams[i].frame);
      avcodec_free_context(&cams[i].vctx);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
      avcodec_free_context(&cams[i].actx);
#endif
      free(cams[i].id);
      free(cams[i].nick);
      --camcount;
      memmove(&cams[i], &cams[i+1], (camcount-i)*sizeof(struct camera));
      break;
    }
  }
}

struct camera* camera_find(const char* id)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    if(!strcmp(cams[i].id, id)){return &cams[i];}
  }
  return 0;
}

struct camera* camera_findbynick(const char* nick)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    if(!strcmp(cams[i].nick, nick)){return &cams[i];}
  }
  return 0;
}

struct camera* camera_new(void)
{
  ++camcount;
  cams=realloc(cams, sizeof(struct camera)*camcount);
  return &cams[camcount-1];
}

void camera_cleanup(void)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    av_frame_free(&cams[i].frame);
    avcodec_free_context(&cams[i].vctx);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
    avcodec_free_context(&cams[i].actx);
#endif
    free(cams[i].id);
    free(cams[i].nick);
  }
  free(cams);
}

#ifdef HAVE_CAM
extern unsigned int cameventsource;
GIOChannel* camthread(const char* name, AVCodec* vencoder, unsigned int delay)
{
  if(camproc)
  {
    g_source_remove(cameventsource);
    kill(camproc, SIGINT);
    usleep(200000); // Give the previous process some time to shut down
  }
  // Set up a second pipe to be handled by handledata() to avoid overlap with tc_client's output
  int campipe[2];
  pipe(campipe);
  camproc=fork();
  if(!camproc)
  {
    close(campipe[0]);
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    // Set up camera
    CAM* cam=cam_open(name);
    AVCodecContext* ctx=avcodec_alloc_context3(vencoder);
    ctx->width=320;
    ctx->height=240;
    cam_resolution(cam, (unsigned int*)&ctx->width, (unsigned int*)&ctx->height);
    ctx->pix_fmt=PIX_FMT_YUV420P;
    ctx->time_base.num=1;
    ctx->time_base.den=10;
    avcodec_open2(ctx, vencoder, 0);
    AVFrame* frame=av_frame_alloc();
    frame->format=PIX_FMT_RGB24;
    frame->width=ctx->width;
    frame->height=ctx->height;
    av_image_alloc(frame->data, frame->linesize, ctx->width, ctx->height, frame->format, 1);
    AVPacket packet;
    packet.buf=0;
    packet.data=0;
    packet.size=0;
    packet.dts=AV_NOPTS_VALUE;
    packet.pts=AV_NOPTS_VALUE;

    // Set up frame for conversion from the camera's format to a format the encoder can use
    AVFrame* dstframe=av_frame_alloc();
    dstframe->format=ctx->pix_fmt;
    dstframe->width=ctx->width;
    dstframe->height=ctx->height;
    av_image_alloc(dstframe->data, dstframe->linesize, ctx->width, ctx->height, ctx->pix_fmt, 1);

    struct SwsContext* swsctx=sws_getContext(frame->width, frame->height, PIX_FMT_RGB24, frame->width, frame->height, AV_PIX_FMT_YUV420P, 0, 0, 0, 0);

    while(1)
    {
      usleep(delay);
      if(delay>100000){delay-=50000;}
      cam_getframe(cam, frame->data[0]);
      int gotpacket;
      sws_scale(swsctx, (const uint8_t*const*)frame->data, frame->linesize, 0, frame->height, dstframe->data, dstframe->linesize);
      av_init_packet(&packet);
      packet.data=0;
      packet.size=0;
      avcodec_encode_video2(ctx, &packet, dstframe, &gotpacket);
      unsigned char frameinfo=0x22; // Note: differentiating between keyframes and non-keyframes seems to break stuff, so let's just go with all being interframes (1=keyframe, 2=interframe, 3=disposable interframe)
      // Send the packet to our main thread so we can see ourselves (the main thread also sends it to the server)
      dprintf(campipe[1], "Video: out %i\n", packet.size+1);
      write(campipe[1], &frameinfo, 1);
      write(campipe[1], packet.data, packet.size);

      av_free_packet(&packet);
    }
    sws_freeContext(swsctx);
    _exit(0);
  }
  close(campipe[1]);
  GIOChannel* channel=g_io_channel_unix_new(campipe[0]);
  g_io_channel_set_encoding(channel, 0, 0);
  return channel;
}

extern GIOFunc handledata;
void camselect_change(GtkComboBox* combo, AVCodec* vencoder)
{
  if(!camproc){return;} // If there isn't a camthread already, it will be started elsewhere
  GIOChannel* channel=camthread(gtk_combo_box_get_active_id(combo), vencoder, 100000);
  cameventsource=g_io_add_watch(channel, G_IO_IN, (void*)&handledata, 0);
}

gboolean camselect_cancel(GtkWidget* widget, void* x1, void* x2)
{
  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "camselection"));
  gtk_widget_hide(window);
  // Note: unchecking the menu item kills the cam thread for us
  GtkCheckMenuItem* item=GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_camera"));
  gtk_check_menu_item_set_active(item, 0);
  return 1;
}

extern int tc_client_in[];
void camselect_accept(GtkWidget* widget, AVCodec* vencoder)
{
  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "camselection"));
  gtk_widget_hide(window);
  // Restart the cam thread with a high initial delay to workaround the bug in the flash client
  GtkComboBox* combo=GTK_COMBO_BOX(gtk_builder_get_object(gui, "camselect_combo"));
  GIOChannel* channel=camthread(gtk_combo_box_get_active_id(combo), vencoder, 500000);
  cameventsource=g_io_add_watch(channel, G_IO_IN, (void*)&handledata, 0);
  dprintf(tc_client_in[1], "/camup\n");
}
#endif
