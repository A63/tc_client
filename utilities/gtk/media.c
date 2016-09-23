/*
    tc_client-gtk, a graphical user interface for tc_client
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
#include "../compat_av.h"
#include "gui.h"
#include "media.h"

#define PREVIEW_MAX_WIDTH 640
#define PREVIEW_MAX_HEIGHT 480
extern int tc_client_in[2];
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
struct size camsize_out={.width=320, .height=240};
struct size camsize_scale={.width=320, .height=240};
GtkWidget* cambox;
GtkWidget** camrows=0;
unsigned int camrowcount=0;
GdkPixbufAnimation* camplaceholder=0;
GdkPixbufAnimationIter* camplaceholder_iter=0;

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

void camera_free(struct camera* cam)
{
  if(cam->placeholder){g_source_remove(cam->placeholder);}
  av_frame_free(&cam->frame);
  avcodec_free_context(&cam->vctx);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
  avcodec_free_context(&cam->actx);
#endif
  free(cam->id);
  free(cam->nick);

  postproc_free(&cam->postproc);
}

void camera_remove(const char* id, char isnick)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    if(!strcmp(isnick?cams[i].nick:cams[i].id, id))
    {
      gtk_widget_destroy(cams[i].box);
      camera_free(&cams[i]);
      --camcount;
      memmove(&cams[i], &cams[i+1], (camcount-i)*sizeof(struct camera));
      break;
    }
  }
  updatescaling(0, 0, 1);
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
  postproc_init(&cam->postproc);
  return cam;
}

void camera_cleanup(void)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    camera_free(&cams[i]);
  }
  free(cams);
}

void freebuffer(guchar* pixels, gpointer data){free(pixels);}

gboolean cam_encode(GIOChannel* iochannel, GIOCondition condition, gpointer datap)
{
  struct camera* cam=camera_find("out");
  char preview=0;
  if(!cam)
  {
    cam=&campreview;
    preview=1;
  }else{
    cam->vctx->width=camsize_out.width;
    cam->vctx->height=camsize_out.height;
    if(!cam->dstframe->data[0])
    {
      cam->dstframe->format=AV_PIX_FMT_YUV420P;
      cam->dstframe->width=camsize_out.width;
      cam->dstframe->height=camsize_out.height;
      av_image_alloc(cam->dstframe->data, cam->dstframe->linesize, camsize_out.width, camsize_out.height, cam->dstframe->format, 1);
    }
  }
  if(cam->frame->width!=camsize_out.width || cam->frame->height!=camsize_out.height)
  {
    cam->frame->format=AV_PIX_FMT_RGB24;
    cam->frame->width=camsize_out.width;
    cam->frame->height=camsize_out.height;
    cam->frame->linesize[0]=cam->frame->width*3;
    av_freep(cam->frame->data);
    av_image_alloc(cam->frame->data, cam->frame->linesize, camsize_out.width, camsize_out.height, cam->frame->format, 1);
  }
  g_io_channel_read_chars(iochannel, (void*)cam->frame->data[0], camsize_out.width*camsize_out.height*3, 0, 0);
  postprocess(&cam->postproc, cam->frame->data[0], cam->frame->width, cam->frame->height);
  // Update our local display
  GdkPixbuf* oldpixbuf=gtk_image_get_pixbuf(GTK_IMAGE(cam->cam));
  GdkPixbuf* gdkframe=gdk_pixbuf_new_from_data(cam->frame->data[0], GDK_COLORSPACE_RGB, 0, 8, cam->frame->width, cam->frame->height, cam->frame->linesize[0], 0, 0);
  if(!preview) // Scale, unless we're previewing
  {
    gdkframe=gdk_pixbuf_scale_simple(gdkframe, camsize_scale.width, camsize_scale.height, GDK_INTERP_BILINEAR);
  }else if(gdk_pixbuf_get_height(gdkframe)>PREVIEW_MAX_HEIGHT || gdk_pixbuf_get_width(gdkframe)>PREVIEW_MAX_WIDTH) // Scale anyway if the input is huge
  {
    unsigned int width=gdk_pixbuf_get_width(gdkframe);
    unsigned int height=gdk_pixbuf_get_height(gdkframe);
    if(height*PREVIEW_MAX_WIDTH/width>PREVIEW_MAX_HEIGHT)
    {
      gdkframe=gdk_pixbuf_scale_simple(gdkframe, width*PREVIEW_MAX_HEIGHT/height, PREVIEW_MAX_HEIGHT, GDK_INTERP_BILINEAR);
    }else{
      gdkframe=gdk_pixbuf_scale_simple(gdkframe, PREVIEW_MAX_WIDTH, height*PREVIEW_MAX_WIDTH/width, GDK_INTERP_BILINEAR);
    }
  }
  gtk_image_set_from_pixbuf(GTK_IMAGE(cam->cam), gdkframe);
  g_object_unref(oldpixbuf);
  if(preview){return 1;}
  // Encode
  struct SwsContext* swsctx=sws_getContext(cam->frame->width, cam->frame->height, cam->frame->format, cam->frame->width, cam->frame->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
  sws_scale(swsctx, (const uint8_t*const*)cam->frame->data, cam->frame->linesize, 0, cam->frame->height, cam->dstframe->data, cam->dstframe->linesize);
  sws_freeContext(swsctx);
  int gotpacket;
  AVPacket packet={
#ifdef AVPACKET_HAS_BUF
    .buf=0,
#endif
    .data=0,
    .size=0,
    .dts=AV_NOPTS_VALUE,
    .pts=AV_NOPTS_VALUE
  };
  av_init_packet(&packet);
  avcodec_send_frame(cam->vctx, cam->dstframe);
  gotpacket=avcodec_receive_packet(cam->vctx, &packet);
  if(gotpacket){return 1;}
  unsigned char frameinfo=0x22; // Note: differentiating between keyframes and non-keyframes seems to break stuff, so let's just go with all being interframes (1=keyframe, 2=interframe, 3=disposable interframe)
  // Send video
  dprintf(tc_client_in[1], "/video %i\n", packet.size+1);
  write(tc_client_in[1], &frameinfo, 1);
  ssize_t w=write(tc_client_in[1], packet.data, packet.size);
if(w!=packet.size){printf("Error: wrote %li of %i bytes\n", w, packet.size);}

  av_packet_unref(&packet);
  return 1;
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
  // Set up a pipe to be handled by cam_encode()
  int campipe[2];
#ifndef _WIN32
  CAM* cam=cam_open(name); // Opening here in case of GUI callbacks
  if(cam){cam_resolution(cam, (unsigned int*)&camsize_out.width, (unsigned int*)&camsize_out.height);}
  pipe(campipe);
  camproc=fork();
  if(!camproc)
  {
    close(campipe[0]);
    unsigned char img[camsize_out.width*camsize_out.height*3];
    if(!cam) // If it failed to open, just give some grey before quitting
    {
      memset(img, 0x7f, camsize_out.width*camsize_out.height*3);
      write(campipe[1], img, camsize_out.width*camsize_out.height*3);
      _exit(1);
    }
#ifndef _WIN32
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    camthread_delay=&delay;
    signal(SIGUSR1, camthread_resetdelay);
#endif
    while(1)
    {
      usleep(delay);
      if(delay>100000){delay-=50000;}
      cam_getframe(cam, img);
      write(campipe[1], img, camsize_out.width*camsize_out.height*3);
    }
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

void camselect_change(GtkComboBox* combo, AVCodec* vencoder)
{
  if(!camproc){return;} // If there isn't a camthread already, it will be started elsewhere
  GIOChannel* channel=camthread(gtk_combo_box_get_active_id(combo), vencoder, 100000);
  cameventsource=g_io_add_watch(channel, G_IO_IN, cam_encode, 0);
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
  cameventsource=g_io_add_watch(channel, G_IO_IN, cam_encode, 0);
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

void updatescaling(unsigned int width, unsigned int height, char changedcams)
{
  if(!camcount){return;}
  if(!width){width=gtk_widget_get_allocated_width(GTK_WIDGET(gtk_builder_get_object(gui, "main")));}
  if(!height){height=gtk_widget_get_allocated_height(GTK_WIDGET(gtk_builder_get_object(gui, "camerascroll")));}

  GtkRequisition label;
  gtk_widget_get_preferred_size(cams[0].label, &label, 0);
  camsize_scale.width=1;
  camsize_scale.height=1;
  unsigned int rowcount=1;
  unsigned int rows;
  for(rows=1; rows<=camcount; ++rows)
  {
    struct size scale;
    unsigned int cams_per_row=camcount/rows;
    if(camcount%rows){++cams_per_row;}
    scale.width=width/cams_per_row;
    // 3/4 ratio
    scale.height=scale.width*3/4;
    unsigned int rowheight=height/rows;
    // Fit by height
    if(rowheight<scale.height+label.height)
    {
      scale.height=rowheight-label.height;
      scale.width=scale.height*4/3;
    }
    if(scale.width>camsize_scale.width) // Check if this number of rows will fit larger cams
    {
      camsize_scale.width=scale.width;
      camsize_scale.height=scale.height;
      rowcount=rows;
    }else if(scale.width<camsize_scale.width){break;} // Only getting smaller from here, use the last one that increased
  }

  unsigned int i;
  if(rowcount!=camrowcount || changedcams) // Changed the number of rows, shuffle everything around to fit. Or added/removed a camera, in which case we need to shuffle things around anyway
  {
    for(i=0; i<camcount; ++i)
    {
      g_object_ref(cams[i].box); // Increase reference counts so that they are not deallocated while they are temporarily detached from the rows
      GtkContainer* parent=GTK_CONTAINER(gtk_widget_get_parent(cams[i].box));
      if(parent){gtk_container_remove(parent, cams[i].box);}
    }
    for(i=0; i<camrowcount; ++i){gtk_widget_destroy(camrows[i]);} // Erase old rows
    camrowcount=rowcount;
    camrows=realloc(camrows, sizeof(GtkWidget*)*camrowcount);
    for(i=0; i<camrowcount; ++i)
    {
      camrows[i]=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(cambox), camrows[i], 1, 0, 0);
      gtk_widget_set_halign(camrows[i], GTK_ALIGN_CENTER);
      gtk_widget_show(camrows[i]);
    }
    unsigned int cams_per_row=camcount/camrowcount;
    if(camcount%camrowcount){++cams_per_row;}
    for(i=0; i<camcount; ++i)
    {
      gtk_box_pack_start(GTK_BOX(camrows[i/cams_per_row]), cams[i].box, 0, 0, 0);
      g_object_unref(cams[i].box); // Decrease reference counts once they're attached again
    }
  }
  // libswscale doesn't handle unreasonably small sizes well
  if(camsize_scale.width<8){camsize_scale.width=8;}
  if(camsize_scale.height<1){camsize_scale.height=1;}
  // Rescale current images to fit
  for(i=0; i<camcount; ++i)
  {
    GdkPixbuf* pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(cams[i].cam));
    if(!pixbuf){continue;}
    GdkPixbuf* old=pixbuf;
    pixbuf=gdk_pixbuf_scale_simple(pixbuf, camsize_scale.width, camsize_scale.height, GDK_INTERP_BILINEAR);
    gtk_image_set_from_pixbuf(GTK_IMAGE(cams[i].cam), pixbuf);
    g_object_unref(old);
  }
}

gboolean camplaceholder_update(void* id)
{
  struct camera* cam=camera_find(id);
  GdkPixbuf* oldpixbuf=gtk_image_get_pixbuf(GTK_IMAGE(cam->cam));
  // Get the current frame of the animation
  gdk_pixbuf_animation_iter_advance(camplaceholder_iter, 0);
  GdkPixbuf* frame=gdk_pixbuf_animation_iter_get_pixbuf(camplaceholder_iter);
  // Scale and replace the current image on camera
  GdkPixbuf* pixbuf=gdk_pixbuf_scale_simple(frame, camsize_scale.width, camsize_scale.height, GDK_INTERP_BILINEAR);
  gtk_image_set_from_pixbuf(GTK_IMAGE(cam->cam), pixbuf);
  g_object_unref(oldpixbuf);
  return G_SOURCE_CONTINUE;
}
