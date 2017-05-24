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
#ifndef MEDIA_H
#define MEDIA_H
#include <glib.h>
#include <gtk/gtk.h>
#include <libavcodec/avcodec.h>
#ifdef HAVE_AVRESAMPLE
  #include <libavresample/avresample.h>
#elif defined(HAVE_SWRESAMPLE)
  #include <libswresample/swresample.h>
#endif
#include "../libcamera/camera.h"
#include "postproc.h"
#define SAMPLERATE_OUT 11025 // 11025 is the most common input sample rate, and it is more CPU-efficient to keep it that way than upsampling or downsampling it when converting from flt to s16
#define CAMFLAG_NONE      0
#define CAMFLAG_GREENROOM 1
struct camera
{
  AVFrame* frame;
  AVFrame* dstframe;
  GtkWidget* cam;
  AVCodecContext* vctx;
  AVCodecContext* actx;
  short* samples;
  unsigned int samplecount;
  char* id;
  char* nick;
  GtkWidget* box; // holds label and cam
  GtkWidget* label;
  struct postproc_ctx postproc;
  unsigned int placeholder;
#ifdef HAVE_AVRESAMPLE
  AVAudioResampleContext* resamplectx;
#elif defined(HAVE_SWRESAMPLE)
  SwrContext* swrctx;
#endif
  unsigned int samplerate;
  float volume;
  unsigned int volumeold;
  unsigned char flags;
};
struct size
{
  int width;
  int height;
};
extern struct camera campreview;
extern struct size camsize_out;
extern struct size camsize_scale;
extern GtkWidget* cambox;
extern GdkPixbufAnimation* camplaceholder;
extern GdkPixbufAnimationIter* camplaceholder_iter;
extern CAM* camout_cam;
extern char pushtotalk_enabled;
extern char pushtotalk_pushed;
extern unsigned int camout_delay;

#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
extern void camera_playsnd(struct camera* cam, int16_t* samples, unsigned int samplecount);
extern gboolean audiomixer(void* p);
#endif
extern void camera_remove(const char* id, char isnick);
extern struct camera* camera_find(const char* id);
extern struct camera* camera_findbynick(const char* nick);
extern struct camera* camera_new(const char* nick, const char* id, unsigned char flags);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
extern char camera_init_audio(struct camera* cam, uint8_t frameinfo);
#endif
extern void camera_cleanup(void);
extern void freebuffer(guchar* pixels, gpointer data);
extern void startcamout(CAM* cam);
extern gboolean cam_encode(void* camera_);
extern void camselect_open(void(*cb)(CAM*), void(*ccb)(void));
extern void camselect_change(GtkComboBox* combo, void* x);
extern gboolean camselect_cancel(GtkWidget* widget, void* x1, void* x2);
extern void camselect_accept(GtkWidget* widget, void* x);
extern const char* camselect_file(void);
extern void camera_postproc(struct camera* cam, unsigned char* buf, unsigned int width, unsigned int height);
extern void updatescaling(unsigned int width, unsigned int height, char changedcams);
extern gboolean camplaceholder_update(void* id);
extern GdkPixbuf* scaled_gdk_pixbuf_from_cam(CAM* cam, unsigned int width, unsigned int height, unsigned int maxwidth, unsigned int maxheight);
extern void* audiothread_in(void* fdp);
extern gboolean mic_encode(GIOChannel* iochannel, GIOCondition condition, gpointer datap);
extern void camera_calcvolume(struct camera* cam, float* samples, unsigned int samplecount);
extern void volume_indicator(GdkPixbuf* frame, struct camera* cam);
extern void camera_decode(struct camera* cam, AVPacket* pkt, unsigned int width, unsigned int height);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
extern void mic_decode(struct camera* cam, AVPacket* pkt);
#endif
#endif
