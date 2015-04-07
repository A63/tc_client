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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <ctype.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#if LIBAVUTIL_VERSION_MAJOR>50 || (LIBAVUTIL_VERSION_MAJOR==50 && LIBAVUTIL_VERSION_MINOR>37)
  #include <libavutil/imgutils.h>
#else
  #include <libavcore/imgutils.h>
#endif
#ifdef HAVE_SOUND
  // Use libavresample if available, otherwise fall back on libswresample
  #if HAVE_SOUND==avresample
    #include <libavutil/opt.h>
    #include <libavresample/avresample.h>
  #else
    #include <libswresample/swresample.h>
  #endif
  #include <ao/ao.h>
#endif
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#ifdef HAVE_V4L2
  #include <libv4l2.h>
  #include <linux/videodev2.h>
#endif
#include "userlist.h"
#include "media.h"
#include "compat.h"

struct viddata
{
  GtkWidget* box;
  AVCodec* vdecoder;
  AVCodec* vencoder;
  AVCodec* adecoder;
  int scalewidth;
  int scaleheight;
#ifdef HAVE_SOUND
  int audiopipe;
  #if HAVE_SOUND==avresample
    AVAudioResampleContext* resamplectx;
  #else
    SwrContext* swrctx;
  #endif
#endif
  GtkTextBuffer* buffer; // TODO: struct buffer array, for PMs
  GtkAdjustment* scroll;
};

int tc_client[2];
int tc_client_in[2];

char autoscroll_before(GtkAdjustment* scroll)
{
  // Figure out if we're at the bottom and should autoscroll with new content
  int upper=gtk_adjustment_get_upper(scroll);
  int size=gtk_adjustment_get_page_size(scroll);
  int value=gtk_adjustment_get_value(scroll);
  return (value+size==upper);
}

void autoscroll_after(GtkAdjustment* scroll)
{
  while(gtk_events_pending()){gtk_main_iteration();} // Make sure the textview's new size affects scroll's "upper" value first
  int upper=gtk_adjustment_get_upper(scroll);
  int size=gtk_adjustment_get_page_size(scroll);
  gtk_adjustment_set_value(scroll, upper-size);
}

void updatescaling(struct viddata* data, unsigned int width, unsigned int height)
{
// TODO: Move updatescaling into media.c?
  if(!camcount){return;}
  if(!width){width=gtk_widget_get_allocated_width(data->box);}
  if(!height){height=gtk_widget_get_allocated_height(data->box);}
  data->scalewidth=width/camcount;
  // 3/4 ratio
  data->scaleheight=data->scalewidth*3/4;
  unsigned int i;
  unsigned int labelsize=0;
  for(i=0; i<camcount; ++i)
  {
    if(gtk_widget_get_allocated_height(cams[i].label)>labelsize)
      labelsize=gtk_widget_get_allocated_height(cams[i].label);
  }
  // Fit by height
  if(height<data->scaleheight+labelsize)
  {
    data->scaleheight=height-labelsize;
    data->scalewidth=data->scaleheight*4/3;
  }
  if(data->scalewidth<8){data->scalewidth=8;}
  if(data->scaleheight<1){data->scaleheight=1;}
  // TODO: wrapping and stuff
  // Rescale current images to fit
  for(i=0; i<camcount; ++i)
  {
    GdkPixbuf* pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(cams[i].cam));
    if(!pixbuf){continue;}
    pixbuf=gdk_pixbuf_scale_simple(pixbuf, data->scalewidth, data->scaleheight, GDK_INTERP_BILINEAR);
    gtk_image_set_from_pixbuf(GTK_IMAGE(cams[i].cam), pixbuf);
// TODO: figure out/fix the "static" noise that seems to happen here
  }
}

void printchat(struct viddata* data, const char* text)
{
  char bottom=autoscroll_before(data->scroll);
  // Insert new content
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(data->buffer, &end);
  gtk_text_buffer_insert(data->buffer, &end, "\n", -1);
  gtk_text_buffer_insert(data->buffer, &end, text, -1);
  if(bottom){autoscroll_after(data->scroll);}
}

