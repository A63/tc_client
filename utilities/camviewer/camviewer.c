/*
    camviewer, a sample application to view tinychat cam streams
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
#include <unistd.h>
#include <fcntl.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <gtk/gtk.h>

#if GTK_MAJOR_VERSION==2
  #define GTK_ORIENTATION_HORIZONTAL 0
  #define GTK_ORIENTATION_VERTICAL 1
  GtkWidget* gtk_box_new(int vertical, int spacing)
  {
    if(vertical)
    {
      return gtk_vbox_new(1, spacing);
    }else{
      return gtk_hbox_new(1, spacing);
    }
  }
#endif

struct camera
{
  AVFrame* frame;
  AVFrame* dstframe;
  GtkWidget* cam;
  AVCodecContext* ctx;
  char* id;
  char* nick;
  GtkWidget* box; // holds label and cam
};

struct viddata
{
  struct camera* cams;
  unsigned int camcount;
  GtkWidget* box;
  AVCodec* decoder;
};

int tc_client[2];
int tc_client_in[2];

char buf[1024];
gboolean handledata(GIOChannel* channel, GIOCondition condition, gpointer datap)
{
  struct viddata* data=datap;
  unsigned int i;
  for(i=0; i<1023; ++i)
  {
    if(read(tc_client[0], &buf[i], 1)<1){printf("No more data\n"); gtk_main_quit(); return 0;}
    if(buf[i]=='\r'||buf[i]=='\n'){break;}
  }
  buf[i]=0;
  // Start streams once we're properly connected
  if(!strncmp(buf, "Currently on cam: ", 18))
  {
    char* next=&buf[16];
    while(next)
    {
      char* user=&next[2];
      next=strstr(user, ", ");
      if(!user[0]){continue;}
      if(next){next[0]=0;}
      dprintf(tc_client_in[1], "/opencam %s\n", user);
    }
    return 1;
  }
  char* space=strchr(buf, ' ');
  // Start a stream when someone cams up
  if(space && !strcmp(space, " cammed up"))
  {
    space[0]=0;
    dprintf(tc_client_in[1], "/opencam %s\n", buf);
    return 1;
  }
  if(!strncmp(buf, "Starting media stream for ", 26))
  {
    char* nick=&buf[26];
    char* id=strstr(nick, " (");
    if(!id){return 1;}
    id[0]=0;
    id=&id[2];
    char* idend=strchr(id, ')');
    if(!idend){return 1;}
    idend[0]=0;
    ++data->camcount;
    data->cams=realloc(data->cams, sizeof(struct camera)*data->camcount);
    struct camera* cam=&data->cams[data->camcount-1];
    cam->frame=av_frame_alloc();
    cam->dstframe=av_frame_alloc();
    cam->nick=strdup(nick);
    cam->id=strdup(id);
    cam->ctx=avcodec_alloc_context3(data->decoder);
    avcodec_open2(cam->ctx, data->decoder, 0);
    cam->cam=gtk_image_new();
    cam->box=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(cam->box), cam->cam, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(cam->box), gtk_label_new(cam->nick), 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(data->box), cam->box, 0, 0, 0);
    gtk_widget_show_all(cam->box);
    return 1;
  }
  if(!strncmp(buf, "VideoEnd: ", 10))
  {
    for(i=0; i<data->camcount; ++i)
    {
      if(!strcmp(data->cams[i].id, &buf[10]))
      {
        gtk_widget_destroy(data->cams[i].box);
        av_frame_free(&data->cams[i].frame);
        avcodec_free_context(&data->cams[i].ctx);
        free(data->cams[i].id);
        free(data->cams[i].nick);
        --data->camcount;
        memmove(&data->cams[i], &data->cams[i+1], (data->camcount-i)*sizeof(struct camera));
        break;
      }
    }
    return 1;
  }
  if(!strncmp(buf, "Audio: ", 7))
  {
    char* sizestr=strchr(&buf[7], ' ');
    if(!sizestr){return 1;}
    unsigned int size=strtoul(&sizestr[1], 0, 0);
    unsigned char frameinfo;
    read(tc_client[0], &frameinfo, 1);
    unsigned char buf[size];
    unsigned int pos=0;
    while(pos<size)
    {
      pos+=read(tc_client[0], &buf[pos], size-pos);
    }
// TODO: decode and send to a sound lib (libao)
  }
  if(strncmp(buf, "Video: ", 7)){printf("Got '%s'\n", buf); return 1;} // Ignore anything else that isn't video
  char* sizestr=strchr(&buf[7], ' ');
  if(!sizestr){return 1;}
  sizestr[0]=0;
  // Find the camera representation for the given ID
  struct camera* cam=0;
  for(i=0; i<data->camcount; ++i)
  {
    if(!strcmp(data->cams[i].id, &buf[7])){cam=&data->cams[i]; break;}
  }
  unsigned int size=strtoul(&sizestr[1], 0, 0);
  // Mostly ignore the first byte (contains frame type (e.g. keyframe etc.) in 4 bits and codec in the other 4)
  --size;
  AVPacket pkt;
  av_init_packet(&pkt);
  unsigned char databuf[size+4];
  pkt.data=databuf;
  unsigned char frameinfo;
  read(tc_client[0], &frameinfo, 1);
// printf("Frametype-frame: %x\n", ((unsigned int)frameinfo&0xf0)/16);
// printf("Frametype-codec: %x\n", (unsigned int)frameinfo&0xf);
  unsigned int pos=0;
  while(pos<size)
  {
    pos+=read(tc_client[0], pkt.data+pos, size-pos);
  }
  if((frameinfo&0xf)!=2){return 1;} // Not FLV1, get data but discard it
  if(!cam){printf("No cam found with ID '%s'\n", &buf[7]); free(pkt.data); return 1;}
  pkt.size=size;
  int gotframe;
  avcodec_decode_video2(cam->ctx, cam->frame, &gotframe, &pkt);
  if(!gotframe){return 1;}

  // Convert to RGB24 format
  unsigned int bufsize=avpicture_get_size(PIX_FMT_RGB24, cam->frame->width, cam->frame->height);
  unsigned char buf[bufsize];
  cam->dstframe->data[0]=buf;
  cam->dstframe->linesize[0]=cam->frame->width*3;
  struct SwsContext* swsctx=sws_getContext(cam->frame->width, cam->frame->height, cam->frame->format, cam->frame->width, cam->frame->height, AV_PIX_FMT_RGB24, 0, 0, 0, 0);
  sws_scale(swsctx, (const uint8_t*const*)cam->frame->data, cam->frame->linesize, 0, cam->frame->height, cam->dstframe->data, cam->dstframe->linesize);
  sws_freeContext(swsctx);

  GdkPixbuf* gdkframe=gdk_pixbuf_new_from_data(cam->dstframe->data[0], GDK_COLORSPACE_RGB, 0, 8, cam->frame->width, cam->frame->height, cam->dstframe->linesize[0], 0, 0);
  gtk_image_set_from_pixbuf(GTK_IMAGE(cam->cam), gdkframe);
  // Make sure it gets redrawn in time
  gdk_window_process_updates(gtk_widget_get_window(cam->cam), 1);

  g_object_unref(gdkframe);
  return 1;
}

int main(int argc, char** argv)
{
  struct viddata data={0,0,0,0};
  avcodec_register_all();
  data.decoder=avcodec_find_decoder(AV_CODEC_ID_FLV1);

  gtk_init(&argc, &argv);
  GtkWidget* w=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(w, "destroy", gtk_main_quit, 0);
  data.box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add(GTK_CONTAINER(w), data.box);
  gtk_widget_show_all(w);

  pipe(tc_client);
  pipe(tc_client_in);
  if(!fork())
  {
    close(tc_client[0]);
    close(tc_client_in[1]);
    dup2(tc_client[1], 1);
    dup2(tc_client_in[0], 0);
    argv[0]="./tc_client";
    execv("./tc_client", argv);
  }
  close(tc_client[1]);
  close(tc_client_in[0]);
  GIOChannel* tcchannel=g_io_channel_unix_new(tc_client[0]);
  g_io_channel_set_encoding(tcchannel, 0, 0);
  unsigned int channel_id=g_io_add_watch(tcchannel, G_IO_IN, handledata, &data);

  gtk_main();

  g_source_remove(channel_id);
  g_io_channel_shutdown(tcchannel, 0, 0);
  unsigned int i;
  for(i=0; i<data.camcount; ++i)
  {
    av_frame_free(&data.cams[i].frame);
    avcodec_free_context(&data.cams[i].ctx);
    free(data.cams[i].id);
    free(data.cams[i].nick);
  }
  free(data.cams);
  return 0;
}
