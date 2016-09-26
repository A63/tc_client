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
#include <libavcodec/avcodec.h>
#include "../libcamera/camera.h"
#include "postproc.h"
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
};
struct size
{
  int width;
  int height;
};
extern struct camera campreview;
extern struct camera* cams;
extern unsigned int camcount;
extern struct size camsize_out;
extern struct size camsize_scale;
extern GtkWidget* cambox;
extern GdkPixbufAnimation* camplaceholder;
extern GdkPixbufAnimationIter* camplaceholder_iter;
extern CAM* camout_cam;

#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
extern void camera_playsnd(int audiopipe, struct camera* cam, short* samples, unsigned int samplecount);
#endif
extern void camera_remove(const char* id, char isnick);
extern struct camera* camera_find(const char* id);
extern struct camera* camera_findbynick(const char* nick);
extern struct camera* camera_new(const char* nick, const char* id);
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