char buf[1024];
gboolean handledata(GIOChannel* channel, GIOCondition condition, gpointer datap)
{
  int fd=g_io_channel_unix_get_fd(channel);
  struct viddata* data=datap;
  unsigned int i;
  for(i=0; i<1023; ++i)
  {
    if(read(fd, &buf[i], 1)<1){printf("No more data\n"); gtk_main_quit(); return 0;}
    if(buf[i]=='\r'||buf[i]=='\n'){break;}
  }
  buf[i]=0;
  if(!strncmp(buf, "Currently online: ", 18))
  {
    printchat(data, buf);
    char* next=&buf[16];
    while(next)
    {
      char* nick=&next[2];
      next=strstr(nick, ", ");
      if(next){next[0]=0;}
      adduser(nick);
    }
    return 1;
  }
  // Start streams once we're properly connected
  if(!strncmp(buf, "Currently on cam: ", 18))
  {
    printchat(data, buf);
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
  // Timestamped events
  if(buf[0]=='['&&isdigit(buf[1])&&isdigit(buf[2])&&buf[3]==':'&&isdigit(buf[4])&&isdigit(buf[5])&&buf[6]==']'&&buf[7]==' ')
  {
    char* nick=&buf[8];
    space=strchr(nick, ' ');
    if(!space){return 1;}
//    if(space[-1]==':') // TODO: Handle messages specifically, e.g. handle /msg
    // Remove escape codes and pick up the text color while we're at it
    char* color=0;
    char* esc;
    char* escend;
    while((esc=strchr(buf, '\x1b'))&&(escend=strchr(esc, 'm')))
    {
      escend[0]=0;
      if(!color && strcmp(&esc[1], "[0")){color=strdup(&esc[1]);}
      memmove(esc, &escend[1], strlen(&escend[1])+1);
    }
    char bottom=autoscroll_before(data->scroll);
    // Insert new content
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(data->buffer, &end);
    gtk_text_buffer_insert(data->buffer, &end, "\n", -1);
    int startnum=gtk_text_iter_get_offset(&end);
    gtk_text_buffer_insert(data->buffer, &end, buf, -1);
    if(color) // Set color if there was one
    {
      GtkTextIter start;
      gtk_text_buffer_get_iter_at_offset(data->buffer, &start, startnum+8); // 8 to skip timestamps
      gtk_text_buffer_apply_tag_by_name(data->buffer, color, &start, &end);
    }
    if(bottom){autoscroll_after(data->scroll);}
    if(space[-1]!=':') // Not a message
    {
      if(!strcmp(space, " entered the channel"))
      {
        space[0]=0;
        adduser(nick);
      }
      else if(!strcmp(space, " left the channel"))
      {
        space[0]=0;
        removeuser(nick);
        camera_removebynick(nick);
      }
      else if(!strncmp(space, " changed nickname to ", 21))
      {
        space[0]=0;
        renameuser(nick, &space[21]);
        struct camera* cam=camera_findbynick(nick);
        if(cam)
        {
          free(cam->nick);
          cam->nick=strdup(&space[21]);
          gtk_label_set_text(GTK_LABEL(cam->label), cam->nick);
        }
      }
    }
    return 1;
  }
  if(space && !strcmp(space, " is a moderator."))
  {
    space[0]=0;
    struct user* user=finduser(buf);
    if(user)
    {
      user->ismod=1;
      renameuser(buf, buf); // Update the userlist label
    }
    return 1;
  }
  if(space && !strcmp(space, " is no longer a moderator."))
  {
    space[0]=0;
    struct user* user=finduser(buf);
    if(user){user->ismod=0;}
    return 1;
  }
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
    camera_removebynick(nick); // Remove any duplicates
    struct camera* cam=camera_new();
    cam->frame=av_frame_alloc();
    cam->dstframe=av_frame_alloc();
    cam->nick=strdup(nick);
    cam->id=strdup(id);
    cam->vctx=avcodec_alloc_context3(data->vdecoder);
    avcodec_open2(cam->vctx, data->vdecoder, 0);
#ifdef HAVE_SOUND
    cam->actx=avcodec_alloc_context3(data->adecoder);
    avcodec_open2(cam->actx, data->adecoder, 0);
    cam->samples=0;
#endif
    cam->cam=gtk_image_new();
    cam->box=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(cam->box), 0);
    gtk_box_pack_start(GTK_BOX(cam->box), cam->cam, 0, 0, 0);
    cam->label=gtk_label_new(cam->nick);
    gtk_box_pack_start(GTK_BOX(cam->box), cam->label, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(data->box), cam->box, 0, 0, 0);
    gtk_widget_show_all(cam->box);
    updatescaling(data, 0, 0);
    while(gtk_events_pending()){gtk_main_iteration();} // Make sure the label gets its size before we calculate scaling
    updatescaling(data, 0, 0);
    return 1;
  }
  if(!strcmp(buf, "Starting outgoing media stream"))
  {
    struct camera* cam=camera_new();
    cam->frame=av_frame_alloc();
    cam->dstframe=av_frame_alloc();
    cam->nick=strdup("You");
    cam->id=strdup("out");
    cam->vctx=avcodec_alloc_context3(data->vdecoder);
    avcodec_open2(cam->vctx, data->vdecoder, 0);
    cam->actx=0;
    cam->cam=gtk_image_new();
    cam->box=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(cam->box), cam->cam, 0, 0, 0);
    cam->label=gtk_label_new(cam->nick);
    gtk_box_pack_start(GTK_BOX(cam->box), cam->label, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(data->box), cam->box, 0, 0, 0);
    gtk_widget_show_all(cam->box);
    updatescaling(data, 0, 0);
    while(gtk_events_pending()){gtk_main_iteration();} // Make sure the label gets its size before we calculate scaling
    updatescaling(data, 0, 0);
    return 1;
  }
  if(!strncmp(buf, "VideoEnd: ", 10))
  {
    camera_remove(&buf[10]);
    updatescaling(data, 0, 0);
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
    read(fd, &frameinfo, 1);
    --size; // For the byte we read above
    AVPacket pkt;
    av_init_packet(&pkt);
    unsigned char databuf[size];
    pkt.data=databuf;
    pkt.size=size;
    unsigned int pos=0;
    while(pos<size)
    {
      pos+=read(fd, pkt.data+pos, size-pos);
    }
#ifdef HAVE_SOUND
    // Find the camera representation for the given ID (for decoder context)
    struct camera* cam=camera_find(&buf[7]);
    if(!cam){printf("No cam found with ID '%s'\n", &buf[7]); return 1;}
    int gotframe;
    avcodec_decode_audio4(cam->actx, cam->frame, &gotframe, &pkt);
    if(!gotframe){return 1;}
  #if HAVE_SOUND==avresample
    int outlen=avresample_convert(data->resamplectx, cam->frame->data, cam->frame->linesize[0], cam->frame->nb_samples, cam->frame->data, cam->frame->linesize[0], cam->frame->nb_samples);
  #else
    int outlen=swr_convert(data->resamplectx, cam->frame->data, cam->frame->nb_samples, (const uint8_t**)cam->frame->data, cam->frame->nb_samples);
  #endif
    camera_playsnd(data->audiopipe, cam, (short*)cam->frame->data[0], outlen);
#endif
    return 1;
  }
  if(strncmp(buf, "Video: ", 7)){printf("Got '%s'\n", buf); fflush(stdout); return 1;} // Ignore anything else that isn't video
  char* sizestr=strchr(&buf[7], ' ');
  if(!sizestr){return 1;}
  sizestr[0]=0;
  // Find the camera representation for the given ID
  struct camera* cam=camera_find(&buf[7]);
  unsigned int size=strtoul(&sizestr[1], 0, 0);
  if(!size){return 1;}
  // Mostly ignore the first byte (contains frame type (e.g. keyframe etc.) in 4 bits and codec in the other 4)
  --size;
  AVPacket pkt;
  av_init_packet(&pkt);
  unsigned char databuf[size+4];
  pkt.data=databuf;
  unsigned char frameinfo;
  read(fd, &frameinfo, 1);
