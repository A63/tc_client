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

struct viddata
{
  AVFrame* frame;
  AVFrame* dstframe;
  GtkWidget* cam;
  AVCodecContext* ctx;
  char* camnick;
};

int tc_client[2];
int tc_client_in[2];

char buf[1024];
gboolean handledata(GIOChannel* channel, GIOCondition condition, gpointer datap)
{
  struct viddata* data=datap;
  int i;
  for(i=0; i<1023; ++i)
  {
    if(read(tc_client[0], &buf[i], 1)<1){printf("No more data\n"); gtk_main_quit(); return 0;}
    if(buf[i]=='\r'||buf[i]=='\n'){break;}
  }
  buf[i]=0;
  // Start stream once we're properly connected
  if(!strncmp(buf, "Connection ID: ", 10)){dprintf(tc_client_in[1], "/opencam %s\n", data->camnick); return 1;}
  if(strncmp(buf, "Video: ", 7)){printf("Got '%s'\n", buf); return 1;} // Ignore anything that isn't video
  char* sizestr=strchr(&buf[7], ' ');
  unsigned int size=strtoul(&sizestr[1], 0, 0);
  // Mostly ignore the first byte (contains frame type (e.g. keyframe etc.) in 4 bits and codec in the other 4, but we assume FLV1)
  --size;
  AVPacket pkt;
  av_init_packet(&pkt);
  pkt.data=malloc(size);
  read(tc_client[0], pkt.data, 1); // Skip
// printf("Frametype-frame: %x\n", ((unsigned int)pkt.data[0]&0xf0)/16);
// printf("Frametype-codec: %x\n", (unsigned int)pkt.data[0]&0xf);
  if((pkt.data[0]&0xf)!=2) // Not FLV1, get data but discard it
  {
    read(tc_client[0], pkt.data, size);
    return 1;
  }
  read(tc_client[0], pkt.data, size);
  pkt.size=size;
  int gotframe;
  avcodec_decode_video2(data->ctx, data->frame, &gotframe, &pkt);
  free(pkt.data);
  if(!gotframe){return 1;}

  // Convert to RGB24 format
  unsigned int bufsize=avpicture_get_size(PIX_FMT_RGB24, data->frame->width, data->frame->height);
  unsigned char buf[bufsize];
  data->dstframe->data[0]=buf;
  data->dstframe->linesize[0]=data->frame->width*3;
  struct SwsContext* swsctx=sws_getContext(data->frame->width, data->frame->height, data->frame->format, data->frame->width, data->frame->height, AV_PIX_FMT_RGB24, 0, 0, 0, 0);
  sws_scale(swsctx, (const uint8_t*const*)data->frame->data, data->frame->linesize, 0, data->frame->height, data->dstframe->data, data->dstframe->linesize);

  GdkPixbuf* gdkframe=gdk_pixbuf_new_from_data(data->dstframe->data[0], GDK_COLORSPACE_RGB, 0, 8, data->frame->width, data->frame->height, data->dstframe->linesize[0], 0, 0);
  gtk_image_set_from_pixbuf(GTK_IMAGE(data->cam), gdkframe);
  g_object_unref(gdkframe);

  return 1;
}

int main(int argc, char** argv)
{
  struct viddata data={av_frame_alloc(),av_frame_alloc()};
  data.camnick=argv[1];
  // Init the decoder
  avcodec_register_all();
  AVCodec* decoder=avcodec_find_decoder(AV_CODEC_ID_FLV1);
  data.ctx=avcodec_alloc_context3(decoder);
  avcodec_open2(data.ctx, decoder, 0);

  gtk_init(&argc, &argv);
  GtkWidget* w=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(w, "destroy", gtk_main_quit, 0);
  data.cam=gtk_image_new();
  gtk_container_add(GTK_CONTAINER(w), data.cam);
  gtk_widget_show_all(w);

  pipe(tc_client);
  pipe(tc_client_in);
  if(!fork())
  {
    close(tc_client[0]);
    close(tc_client_in[1]);
    dup2(tc_client[1], 1);
    dup2(tc_client_in[0], 0);
    argv[1]="./tc_client";
    execv("./tc_client", &argv[1]);
  }
  close(tc_client[1]);
  close(tc_client_in[0]);
  GIOChannel* tcchannel=g_io_channel_unix_new(tc_client[0]);
  g_io_channel_set_encoding(tcchannel, 0, 0);
  g_io_add_watch(tcchannel, G_IO_IN, handledata, &data);

  gtk_main();

  free(data.frame->data[0]);
  av_frame_free(&data.frame);
  return 0;
}
