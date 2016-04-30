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
#ifndef _WIN32
#include <sys/prctl.h>
#endif
#include <libswscale/swscale.h>
#if LIBAVUTIL_VERSION_MAJOR>50 || (LIBAVUTIL_VERSION_MAJOR==50 && LIBAVUTIL_VERSION_MINOR>37)
  #include <libavutil/imgutils.h>
#else
  #include <libavcore/imgutils.h>
#endif
#include "../libcamera/camera.h"
#include "compat.h"
#include "../compat.h"
#include "gui.h"
#include "media.h"
struct camera campreview={
  .postproc.min_brightness=0,
  .postproc.max_brightness=255,
  .postproc.autoadjust=0
};
struct camera* cams=0;
unsigned int camcount=0;
#ifdef _WIN32
  PROCESS_INFORMATION camprocess={.hProcess=0};
#else
  pid_t camproc=0;
#endif

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

struct camera* camera_new(const char* nick, const char* id)
{
  ++camcount;
  cams=realloc(cams, sizeof(struct camera)*camcount);
  struct camera* cam=&cams[camcount-1];
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
  cam->samples=0;
  cam->samplecount=0;
#endif
  cam->nick=strdup(nick);
  cam->id=strdup(id);
  cam->frame=av_frame_alloc();
  cam->dstframe=av_frame_alloc();
  cam->cam=gtk_image_new();
  cam->box=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_set_homogeneous(GTK_BOX(cam->box), 0);
  // Wrap cam image in an event box to catch (right) clicks
  GtkWidget* eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), cam->cam);
  gtk_event_box_set_above_child(GTK_EVENT_BOX(eventbox), 1);
  cam->label=gtk_label_new(cam->nick);
  gtk_box_pack_start(GTK_BOX(cam->box), eventbox, 0, 0, 0);
  gtk_box_pack_start(GTK_BOX(cam->box), cam->label, 0, 0, 0);
  g_signal_connect(eventbox, "button-release-event", G_CALLBACK(gui_show_cam_menu), cam->id);
  // Initialize postprocessing values
  cam->postproc.min_brightness=0;
  cam->postproc.max_brightness=255;
  cam->postproc.autoadjust=0;
  return cam;
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

unsigned int* camthread_delay;
void camthread_resetdelay(int x)
{
  *camthread_delay=500000;
}
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
#ifndef _WIN32
  CAM* cam=cam_open(name); // Opening here in case of GUI callbacks
  pipe(campipe);
  camproc=fork();
  if(!camproc)
  {
    close(campipe[0]);
    if(!cam){_exit(1);}
#ifndef _WIN32
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    camthread_delay=&delay;
    signal(SIGUSR1, camthread_resetdelay);
#endif
    // Set up camera
    AVCodecContext* ctx=avcodec_alloc_context3(vencoder);
    ctx->width=320;
    ctx->height=240;
    cam_resolution(cam, (unsigned int*)&ctx->width, (unsigned int*)&ctx->height);
    ctx->pix_fmt=AV_PIX_FMT_YUV420P;
    ctx->time_base.num=1;
    ctx->time_base.den=10;
    avcodec_open2(ctx, vencoder, 0);
    AVFrame* frame=av_frame_alloc();
    frame->format=AV_PIX_FMT_RGB24;
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

    struct SwsContext* swsctx=sws_getContext(frame->width, frame->height, AV_PIX_FMT_RGB24, frame->width, frame->height, AV_PIX_FMT_YUV420P, 0, 0, 0, 0);

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
  if(cam){cam_close(cam);} // Leave the cam to the child process
  close(campipe[1]);
#else
  char cmd[snprintf(0,0, "./tc_client-gtk-camthread %s %u", name, delay)+1];
  sprintf(cmd, "./tc_client-gtk-camthread %s %u", name, delay);
  w32_runcmdpipes(cmd, ((int*)0), campipe, camprocess);
#endif
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
#ifndef _WIN32
  // Tell the camthread to reset the delay to 500000 as a workaround for a quirk in the flash client
  kill(camproc, SIGUSR1);
#else
  // For platforms without proper signals, resort to restarting the camthread with the new delay
  GtkComboBox* combo=GTK_COMBO_BOX(gtk_builder_get_object(gui, "camselect_combo"));
  GIOChannel* channel=camthread(gtk_combo_box_get_active_id(combo), vencoder, 500000);
  cameventsource=g_io_add_watch(channel, G_IO_IN, (void*)&handledata, 0);
#endif
  dprintf(tc_client_in[1], "/camup\n");
}

void camselect_file_preview(GtkFileChooser* dialog, gpointer data)
{
  GtkImage* preview=GTK_IMAGE(data);
  char* file=gtk_file_chooser_get_preview_filename(dialog);
  GdkPixbuf* img=gdk_pixbuf_new_from_file_at_size(file, 256, 256, 0);
  g_free(file);
  gtk_image_set_from_pixbuf(preview, img);
  if(img){g_object_unref(img);}
  gtk_file_chooser_set_preview_widget_active(dialog, !!img);
}

const char* camselect_file(void)
{
  GtkWidget* preview=gtk_image_new();
#ifndef _WIN32
  GtkWindow* window=GTK_WINDOW(gtk_builder_get_object(gui, "camselection"));
#else
  GtkWindow* window=0;
#endif
  GtkWidget* dialog=gtk_file_chooser_dialog_new("Open Image", window, GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, (char*)0);
  GtkFileFilter* filter=gtk_file_filter_new();
  gtk_file_filter_add_pixbuf_formats(filter);
  gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
  gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog), preview);
  g_signal_connect(dialog, "update-preview", G_CALLBACK(camselect_file_preview), preview);
  int res=gtk_dialog_run(GTK_DIALOG(dialog));
  char* file;
  if(res==GTK_RESPONSE_ACCEPT)
  {
    file=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  }else{file=0;}
  gtk_widget_destroy(dialog);
  return file;
}

void camera_postproc(struct camera* cam, unsigned char* buf, unsigned int count)
{
  if(cam->postproc.min_brightness==0 && cam->postproc.max_brightness==255 && !cam->postproc.autoadjust){return;}
  unsigned char min=255;
  unsigned char max=0;
  unsigned int i;
  for(i=0; i<count*3; ++i)
  {
    if(cam->postproc.autoadjust)
    {
      if(buf[i]<min){min=buf[i];}
      if(buf[i]>max){max=buf[i];}
    }
    double v=((double)buf[i]-cam->postproc.min_brightness)*255/(cam->postproc.max_brightness-cam->postproc.min_brightness);
    if(v<0){v=0;}
    if(v>255){v=255;}
    buf[i]=v;
  }
  if(cam->postproc.autoadjust)
  {
    cam->postproc.min_brightness=min;
    cam->postproc.max_brightness=max;
  }
}