// printf("Frametype-frame: %x\n", ((unsigned int)frameinfo&0xf0)/16);
// printf("Frametype-codec: %x\n", (unsigned int)frameinfo&0xf);
  unsigned int pos=0;
  while(pos<size)
  {
    pos+=read(fd, pkt.data+pos, size-pos);
  }
  if((frameinfo&0xf)!=2){return 1;} // Not FLV1, get data but discard it
  if(!cam){printf("No cam found with ID '%s'\n", &buf[7]); return 1;}
  pkt.size=size;
  int gotframe;
  avcodec_decode_video2(cam->vctx, cam->frame, &gotframe, &pkt);
  if(!gotframe){return 1;}

  // Scale and convert to RGB24 format
  unsigned int bufsize=avpicture_get_size(PIX_FMT_RGB24, data->scalewidth, data->scaleheight);
  unsigned char buf[bufsize];
  cam->dstframe->data[0]=buf;
  cam->dstframe->linesize[0]=data->scalewidth*3;
  struct SwsContext* swsctx=sws_getContext(cam->frame->width, cam->frame->height, cam->frame->format, data->scalewidth, data->scaleheight, AV_PIX_FMT_RGB24, 0, 0, 0, 0);
  sws_scale(swsctx, (const uint8_t*const*)cam->frame->data, cam->frame->linesize, 0, cam->frame->height, cam->dstframe->data, cam->dstframe->linesize);
  sws_freeContext(swsctx);

  GdkPixbuf* gdkframe=gdk_pixbuf_new_from_data(cam->dstframe->data[0], GDK_COLORSPACE_RGB, 0, 8, data->scalewidth, data->scaleheight, cam->dstframe->linesize[0], 0, 0);
  gtk_image_set_from_pixbuf(GTK_IMAGE(cam->cam), gdkframe);
  // Make sure it gets redrawn in time
  gdk_window_process_updates(gtk_widget_get_window(cam->cam), 1);

  return 1;
}

