/*
    camviewer, a sample application to view tinychat cam streams
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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #include <wtypes.h>
  #include <winbase.h>
#else
  #include <sys/prctl.h>
#endif
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#if LIBAVUTIL_VERSION_MAJOR>50 || (LIBAVUTIL_VERSION_MAJOR==50 && LIBAVUTIL_VERSION_MINOR>37)
  #include <libavutil/imgutils.h>
#else
  #include <libavcore/imgutils.h>
#endif
// Use libavresample instead if available
#ifdef HAVE_AVRESAMPLE
  #include <libavutil/opt.h>
  #include <libavresample/avresample.h>
  #include <ao/ao.h>
#elif defined(HAVE_SWRESAMPLE)
  #include <libswresample/swresample.h>
  #include <ao/ao.h>
#endif
#include <gtk/gtk.h>
#ifndef _WIN32
  #include "../libcamera/camera.h"
#endif
#include "../compat.h"

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

#ifdef _WIN32
SECURITY_ATTRIBUTES sa={
  .nLength=sizeof(SECURITY_ATTRIBUTES),
  .bInheritHandle=1,
  .lpSecurityDescriptor=0
};
#endif

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
};

struct viddata
{
  struct camera* cams;
  unsigned int camcount;
  GtkWidget* box;
  AVCodec* vdecoder;
  AVCodec* vencoder;
  AVCodec* adecoder;
#ifdef HAVE_AVRESAMPLE
  int audiopipe;
  AVAudioResampleContext* resamplectx;
#elif defined(HAVE_SWRESAMPLE)
  int audiopipe;
  SwrContext* swrctx;
#endif
};

int tc_client[2];
int tc_client_in[2];

#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
// Experimental mixer, not sure if it really works
void camera_playsnd(struct viddata* data, struct camera* cam, short* samples, unsigned int samplecount)
{
  if(cam->samples)
  {
// int sources=1;
    unsigned int i;
    for(i=0; i<data->camcount; ++i)
    {
      if(!data->cams[i].samples){continue;}
      if(cam==&data->cams[i]){continue;}
      unsigned j;
      for(j=0; j<cam->samplecount && j<data->cams[i].samplecount; ++j)
      {
        cam->samples[j]+=data->cams[i].samples[j];
      }
      free(data->cams[i].samples);
      data->cams[i].samples=0;
// ++sources;
    }
    write(data->audiopipe, cam->samples, cam->samplecount*sizeof(short));
    free(cam->samples);
// printf("Mixed sound from %i sources (cam: %p)\n", sources, cam);
  }
  cam->samples=malloc(samplecount*sizeof(short));
  memcpy(cam->samples, samples, samplecount*sizeof(short));
  cam->samplecount=samplecount;
}
#endif

void camera_remove(struct viddata* data, const char* nick)
{
  unsigned int i;
  for(i=0; i<data->camcount; ++i)
  {
    if(!strcmp(data->cams[i].id, nick))
    {
      gtk_widget_destroy(data->cams[i].box);
      av_frame_free(&data->cams[i].frame);
      avcodec_free_context(&data->cams[i].vctx);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
      avcodec_free_context(&data->cams[i].actx);
#endif
      free(data->cams[i].id);
      free(data->cams[i].nick);
      --data->camcount;
      memmove(&data->cams[i], &data->cams[i+1], (data->camcount-i)*sizeof(struct camera));
      break;
    }
  }
}

void captcha_done(GtkButton* button, GtkWidget* box)
{
  gtk_widget_destroy(box);
  write(tc_client_in[1], "\n", 1);
}

char buf[1024];
gboolean handledata(GIOChannel* channel, GIOCondition condition, gpointer datap)
{
  struct viddata* data=datap;
  gsize r;
  unsigned int i;
  for(i=0; i<1023; ++i)
  {
    g_io_channel_read_chars(channel, &buf[i], 1, &r, 0);
    if(r<1){printf("No more data\n"); gtk_main_quit(); return 0;}
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
  // Make sure the cam goes away when a user leaves
  else if(space && !strcmp(space, " left the channel"))
  {
    space[0]=0;
    camera_remove(data, buf);
    return 1;
  }
  if(!strncmp(buf, "Captcha: ", 9))
  {
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* label=gtk_label_new(0);
    char link[snprintf(0,0,"Captcha: <a href=\"%s\">%s</a>", &buf[9], &buf[9])+1];
    sprintf(link, "Captcha: <a href=\"%s\">%s</a>", &buf[9], &buf[9]);
    gtk_label_set_markup(GTK_LABEL(label), link);
    gtk_box_pack_start(GTK_BOX(box), label, 0, 0, 0);
    GtkWidget* button=gtk_button_new_with_label("Done");
    g_signal_connect(button, "clicked", G_CALLBACK(captcha_done), box);
    gtk_box_pack_start(GTK_BOX(box), button, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(data->box), box, 0, 0, 0);
    gtk_widget_show_all(box);
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
    cam->vctx=avcodec_alloc_context3(data->vdecoder);
    avcodec_open2(cam->vctx, data->vdecoder, 0);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
    cam->actx=avcodec_alloc_context3(data->adecoder);
    avcodec_open2(cam->actx, data->adecoder, 0);
    cam->samples=0;
#endif
    cam->cam=gtk_image_new();
    cam->box=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(cam->box), cam->cam, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(cam->box), gtk_label_new(cam->nick), 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(data->box), cam->box, 0, 0, 0);
    gtk_widget_show_all(cam->box);
    return 1;
  }
  if(!strcmp(buf, "Starting outgoing media stream"))
  {
    ++data->camcount;
    data->cams=realloc(data->cams, sizeof(struct camera)*data->camcount);
    struct camera* cam=&data->cams[data->camcount-1];
    cam->frame=av_frame_alloc();
    cam->dstframe=av_frame_alloc();
    cam->nick=strdup("You");
    cam->id=strdup("out");
    cam->vctx=avcodec_alloc_context3(data->vdecoder);
    avcodec_open2(cam->vctx, data->vdecoder, 0);
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
    camera_remove(data, &buf[10]);
    return 1;
  }
  if(!strncmp(buf, "Audio: ", 7))
  {
    char* sizestr=strchr(&buf[7], ' ');
    if(!sizestr){return 1;}
    sizestr[0]=0;
    unsigned int size=strtoul(&sizestr[1], 0, 0);
    if(!size){return 1;}
    unsigned char frameinfo;
    g_io_channel_read_chars(channel, (gchar*)&frameinfo, 1, 0, 0);
    --size; // For the byte we read above
    AVPacket pkt;
    av_init_packet(&pkt);
    unsigned char databuf[size];
    pkt.data=databuf;
    pkt.size=size;
    unsigned int pos=0;
    while(pos<size)
    {
      g_io_channel_read_chars(channel, (gchar*)pkt.data+pos, size-pos, &r, 0);
      pos+=r;
    }
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
    // Find the camera representation for the given ID (for decoder context)
    struct camera* cam=0;
    for(i=0; i<data->camcount; ++i)
    {
      if(!strcmp(data->cams[i].id, &buf[7])){cam=&data->cams[i]; break;}
    }
    if(!cam){printf("No cam found with ID '%s'\n", &buf[7]); return 1;}
    int gotframe;
    avcodec_send_packet(cam->actx, &pkt);
    gotframe=avcodec_receive_frame(cam->actx, cam->frame);
    if(gotframe){return 1;}
  #ifdef HAVE_AVRESAMPLE
    int outlen=avresample_convert(data->resamplectx, cam->frame->data, cam->frame->linesize[0], cam->frame->nb_samples, cam->frame->data, cam->frame->linesize[0], cam->frame->nb_samples);
  #else
    int outlen=swr_convert(data->swrctx, cam->frame->data, cam->frame->nb_samples, (const uint8_t**)cam->frame->data, cam->frame->nb_samples);
  #endif
    if(outlen>0){camera_playsnd(data, cam, (short*)cam->frame->data[0], outlen);}
#endif
    return 1;
  }
  if(strncmp(buf, "Video: ", 7)){printf("Got '%s'\n", buf); fflush(stdout); return 1;} // Ignore anything else that isn't video
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
  if(!size){return 1;}
  // Mostly ignore the first byte (contains frame type (e.g. keyframe etc.) in 4 bits and codec in the other 4)
  --size;
  AVPacket pkt;
  av_init_packet(&pkt);
  unsigned char databuf[size+4];
  pkt.data=databuf;
  unsigned char frameinfo;
  g_io_channel_read_chars(channel, (gchar*)&frameinfo, 1, 0, 0);
// printf("Frametype-frame: %x\n", ((unsigned int)frameinfo&0xf0)/16);
// printf("Frametype-codec: %x\n", (unsigned int)frameinfo&0xf);
  unsigned int pos=0;
  while(pos<size)
  {
    g_io_channel_read_chars(channel, (gchar*)pkt.data+pos, size-pos, &r, 0);
    pos+=r;
  }
  if((frameinfo&0xf)!=2){return 1;} // Not FLV1, get data but discard it
  if(!cam){printf("No cam found with ID '%s'\n", &buf[7]); return 1;}
  pkt.size=size;
  int gotframe;
  avcodec_send_packet(cam->vctx, &pkt);
  gotframe=avcodec_receive_frame(cam->vctx, cam->frame);
  if(gotframe){return 1;}

  // Convert to RGB24 format
  unsigned int bufsize=av_image_get_buffer_size(AV_PIX_FMT_RGB24, cam->frame->width, cam->frame->height, 1);
  unsigned char buf[bufsize];
  cam->dstframe->data[0]=buf;
  cam->dstframe->linesize[0]=cam->frame->width*3;
  struct SwsContext* swsctx=sws_getContext(cam->frame->width, cam->frame->height, cam->frame->format, cam->frame->width, cam->frame->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, 0, 0, 0);
  sws_scale(swsctx, (const uint8_t*const*)cam->frame->data, cam->frame->linesize, 0, cam->frame->height, cam->dstframe->data, cam->dstframe->linesize);
  sws_freeContext(swsctx);

  GdkPixbuf* gdkframe=gdk_pixbuf_new_from_data(cam->dstframe->data[0], GDK_COLORSPACE_RGB, 0, 8, cam->frame->width, cam->frame->height, cam->dstframe->linesize[0], 0, 0);
  gtk_image_set_from_pixbuf(GTK_IMAGE(cam->cam), gdkframe);
  // Make sure it gets redrawn in time
  gdk_window_process_updates(gtk_widget_get_window(cam->cam), 1);

  g_object_unref(gdkframe);
  return 1;
}

#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
void audiothread(int fd)
{
  ao_initialize();
  ao_sample_format samplefmt;
  samplefmt.bits=16;
  samplefmt.rate=22050;
  samplefmt.channels=1;
  samplefmt.byte_format=AO_FMT_NATIVE; // I'm guessing libavcodec decodes it to native
  samplefmt.matrix=0;
  ao_option clientname={.key="client_name", .value="tc_client/camviewer", .next=0};
  ao_device* dev=ao_open_live(ao_default_driver_id(), &samplefmt, &clientname);
  char buf[2048];
  size_t len;
  while((len=read(fd, buf, 2048))>0)
  {
    ao_play(dev, buf, len);
  }
  ao_close(dev);
}
#endif

#ifndef _WIN32
pid_t camproc=0;
void togglecam(GtkButton* button, struct viddata* data)
{
  if(camproc)
  {
    kill(camproc, SIGINT);
    camproc=0;
    gtk_button_set_label(button, "Broadcast cam");
    dprintf(tc_client_in[1], "/camdown\n");
    dprintf(tc_client[1], "VideoEnd: out\n"); // Close our local display
    return;
  }
  unsigned int count;
  char** cams=cam_list(&count);
  if(!count){printf("No camera found\n"); return;}
  // Set up a second pipe to be handled by handledata() to avoid overlap with tc_client's output
  int campipe[2];
  pipe(campipe);
  dprintf(tc_client_in[1], "/camup\n");
  gtk_button_set_label(button, "Stop broadcasting");
// printf("Camming up!\n");
  camproc=fork();
  if(!camproc)
  {
    close(campipe[0]);
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    unsigned int delay=500000;
    // Set up camera
    CAM* cam=cam_open(cams[0]);
    AVCodecContext* ctx=avcodec_alloc_context3(data->vencoder);
    ctx->width=320;
    ctx->height=240;
    cam_resolution(cam, (unsigned int*)&ctx->width, (unsigned int*)&ctx->height);
    ctx->pix_fmt=AV_PIX_FMT_YUV420P;
    ctx->time_base.num=1;
    ctx->time_base.den=10;
    avcodec_open2(ctx, data->vencoder, 0);
    AVFrame* frame=av_frame_alloc();
    frame->format=AV_PIX_FMT_RGB24;
    frame->width=ctx->width;
    frame->height=ctx->height;
    av_image_alloc(frame->data, frame->linesize, ctx->width, ctx->height, frame->format, 1);
    AVPacket packet;
#ifdef AVPACKET_HAS_BUF
    packet.buf=0;
#endif
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

    struct SwsContext* swsctx=sws_getContext(frame->width, frame->height, AV_PIX_FMT_RGB24, frame->width, frame->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);

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
      avcodec_send_frame(ctx, dstframe);
      gotpacket=avcodec_receive_packet(ctx, &packet);
      if(gotpacket){continue;}
      unsigned char frameinfo=0x22; // Note: differentiating between keyframes and non-keyframes seems to break stuff, so let's just go with all being interframes (1=keyframe, 2=interframe, 3=disposable interframe)
      dprintf(tc_client_in[1], "/video %i\n", packet.size+1);
      write(tc_client_in[1], &frameinfo, 1);
      write(tc_client_in[1], packet.data, packet.size);
      // Also send the packet to our main thread so we can see ourselves
      dprintf(campipe[1], "Video: out %i\n", packet.size+1);
      write(campipe[1], &frameinfo, 1);
      write(campipe[1], packet.data, packet.size);

      av_packet_unref(&packet);
    }
    sws_freeContext(swsctx);
    _exit(0);
  }
  close(campipe[1]);
  GIOChannel* channel=g_io_channel_unix_new(campipe[0]);
  g_io_channel_set_encoding(channel, 0, 0);
  g_io_add_watch(channel, G_IO_IN, handledata, data);
}
#endif

int main(int argc, char** argv)
{
  struct viddata data={0,0,0,0,0};
  avcodec_register_all();
  data.vdecoder=avcodec_find_decoder(AV_CODEC_ID_FLV1);
  data.adecoder=avcodec_find_decoder(AV_CODEC_ID_NELLYMOSER);

#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
  #ifdef HAVE_AVRESAMPLE
  data.resamplectx=avresample_alloc_context();
  av_opt_set_int(data.resamplectx, "in_channel_layout", AV_CH_FRONT_CENTER, 0);
  av_opt_set_int(data.resamplectx, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
  // TODO: any way to get the sample rate from the frame/decoder? cam->frame->sample_rate seems to be 0
  av_opt_set_int(data.resamplectx, "in_sample_rate", 11025, 0);
  av_opt_set_int(data.resamplectx, "out_channel_layout", AV_CH_FRONT_CENTER, 0);
  av_opt_set_int(data.resamplectx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
  av_opt_set_int(data.resamplectx, "out_sample_rate", 22050, 0);
  avresample_open(data.resamplectx);
  #else
  data.swrctx=swr_alloc_set_opts(0, AV_CH_FRONT_CENTER, AV_SAMPLE_FMT_S16, 22050, AV_CH_FRONT_CENTER, AV_SAMPLE_FMT_FLT, 11025, 0, 0);
  swr_init(data.swrctx);
  #endif
  int audiopipe[2];
  pipe(audiopipe);
  data.audiopipe=audiopipe[1];
  if(!fork())
  {
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    close(audiopipe[1]);
    audiothread(audiopipe[0]);
    _exit(0);
  }
  close(audiopipe[0]);
#endif

  gtk_init(&argc, &argv);
  GtkWidget* w=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(w, "destroy", gtk_main_quit, 0);
  data.box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add(GTK_CONTAINER(w), data.box);
#ifndef _WIN32
  data.vencoder=avcodec_find_encoder(AV_CODEC_ID_FLV1);
  GtkWidget* cambutton=gtk_button_new_with_label("Broadcast cam");
  g_signal_connect(cambutton, "clicked", G_CALLBACK(togglecam), &data);
  gtk_box_pack_start(GTK_BOX(data.box), cambutton, 0, 0, 0);
#endif
  gtk_widget_show_all(w);

  unsigned int i;
#ifdef _WIN32
  HANDLE h_tc_client0, h_tc_client1;
  CreatePipe(&h_tc_client0, &h_tc_client1, &sa, 0);
  HANDLE h_tc_client_in0, h_tc_client_in1;
  CreatePipe(&h_tc_client_in0, &h_tc_client_in1, &sa, 0);
  tc_client[0]=_open_osfhandle(h_tc_client0, _O_RDONLY);
  tc_client[1]=_open_osfhandle(h_tc_client1, _O_WRONLY);
  tc_client_in[0]=_open_osfhandle(h_tc_client_in0, _O_RDONLY);
  tc_client_in[1]=_open_osfhandle(h_tc_client_in1, _O_WRONLY);
  STARTUPINFO startup;
  GetStartupInfo(&startup);
  startup.dwFlags|=STARTF_USESTDHANDLES;
  startup.hStdInput=h_tc_client_in0;
  startup.hStdOutput=h_tc_client1;
  PROCESS_INFORMATION pi;
  int len=strlen("./tc_client");
  for(i=1; i<argc; ++i){len+=strlen(argv[i])+1;}
  char cmd[len+1];
  strcpy(cmd, "./tc_client");
  for(i=1; i<argc; ++i){strcat(cmd, " "); strcat(cmd, argv[i]);}
  CreateProcess(0, cmd, 0, 0, 1, DETACHED_PROCESS, 0, 0, &startup, &pi);
#else
  pipe(tc_client);
  pipe(tc_client_in);
  if(!fork())
  {
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    close(tc_client[0]);
    close(tc_client_in[1]);
    dup2(tc_client[1], 1);
    dup2(tc_client_in[0], 0);
    argv[0]=(strncmp(argv[0], "./", 2)?"tc_client":"./tc_client");
    execvp(argv[0], argv);
  }
#endif
  close(tc_client_in[0]);
  GIOChannel* tcchannel=g_io_channel_unix_new(tc_client[0]);
  g_io_channel_set_encoding(tcchannel, 0, 0);
  unsigned int channel_id=g_io_add_watch(tcchannel, G_IO_IN, handledata, &data);

  gtk_main();
 
#ifdef _WIN32
  TerminateProcess(pi.hProcess, 0);
#endif
  g_source_remove(channel_id);
  g_io_channel_shutdown(tcchannel, 0, 0);
  for(i=0; i<data.camcount; ++i)
  {
    av_frame_free(&data.cams[i].frame);
    avcodec_free_context(&data.cams[i].vctx);
#ifdef HAVE_AVRESAMPLE
    avcodec_free_context(&data.cams[i].actx);
    avresample_free(&data.resamplectx);
#elif defined(HAVE_SWRESAMPLE)
    avcodec_free_context(&data.cams[i].actx);
    swr_free(&data.swrctx);
#endif
    free(data.cams[i].id);
    free(data.cams[i].nick);
  }
  free(data.cams);
  return 0;
}
