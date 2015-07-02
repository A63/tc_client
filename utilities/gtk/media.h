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
};
extern struct camera campreview;
extern struct camera* cams;
extern unsigned int camcount;
extern pid_t camproc;

#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
extern void camera_playsnd(int audiopipe, struct camera* cam, short* samples, unsigned int samplecount);
#endif
extern void camera_remove(const char* nick);
extern void camera_removebynick(const char* nick);
extern struct camera* camera_find(const char* id);
extern struct camera* camera_findbynick(const char* nick);
extern struct camera* camera_new(void);
extern void camera_cleanup(void);
#ifdef HAVE_CAM
extern GIOChannel* camthread(const char* name, AVCodec* vencoder, unsigned int delay);
extern void camselect_change(GtkComboBox* combo, AVCodec* vencoder);
extern gboolean camselect_cancel(GtkWidget* widget, void* x1, void* x2);
extern void camselect_accept(GtkWidget* widget, AVCodec* vencoder);
#endif