#ifdef HAVE_SOUND
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

#ifdef HAVE_V4L2
pid_t camproc=0;
void togglecam(GtkCheckMenuItem* item, struct viddata* data)
{
  if(!gtk_check_menu_item_get_active(item))
  {
    kill(camproc, SIGINT);
    camproc=0;
    dprintf(tc_client_in[1], "/camdown\n");
    dprintf(tc_client[1], "VideoEnd: out\n"); // Close our local display
    return;
  }
  // Set up a second pipe to be handled by handledata() to avoid overlap with tc_client's output
  int campipe[2];
  pipe(campipe);
  dprintf(tc_client_in[1], "/camup\n");
// printf("Camming up!\n");
  camproc=fork();
  if(!camproc)
  {
    close(campipe[0]);
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    unsigned int delay=500000;
    // Set up camera
    int fd=v4l2_open("/dev/video0", O_RDWR);
    struct v4l2_format fmt;
    fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width=320;
    fmt.fmt.pix.height=240;
    fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.field=V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline=fmt.fmt.pix.width*3;
    fmt.fmt.pix.sizeimage=fmt.fmt.pix.bytesperline*fmt.fmt.pix.height;
    v4l2_ioctl(fd, VIDIOC_S_FMT, &fmt);
    AVCodecContext* ctx=avcodec_alloc_context3(data->vencoder);
    ctx->width=fmt.fmt.pix.width;
    ctx->height=fmt.fmt.pix.height;
    ctx->pix_fmt=PIX_FMT_YUV420P;
    ctx->time_base.num=1;
    ctx->time_base.den=10;
    avcodec_open2(ctx, data->vencoder, 0);
    AVFrame* frame=av_frame_alloc();
    frame->format=PIX_FMT_RGB24;
    frame->width=fmt.fmt.pix.width;
    frame->height=fmt.fmt.pix.height;
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
      v4l2_read(fd, frame->data[0], fmt.fmt.pix.sizeimage);
      int gotpacket;
      sws_scale(swsctx, (const uint8_t*const*)frame->data, frame->linesize, 0, frame->height, dstframe->data, dstframe->linesize);
      av_init_packet(&packet);
      packet.data=0;
packet.size=0;
      avcodec_encode_video2(ctx, &packet, dstframe, &gotpacket);
      unsigned char frameinfo=0x22; // Note: differentiating between keyframes and non-keyframes seems to break stuff, so let's just go with all being interframes (1=keyframe, 2=interframe, 3=disposable interframe)
      dprintf(tc_client_in[1], "/video %i\n", packet.size+1);
      write(tc_client_in[1], &frameinfo, 1);
      write(tc_client_in[1], packet.data, packet.size);
      // Also send the packet to our main thread so we can see ourselves
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
  g_io_add_watch(channel, G_IO_IN, handledata, data);
}
#endif

gboolean handleresize(GtkWidget* widget, GdkEventConfigure* event, struct viddata* data)
{
  char bottom=autoscroll_before(data->scroll);
  if(event->width!=gtk_widget_get_allocated_width(data->box))
  {
    updatescaling(data, event->width, 0);
  }
  if(bottom){autoscroll_after(data->scroll);}
  return 0;
}

void handleresizepane(GObject* obj, GParamSpec* spec, struct viddata* data)
{
  char bottom=autoscroll_before(data->scroll);
  updatescaling(data, 0, gtk_paned_get_position(GTK_PANED(obj)));
  if(bottom){autoscroll_after(data->scroll);}
}

gboolean inputkeys(GtkWidget* widget, GdkEventKey* event, void* data)
{
  if(event->keyval==GDK_KEY_Up || event->keyval==GDK_KEY_Down){return 1;}
  if(event->keyval==GDK_KEY_Tab)
  {
    // Tab completion
    int cursor=gtk_editable_get_position(GTK_EDITABLE(widget));;
    GtkEntryBuffer* buf=gtk_entry_get_buffer(GTK_ENTRY(widget));
    const char* text=gtk_entry_buffer_get_text(buf);
    unsigned int namestart=0;
    unsigned int i;
    for(i=0; i<cursor; ++i)
    {
      if(text[i]==' '){namestart=i+1;}
    }
    const char* matches[usercount];
    unsigned int matchcount=0;
    unsigned int commonlen=128;
    for(i=0; i<usercount; ++i)
    {
      if(!strncmp(&text[namestart], userlist[i].nick, cursor-namestart))
      {
        unsigned int j;
        for(j=0; j<matchcount; ++j)
        {
          if(strncmp(matches[j], userlist[i].nick, commonlen))
          {
            for(commonlen=0; userlist[i].nick[commonlen] && matches[j][commonlen] && userlist[i].nick[commonlen]==matches[j][commonlen]; ++commonlen);
          }
        }
        matches[matchcount]=userlist[i].nick;
        ++matchcount;
      }
    }
    if(matchcount==1)
    {
      gtk_entry_buffer_insert_text(buf, cursor, &matches[0][cursor-namestart], -1);
      cursor+=strlen(&matches[0][cursor-namestart]);
      if(!namestart){gtk_entry_buffer_insert_text(buf, cursor, ": ", -1); cursor+=2;}
      gtk_editable_set_position(GTK_EDITABLE(widget), cursor);
    }
    else if(matchcount>1)
    {
      gtk_entry_buffer_insert_text(buf, cursor, &matches[0][cursor-namestart], commonlen+namestart-cursor);
      cursor=namestart+commonlen;
      gtk_editable_set_position(GTK_EDITABLE(widget), cursor);
    }
    return 1;
  }
  return 0;
}

void sendmessage(GtkEntry* entry, struct viddata* data)
{
  const char* msg=gtk_entry_get_text(entry);
  char text[strlen("[00:00] ")+strlen("You: ")+strlen(msg)+1];
  time_t timestamp=time(0);
  struct tm* t=localtime(&timestamp);
  sprintf(text, "[%02i:%02i] ", t->tm_hour, t->tm_min);
  sprintf(&text[8], "You: %s", msg);
  printchat(data, text);
  dprintf(tc_client_in[1], "%s\n", msg);
  gtk_entry_set_text(entry, "");
}

int main(int argc, char** argv)
{
  struct viddata data={0,0,0,0,0};
  avcodec_register_all();
  data.vdecoder=avcodec_find_decoder(AV_CODEC_ID_FLV1);
  data.adecoder=avcodec_find_decoder(AV_CODEC_ID_NELLYMOSER);

#ifdef HAVE_SOUND
  #if HAVE_SOUND==avresample
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
  data.resamplectx=swr_alloc_set_opts(0, AV_CH_FRONT_CENTER, AV_SAMPLE_FMT_S16, 22050, AV_CH_FRONT_CENTER, AV_SAMPLE_FMT_FLT, 11025, 0, 0);
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
  GtkBuilder* gui=gtk_builder_new_from_file("gtkgui.glade");
  gtk_builder_connect_signals(gui, 0);

#ifdef HAVE_V4L2
  GtkWidget* item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_broadcast_camera"));
  g_signal_connect(item, "toggled", G_CALLBACK(togglecam), &data);
  data.vencoder=avcodec_find_encoder(AV_CODEC_ID_FLV1);
#else
  GtkWidget* item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_broadcast"));
  gtk_widget_destroy(item);
#endif
  
  data.box=GTK_WIDGET(gtk_builder_get_object(gui, "cambox"));
  userlistwidget=GTK_WIDGET(gtk_builder_get_object(gui, "userlistbox"));
  GtkWidget* chatview=GTK_WIDGET(gtk_builder_get_object(gui, "chatview"));
  data.scroll=gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(gtk_builder_get_object(gui, "chatscroll")));

  data.buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(chatview));
  #define colormap(code, color) gtk_text_buffer_create_tag(data.buffer, code, "foreground", color, (char*)0)
  colormap("[31", "#821615");
  colormap("[31;1", "#c53332");
  colormap("[33", "#a08f23");
  //colormap("[33", "#a78901");
  colormap("[33;1", "#919104");
  colormap("[32;1", "#7bb224");
  //colormap("[32;1", "#7db257");
  colormap("[32", "#487d21");
  colormap("[36", "#00a990");
  colormap("[34;1", "#32a5d9");
  //colormap("[34;1", "#1d82eb");
  colormap("[34", "#1965b6");
  colormap("[35", "#5c1a7a");
  colormap("[35;1", "#9d5bb5");
  //colormap("[35;1", "#c356a3");
  //colormap("[35;1", "#b9807f");

  GtkWidget* panes=GTK_WIDGET(gtk_builder_get_object(gui, "vpaned"));
  g_signal_connect(panes, "notify::position", (GCallback)handleresizepane, &data);

  GtkWidget* inputfield=GTK_WIDGET(gtk_builder_get_object(gui, "inputfield"));
  g_signal_connect(inputfield, "activate", (GCallback)sendmessage, &data);
  g_signal_connect(inputfield, "key-press-event", (GCallback)inputkeys, &data);

  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "main"));
  g_signal_connect(window, "configure-event", (GCallback)handleresize, &data);
  gtk_widget_show_all(window);

  pipe(tc_client);
  pipe(tc_client_in);
  if(!fork())
  {
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    close(tc_client[0]);
    close(tc_client_in[1]);
    dup2(tc_client[1], 1);
    dup2(tc_client_in[0], 0);
    argv[0]="./tc_client";
    execv("./tc_client", argv);
  }
  close(tc_client_in[0]);
  GIOChannel* tcchannel=g_io_channel_unix_new(tc_client[0]);
  g_io_channel_set_encoding(tcchannel, 0, 0);
  unsigned int channel_id=g_io_add_watch(tcchannel, G_IO_IN, handledata, &data);

  gtk_main();
 
  g_source_remove(channel_id);
  g_io_channel_shutdown(tcchannel, 0, 0);
  camera_cleanup();
#ifdef HAVE_SOUND
  #if HAVE_SOUND==avresample
  avresample_free(&data.resamplectx);
  #else
  swr_free(&data.swrctx);
  #endif
#endif
  return 0;
}
