/*
    tc_client-gtk, a graphical user interface for tc_client
    Copyright (C) 2015-2017  alicia@ion.nu

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
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#if LIBAVUTIL_VERSION_MAJOR>50 || (LIBAVUTIL_VERSION_MAJOR==50 && LIBAVUTIL_VERSION_MINOR>37)
  #include <libavutil/imgutils.h>
#else
  #include <libavcore/imgutils.h>
#endif
#ifdef HAVE_PULSEAUDIO
  #include <pulse/simple.h>
#endif
#include "../libcamera/camera.h"
#include "compat.h"
#include "../compat.h"
#include "../compat_av.h"
#include "gui.h"
#include "main.h"
#include "greenroom.h"
#include "media.h"

#define PREVIEW_MAX_WIDTH 640
#define PREVIEW_MAX_HEIGHT 480
extern int tc_client_in[2];
struct camera campreview={
  .postproc.min_brightness=0,
  .postproc.max_brightness=255,
  .postproc.autoadjust=0
};
static struct camera** cams=0;
static unsigned int camcount=0;
struct size camsize_out={.width=320, .height=240};
struct size camsize_scale={.width=320, .height=240};
GtkWidget* cambox;
GtkWidget** camrows=0;
unsigned int camrowcount=0;
GdkPixbufAnimation* camplaceholder=0;
GdkPixbufAnimationIter* camplaceholder_iter=0;
CAM* camout_cam=0;
#ifdef HAVE_PULSEAUDIO
char pushtotalk_enabled=0;
char pushtotalk_pushed=0;
#endif
unsigned int camout_delay;

#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
void camera_playsnd(struct camera* cam, int16_t* samples, unsigned int samplecount)
{
  cam->samples=realloc(cam->samples, sizeof(int16_t)*(cam->samplecount+samplecount));
  memcpy(&cam->samples[cam->samplecount], samples, samplecount*sizeof(short));
  cam->samplecount+=samplecount;
}

gboolean audiomixer(void* p)
{
  int audiopipe=*(int*)p;
  unsigned int i;
  int sources=0;
  for(i=0; i<camcount; ++i){sources+=!!cams[i]->samplecount;}
  if(!sources){return G_SOURCE_CONTINUE;}
  unsigned int samplecount=SAMPLERATE_OUT/25; // Play one 25th of the samplerate's samples per iteration (which happens 25 times per second)
  int16_t samples[samplecount];
  memset(samples, 0, samplecount*sizeof(int16_t));
  for(i=0; i<camcount; ++i)
  {
    if(!cams[i]->samplecount){continue;}
    unsigned j;
    for(j=0; j<samplecount && j<cams[i]->samplecount; ++j)
    {
      // Divide by number of sources to prevent integer overflow
      samples[j]+=cams[i]->samples[j]/sources;
    }
    if(cams[i]->samplecount>samplecount)
    {
      cams[i]->samplecount-=samplecount;
      // Deal with drift and post-lag floods
      if(cams[i]->samplecount>SAMPLERATE_OUT/5){cams[i]->samplecount=0; continue;}
    }else{
      cams[i]->samplecount=0;
      continue;
    }
    memmove(cams[i]->samples, &cams[i]->samples[samplecount], sizeof(int16_t)*cams[i]->samplecount);
  }
  write(audiopipe, samples, samplecount*sizeof(int16_t));
  return G_SOURCE_CONTINUE;
}
#endif

void camera_free(struct camera* cam)
{
  if(cam->placeholder){g_source_remove(cam->placeholder);}
  av_frame_free(&cam->frame);
  av_frame_free(&cam->dstframe);
  avcodec_free_context(&cam->vctx);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
  if(cam->actx)
  {
    avcodec_free_context(&cam->actx);
    #ifdef HAVE_AVRESAMPLE
      avresample_free(&cam->resamplectx);
    #else
      swr_free(&cam->swrctx);
    #endif
  }
#endif
  free(cam->id);
  free(cam->nick);

  postproc_free(&cam->postproc);
  free(cam);
}

void camera_remove(const char* id, char isnick)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    if(!strcmp(isnick?cams[i]->nick:cams[i]->id, id))
    {
      gtk_widget_destroy(cams[i]->box);
      camera_free(cams[i]);
      --camcount;
      memmove(&cams[i], &cams[i+1], (camcount-i)*sizeof(struct camera*));
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
    if(!strcmp(cams[i]->id, id)){return cams[i];}
  }
  return 0;
}

struct camera* camera_findbynick(const char* nick)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    if(!strcmp(cams[i]->nick, nick)){return cams[i];}
  }
  return 0;
}

struct camera* camera_new(const char* nick, const char* id, unsigned char flags)
{
  struct camera* cam=malloc(sizeof(struct camera));
  cam->vctx=0;
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
  cam->actx=0;
  cam->samples=0;
  cam->samplecount=0;
  #ifdef HAVE_AVRESAMPLE
  cam->resamplectx=0;
  #else
  cam->swrctx=0;
  #endif
#endif
  cam->nick=strdup(nick);
  cam->id=strdup(id);
  cam->frame=av_frame_alloc();
  cam->dstframe=av_frame_alloc();
  cam->cam=gtk_image_new();
  if(flags&CAMFLAG_GREENROOM)
  {
    cam->box=0;
    cam->placeholder=0;
  }else{
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
    cam->placeholder=g_timeout_add(100, camplaceholder_update, cam->id);
  }
  cam->volume=0;
  cam->volumeold=1024;
  cam->flags=flags;
  // Initialize postprocessing values
  postproc_init(&cam->postproc);
  // Add new cam to the list
  cams=realloc(cams, sizeof(struct camera*)*(camcount+1));
  cams[camcount]=cam;
  ++camcount;
  return cam;
}

#ifdef HAVE_LIBAO
char camera_init_audio(struct camera* cam, uint8_t frameinfo)
{
  switch((frameinfo&0xc)/0x4)
  {
    case 0: cam->samplerate=5500; break;
    case 1: cam->samplerate=11025; break;
    case 2: cam->samplerate=22050; break;
    case 3: cam->samplerate=44100; break;
  }
  AVCodec* decoder=0;
  switch(frameinfo/0x10)
  {
    case 0x4: cam->samplerate=16000; decoder=avcodec_find_decoder(AV_CODEC_ID_NELLYMOSER); break;
    case 0x5: cam->samplerate=8000;
    case 0x6: decoder=avcodec_find_decoder(AV_CODEC_ID_NELLYMOSER); break;
    case 0xb: decoder=avcodec_find_decoder(AV_CODEC_ID_SPEEX); break;
    default:
      printf("Unknown audio codec ID 0x%hhx\n", frameinfo/0x10);
      return 1;
  }
  cam->actx=avcodec_alloc_context3(decoder);
  avcodec_open2(cam->actx, decoder, 0);
  #ifdef HAVE_AVRESAMPLE
    cam->resamplectx=avresample_alloc_context();
    av_opt_set_int(cam->resamplectx, "in_channel_layout", AV_CH_FRONT_CENTER, 0);
    av_opt_set_int(cam->resamplectx, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    av_opt_set_int(cam->resamplectx, "in_sample_rate", cam->samplerate, 0);
    av_opt_set_int(cam->resamplectx, "out_channel_layout", AV_CH_FRONT_CENTER, 0);
    av_opt_set_int(cam->resamplectx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int(cam->resamplectx, "out_sample_rate", SAMPLERATE_OUT, 0);
    avresample_open(cam->resamplectx);
  #else
    cam->swrctx=swr_alloc_set_opts(0, AV_CH_FRONT_CENTER, AV_SAMPLE_FMT_S16, SAMPLERATE_OUT, AV_CH_FRONT_CENTER, AV_SAMPLE_FMT_FLT, cam->samplerate, 0, 0);
    swr_init(cam->swrctx);
  #endif
  return 0;
}
#endif

void camera_cleanup(void)
{
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    camera_free(cams[i]);
  }
  free(cams);
}

void freebuffer(guchar* pixels, gpointer data){free(pixels);}

void startcamout(CAM* cam)
{
  if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_mic"))))
  { // Only /camup if we're not already broadcasting mic
    int fd=((hasgreenroom && !greenroom_gotpass)?greenroompipe_in[1]:tc_client_in[1]);
    dprintf(fd, "/camup\n");
  }
  camout_cam=cam;
  camout_delay=500;
  camsize_out.width=320;
  camsize_out.height=240;
  cam_resolution(cam, (unsigned int*)&camsize_out.width, (unsigned int*)&camsize_out.height);
  g_timeout_add(camout_delay, cam_encode, cam);
}

GdkPixbuf* scaleframe(void* data, unsigned int width, unsigned int height, unsigned int linesize, unsigned int towidth, unsigned int toheight)
{
  GdkPixbuf* gdkframe=gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, 0, 8, width, height, linesize, 0, 0);
  // Scale to fit
  GdkPixbuf* scaled=gdk_pixbuf_new(GDK_COLORSPACE_RGB, 0, 8, towidth, toheight);
  double scalex=(double)towidth/(double)width;
  double scaley=(double)toheight/(double)height;
  unsigned int offsetx=0;
  unsigned int offsety=0;
  if(height*4/3>width)
  {
    scalex=scaley;
    offsetx=(towidth-width*toheight/height)/2;
    towidth=width*toheight/height;
    gdk_pixbuf_fill(scaled, 0);
  }
  else if(width*3/4>height)
  {
    scaley=scalex;
    offsety=(toheight-height*towidth/width)/2;
    toheight=height*towidth/width;
    gdk_pixbuf_fill(scaled, 0);
  }
  gdk_pixbuf_scale(gdkframe, scaled, offsetx, offsety, towidth, toheight, offsetx, offsety, scalex, scaley, GDK_INTERP_BILINEAR);
  g_object_unref(gdkframe);
  return scaled;
}

gboolean cam_encode(void* camera_)
{
  CAM* camera=camera_;
  if(camera!=camout_cam){return G_SOURCE_REMOVE;}
  struct camera* cam=camera_find("out");
  if(!cam){return G_SOURCE_REMOVE;}
  if(cam->placeholder) // Remove the placeholder animation if it has it
  {
    g_source_remove(cam->placeholder);
    cam->placeholder=0;
  }
  cam->vctx->width=camsize_out.width;
  cam->vctx->height=camsize_out.height;
  if(!cam->dstframe->data[0])
  {
    cam->dstframe->format=AV_PIX_FMT_YUV420P;
    cam->dstframe->width=camsize_out.width;
    cam->dstframe->height=camsize_out.height;
    av_image_alloc(cam->dstframe->data, cam->dstframe->linesize, camsize_out.width, camsize_out.height, cam->dstframe->format, 1);
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
  cam_getframe(camera, cam->frame->data[0]);
  postprocess(&cam->postproc, cam->frame->data[0], cam->frame->width, cam->frame->height);
  // Update our local display
  GdkPixbuf* oldpixbuf=gtk_image_get_pixbuf(GTK_IMAGE(cam->cam));
  GdkPixbuf* gdkframe=scaleframe(cam->frame->data[0], cam->frame->width, cam->frame->height, cam->frame->linesize[0], camsize_scale.width, camsize_scale.height);
  volume_indicator(gdkframe, cam); // Add volume indicator
  int fd;
  if(hasgreenroom && !greenroom_gotpass)
  {
    fd=greenroompipe_in[1];
    greenroom_indicator(gdkframe); // Add greenroom indicator
  }else{
    fd=tc_client_in[1];
  }
  gtk_image_set_from_pixbuf(GTK_IMAGE(cam->cam), gdkframe);
  g_object_unref(oldpixbuf);
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
  if(gotpacket){return G_SOURCE_CONTINUE;}
  char key=!!(packet.flags&AV_PKT_FLAG_KEY);
  unsigned char frameinfo=(key?0x12:0x22); // In the first 4 bits: 1=keyframe, 2=interframe
  // Send video
  dprintf(fd, "/video %i\n", packet.size+1);
  write(fd, &frameinfo, 1);
  ssize_t w=write(fd, packet.data, packet.size);
if(w!=packet.size){printf("Error: wrote %zi of %i bytes\n", w, packet.size);}

  av_packet_unref(&packet);
  if(camout_delay>100) // Slowly speed up to 10fps, otherwise the flash client won't show it.
  {
    camout_delay-=50;
    g_timeout_add(camout_delay, cam_encode, camera_);
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}

struct
{
  void(*callback)(CAM* cam);
  void(*cancelcallback)(void);
  unsigned int eventsource;
  CAM* current;
  struct size size;
} camselect={.eventsource=0, .current=0};

gboolean camselect_frame(void* x)
{
  if(!camselect.current){return G_SOURCE_CONTINUE;}
  GdkPixbuf* oldpixbuf=gtk_image_get_pixbuf(GTK_IMAGE(campreview.cam));
  GdkPixbuf* gdkframe=scaled_gdk_pixbuf_from_cam(camselect.current, camselect.size.width, camselect.size.height, PREVIEW_MAX_WIDTH, PREVIEW_MAX_HEIGHT);
  gtk_image_set_from_pixbuf(GTK_IMAGE(campreview.cam), gdkframe);
  if(oldpixbuf){g_object_unref(oldpixbuf);}
  return G_SOURCE_CONTINUE;
}

void camselect_open(void(*cb)(CAM*), void(*ccb)(void))
{
  camselect.callback=cb;
  camselect.cancelcallback=ccb;
  // Start the preview
  if(!camselect.eventsource)
  {
    camselect.eventsource=g_timeout_add(100, camselect_frame, 0);
  }
  // Open the currently selected camera (usually starting with the top alternative)
  if(camselect.current){cam_close(camselect.current); camselect.current=0;}
  GtkComboBox* combo=GTK_COMBO_BOX(gtk_builder_get_object(gui, "camselect_combo"));
  camselect_change(combo, 0);
  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "camselection"));
  gtk_widget_show_all(window);
}

void camselect_change(GtkComboBox* combo, void* x)
{
  if(!camselect.eventsource){return;} // Haven't opened the cam selection window yet (happens at startup)
  if(camselect.current){cam_close(camselect.current); camselect.current=0;}
  camselect.current=cam_open(gtk_combo_box_get_active_id(combo));
  if(!camselect.current){return;}
  camselect.size.width=320;
  camselect.size.height=240;
  cam_resolution(camselect.current, (unsigned int*)&camselect.size.width, (unsigned int*)&camselect.size.height);
}

gboolean camselect_cancel(GtkWidget* widget, void* x1, void* x2)
{
  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "camselection"));
  gtk_widget_hide(window);
  g_source_remove(camselect.eventsource);
  camselect.eventsource=0;
  if(camselect.current)
  {
    cam_close(camselect.current);
    camselect.current=0;
  }
  if(camselect.cancelcallback){camselect.cancelcallback();}
  return 1;
}

void camselect_accept(GtkWidget* widget, void* x)
{
  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "camselection"));
  gtk_widget_hide(window);
  g_source_remove(camselect.eventsource);
  camselect.eventsource=0;
  if(camselect.callback)
  {
    camselect.callback(camselect.current);
  }else{
    cam_close(camselect.current);
  }
  camselect.current=0;
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
  GtkWindow* window=GTK_WINDOW(gtk_builder_get_object(gui, "camselection"));
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
  unsigned int boxcount=0;
  unsigned int i;
  for(i=0; i<camcount; ++i)
  {
    if(!cams[i]->flags&CAMFLAG_GREENROOM){++boxcount;}
  }
  if(!boxcount){return;}
  if(!width){width=gtk_widget_get_allocated_width(GTK_WIDGET(gtk_builder_get_object(gui, "main")));}
  if(!height){height=gtk_widget_get_allocated_height(GTK_WIDGET(gtk_builder_get_object(gui, "camerascroll")));}

  GtkRequisition label;
  gtk_widget_get_preferred_size(cams[0]->label, &label, 0);
  camsize_scale.width=1;
  camsize_scale.height=1;
  unsigned int rowcount=1;
  unsigned int rows;
  for(rows=1; rows<=boxcount; ++rows)
  {
    struct size scale;
    unsigned int cams_per_row=boxcount/rows;
    if(boxcount%rows){++cams_per_row;}
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

  if(rowcount!=camrowcount || changedcams) // Changed the number of rows, shuffle everything around to fit. Or added/removed a camera, in which case we need to shuffle things around anyway
  {
    for(i=0; i<camcount; ++i)
    {
      if(cams[i]->flags&CAMFLAG_GREENROOM){continue;}
      g_object_ref(cams[i]->box); // Increase reference counts so that they are not deallocated while they are temporarily detached from the rows
      GtkContainer* parent=GTK_CONTAINER(gtk_widget_get_parent(cams[i]->box));
      if(parent){gtk_container_remove(parent, cams[i]->box);}
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
    unsigned int cams_per_row=boxcount/camrowcount;
    if(boxcount%camrowcount){++cams_per_row;}
    unsigned int index=0;
    for(i=0; i<camcount; ++i)
    {
      if(cams[i]->flags&CAMFLAG_GREENROOM){continue;}
      gtk_box_pack_start(GTK_BOX(camrows[index/cams_per_row]), cams[i]->box, 0, 0, 0);
      ++index;
      g_object_unref(cams[i]->box); // Decrease reference counts once they're attached again
    }
  }
  // libswscale doesn't handle unreasonably small sizes well
  if(camsize_scale.width<8){camsize_scale.width=8;}
  if(camsize_scale.height<6){camsize_scale.height=6;}
  // Rescale current images to fit
  for(i=0; i<camcount; ++i)
  {
    if(cams[i]->flags&CAMFLAG_GREENROOM){continue;}
    GdkPixbuf* pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(cams[i]->cam));
    if(!pixbuf){continue;}
    GdkPixbuf* old=pixbuf;
    pixbuf=gdk_pixbuf_scale_simple(pixbuf, camsize_scale.width, camsize_scale.height, GDK_INTERP_BILINEAR);
    gtk_image_set_from_pixbuf(GTK_IMAGE(cams[i]->cam), pixbuf);
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
  volume_indicator(pixbuf, cam); // Add volume indicator
  gtk_image_set_from_pixbuf(GTK_IMAGE(cam->cam), pixbuf);
  if(oldpixbuf){g_object_unref(oldpixbuf);}
  return G_SOURCE_CONTINUE;
}

GdkPixbuf* scaled_gdk_pixbuf_from_cam(CAM* cam, unsigned int width, unsigned int height, unsigned int maxwidth, unsigned int maxheight)
{
  void* buf=malloc(width*height*3);
  cam_getframe(cam, buf);
  GdkPixbuf* gdkframe=gdk_pixbuf_new_from_data(buf, GDK_COLORSPACE_RGB, 0, 8, width, height, width*3, 0, freebuffer);
  if(height>maxheight || width>maxwidth) // Scale if the input is huge
  {
    GdkPixbuf* oldframe=gdkframe;
    if(height*maxwidth/width>maxheight)
    {
      gdkframe=gdk_pixbuf_scale_simple(gdkframe, width*maxheight/height, maxheight, GDK_INTERP_BILINEAR);
    }else{
      gdkframe=gdk_pixbuf_scale_simple(gdkframe, maxwidth, height*maxwidth/width, GDK_INTERP_BILINEAR);
    }
    g_object_unref(oldframe);
  }
  return gdkframe;
}

#ifdef HAVE_PULSEAUDIO
extern void justwait(int);
void* audiothread_in(void* fdp)
{
  int fd=*(int*)fdp;
  pa_simple* pulse;
  pa_sample_spec pulsespec;
  pulsespec.format=PA_SAMPLE_FLOAT32;
  pulsespec.channels=1;
  pulsespec.rate=44100;
  signal(SIGCHLD, SIG_DFL); // Briefly revert to the default handler to not break pulseaudio's autospawn feature
  pulse=pa_simple_new(0, "tc_client-gtk", PA_STREAM_RECORD, 0, "mic", &pulsespec, 0, 0, 0);
  signal(SIGCHLD, justwait);
  if(!pulse){return 0;}
  char buf[1024];
  // Just read/listen and write to the main thread until either reading/listening or writing fails
  while(1)
  {
    if(pa_simple_read(pulse, buf, 1024, 0)<0){break;}
    if(write(fd, buf, 1024)<1024){break;}
  }
  pa_simple_free(pulse);
  close(fd);
  return 0;
}

gboolean mic_encode(GIOChannel* iochannel, GIOCondition condition, gpointer datap)
{
  static AVFrame* micframe=0;
  static AVCodecContext* avctx=0;
  if(!micframe)
  {
    micframe=av_frame_alloc();
    micframe->format=AV_SAMPLE_FMT_FLT;
    micframe->sample_rate=44100;
    unsigned int i;
    for(i=0; i<AV_NUM_DATA_POINTERS; ++i)
    {
      micframe->data[i]=0;
      micframe->linesize[i]=0;
    }
    AVCodec* encoder=avcodec_find_encoder(AV_CODEC_ID_NELLYMOSER);
    avctx=avcodec_alloc_context3(encoder);
    avctx->sample_fmt=AV_SAMPLE_FMT_FLT;
    avctx->sample_rate=44100;
    avctx->channels=1;
    avctx->time_base.num=1;
    avctx->time_base.den=10;
    avcodec_open2(avctx, encoder, 0);
  }
  // Read up to 1024 bytes (256 samples) and start encoding
  unsigned char buf[1024];
  gsize r;
  int status=g_io_channel_read_chars(iochannel, (char*)buf, 1024, &r, 0);
  if(status!=G_IO_STATUS_NORMAL){return 0;}

  struct camera* cam=camera_find("out");
  // If push-to-talk is enabled but not pushed, return as soon as we've consumed the incoming data
  if(pushtotalk_enabled && !pushtotalk_pushed)
  {
    if(cam){cam->volume=0;}
    return 1;
  }
  if(cam)
  {
    camera_calcvolume(cam, (float*)buf, r/sizeof(float));
  }
  micframe->nb_samples=r/4; // 32bit floats
  micframe->data[0]=buf;
  micframe->linesize[0]=r;
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
  avcodec_send_frame(avctx, micframe);
  if(avcodec_receive_packet(avctx, &packet)){return 1;}
  unsigned char frameinfo=0x6c; // 6=Nellymoser, 3<<2=44100 samplerate
  // Send audio
  int fd=((hasgreenroom && !greenroom_gotpass)?greenroompipe_in[1]:tc_client_in[1]);
  dprintf(fd, "/audio %i\n", packet.size+1);
  write(fd, &frameinfo, 1);
  write(fd, packet.data, packet.size);

  av_packet_unref(&packet);
  return 1;
}
#endif

void camera_calcvolume(struct camera* cam, float* samples, unsigned int samplecount)
{
  unsigned int i;
  float max=0;
  float min=99999;
  for(i=0; i<samplecount; ++i)
  {
    if(samples[i]>max){max=samples[i];}
    if(samples[i]<min){min=samples[i];}
  }
  float diff=max-min;
  if(diff>cam->volume || cam->volumeold){cam->volume=diff; cam->volumeold=0;}
}

void volume_indicator(GdkPixbuf* frame, struct camera* cam)
{
  if(!frame){return;}
  if(cam->volumeold>10){return;} // Not sending any audio anymore
  guchar* pixels=gdk_pixbuf_get_pixels(frame);
  unsigned int channels=gdk_pixbuf_get_n_channels(frame);
  unsigned int stride=gdk_pixbuf_get_rowstride(frame);
  unsigned int width=gdk_pixbuf_get_width(frame);
  unsigned int height=gdk_pixbuf_get_height(frame);
  unsigned int size_x=width/24;
  unsigned int size_y=height/5;
  unsigned int pos_x=width*47/48-size_x;
  unsigned int pos_y=height*47/48-size_y;
  int volumebar=size_y-cam->volume*size_y;
  if(volumebar<0){volumebar=0;}
  unsigned int x, y;
  for(y=0; y<size_y; ++y)
  {
    for(x=0; x<size_x; ++x)
    {
      if(y>=volumebar && x>=size_x/8 && x<size_x-size_x/8)
      { // Green bar
        pixels[stride*(y+pos_y)+(x+pos_x)*channels]=0x0;
        pixels[stride*(y+pos_y)+(x+pos_x)*channels+1]=0xff;
        pixels[stride*(y+pos_y)+(x+pos_x)*channels+2]=0x0;
      }else{ // Gray background
        pixels[stride*(y+pos_y)+(x+pos_x)*channels]=0x80;
        pixels[stride*(y+pos_y)+(x+pos_x)*channels+1]=0x80;
        pixels[stride*(y+pos_y)+(x+pos_x)*channels+2]=0x80;
      }
    }
  }
  ++cam->volumeold;
}

void camera_decode(struct camera* cam, AVPacket* pkt, unsigned int width, unsigned int height)
{
  if(!cam->vctx)
  {
    AVCodec* codec=avcodec_find_decoder(AV_CODEC_ID_FLV1);
    cam->vctx=avcodec_alloc_context3(codec);
    avcodec_open2(cam->vctx, codec, 0);
  }
  int gotframe;
  avcodec_send_packet(cam->vctx, pkt);
  gotframe=avcodec_receive_frame(cam->vctx, cam->frame);
  if(gotframe){return;}

  if(cam->placeholder) // Remove the placeholder animation if it has it
  {
    g_source_remove(cam->placeholder);
    cam->placeholder=0;
  }
  // Scale and convert to RGB24 format
  unsigned int bufsize=av_image_get_buffer_size(AV_PIX_FMT_RGB24, cam->frame->width, cam->frame->height, 1);
  unsigned char buf[bufsize];
  cam->dstframe->data[0]=buf;
  cam->dstframe->linesize[0]=cam->frame->width*3;
  struct SwsContext* swsctx=sws_getContext(cam->frame->width, cam->frame->height, cam->frame->format, cam->frame->width, cam->frame->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, 0, 0, 0);
  sws_scale(swsctx, (const uint8_t*const*)cam->frame->data, cam->frame->linesize, 0, cam->frame->height, cam->dstframe->data, cam->dstframe->linesize);
  sws_freeContext(swsctx);
  postprocess(&cam->postproc, cam->dstframe->data[0], cam->frame->width, cam->frame->height);

  GdkPixbuf* oldpixbuf=gtk_image_get_pixbuf(GTK_IMAGE(cam->cam));
  GdkPixbuf* gdkframe=scaleframe(cam->dstframe->data[0], cam->frame->width, cam->frame->height, cam->dstframe->linesize[0], width, height);
  volume_indicator(gdkframe, cam);
  gtk_image_set_from_pixbuf(GTK_IMAGE(cam->cam), gdkframe);
  if(oldpixbuf){g_object_unref(oldpixbuf);}
}

#ifdef HAVE_LIBAO
void mic_decode(struct camera* cam, AVPacket* pkt)
{
  int gotframe;
  avcodec_send_packet(cam->actx, pkt);
  gotframe=avcodec_receive_frame(cam->actx, cam->frame);
  if(gotframe){return;}
  camera_calcvolume(cam, (float*)cam->frame->data[0], cam->frame->nb_samples);
  unsigned int samplecount=cam->frame->nb_samples*SAMPLERATE_OUT/cam->samplerate;
  int16_t outbuf[samplecount];
  void* outdata[]={outbuf, 0};
#ifdef HAVE_AVRESAMPLE
  int outlen=avresample_convert(cam->resamplectx, (void*)outdata, samplecount*sizeof(uint8_t), samplecount, cam->frame->data, cam->frame->linesize[0], cam->frame->nb_samples);
#else
  int outlen=swr_convert(cam->swrctx, (void*)outdata, samplecount, (const uint8_t**)cam->frame->data, cam->frame->nb_samples);
#endif
  if(outlen>0){camera_playsnd(cam, outbuf, outlen);}
}
#endif
