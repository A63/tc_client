/*
    tc_client-gtk, a graphical user interface for tc_client
    Copyright (C) 2015-2016  alicia@ion.nu
    Copyright (C) 2015  Pamela Hiatt

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
  #include <sys/wait.h>
#endif
#include <ctype.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#if LIBAVUTIL_VERSION_MAJOR>50 || (LIBAVUTIL_VERSION_MAJOR==50 && LIBAVUTIL_VERSION_MINOR>37)
  #include <libavutil/imgutils.h>
#else
  #include <libavcore/imgutils.h>
#endif
// Use libavresample if available, otherwise fall back on libswresample
#ifdef HAVE_AVRESAMPLE
  #include <libavutil/opt.h>
  #include <libavresample/avresample.h>
  #include <ao/ao.h>
#elif defined(HAVE_SWRESAMPLE)
  #include <libswresample/swresample.h>
  #include <ao/ao.h>
#endif
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "../libcamera/camera.h"
#include "userlist.h"
#include "media.h"
#include "compat.h"
#include "config.h"
#include "gui.h"
#include "logging.h"
#include "../stringutils.h"
#include "../compat.h"
#include "../compat_av.h"
#include "inputhistory.h"

struct viddata
{
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
  GtkTextBuffer* buffer;
  GtkAdjustment* scroll;
};
struct viddata* data;

int tc_client[2];
int tc_client_in[2];
const char* channel=0;
const char* mycolor=0;
char* nickname=0;
char frombuild=0; // Running from the build directory
#define TC_CLIENT (frombuild?"./tc_client":"tc_client")
#ifdef _WIN32
  PROCESS_INFORMATION coreprocess={.hProcess=0};
#endif

void printchat(const char* text, const char* pm)
{
  GtkAdjustment* scroll;
  GtkTextBuffer* buffer;
  struct user* user;
  if(pm && (user=finduser(pm)))
  {
    pm_open(pm, 0, data->scroll);
    scroll=user->pm_scroll;
    buffer=user->pm_buffer;
  }else{
    scroll=data->scroll;
    buffer=data->buffer;
  }
  char bottom=autoscroll_before(scroll);
  // Insert new content
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buffer, &end);
  gtk_text_buffer_insert(buffer, &end, "\n", -1);
  gtk_text_buffer_insert(buffer, &end, text, -1);
  buffer_updatesize(buffer);
  if(bottom){autoscroll_after(scroll);}
}

void printchat_color(const char* text, const char* color, unsigned int offset, const char* pm)
{
  GtkAdjustment* scroll;
  GtkTextBuffer* buffer;
  struct user* user;
  if(pm && (user=finduser(pm)))
  {
    pm_open(pm, 0, data->scroll);
    scroll=user->pm_scroll;
    buffer=user->pm_buffer;
  }else{
    scroll=data->scroll;
    buffer=data->buffer;
  }
  char bottom=autoscroll_before(scroll);
  // Insert new content
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buffer, &end);
  gtk_text_buffer_insert(buffer, &end, "\n", -1);
  int startnum=gtk_text_iter_get_offset(&end);
  gtk_text_buffer_insert(buffer, &end, text, -1);
  GtkTextIter start;
  // Set color if there was one
  if(color)
  {
    GtkTextTagTable* table=gtk_text_buffer_get_tag_table(buffer);
    if(!gtk_text_tag_table_lookup(table, color)) // Color isn't defined, define it
    {
      char colorbuf[8];
      memcpy(colorbuf, color, 7);
      colorbuf[7]=0;
      while(strchr(colorbuf,',')){memmove(&colorbuf[2], &colorbuf[1], 5); colorbuf[1]='0';}
      gtk_text_buffer_create_tag(buffer, color, "foreground", colorbuf, (char*)0);
    }
    gtk_text_buffer_get_iter_at_offset(buffer, &start, startnum+offset);
    gtk_text_buffer_apply_tag_by_name(buffer, color, &start, &end);
  }
  if(offset==8) // Chat message, has timestamp and nickname, turn them gray and bold
  {
    gtk_text_buffer_get_iter_at_offset(buffer, &start, startnum);
    gtk_text_buffer_get_iter_at_offset(buffer, &end, startnum+offset);
    gtk_text_buffer_apply_tag_by_name(buffer, "timestamp", &start, &end);
    unsigned int nicklen=strchr(&text[offset], ' ')-text;
    gtk_text_buffer_get_iter_at_offset(buffer, &start, startnum+offset);
    gtk_text_buffer_get_iter_at_offset(buffer, &end, startnum+nicklen);
    gtk_text_buffer_apply_tag_by_name(buffer, "nickname", &start, &end);
  }
  buffer_updatesize(buffer);
  if(bottom){autoscroll_after(scroll);}
}

unsigned int cameventsource=0;
char buf[1024];
gboolean handledata(GIOChannel* iochannel, GIOCondition condition, gpointer datap)
{
  gsize r;
  unsigned int i;
  for(i=0; i<1023; ++i)
  {
    g_io_channel_read_chars(iochannel, &buf[i], 1, &r, 0);
    if(r<1){printf("No more data\n"); gtk_main_quit(); return 0;}
    if(buf[i]=='\r'||buf[i]=='\n'){break;}
  }
  buf[i]=0;
  if(!strncmp(buf, "Video: ", 7))
  {
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
    g_io_channel_read_chars(iochannel, (gchar*)&frameinfo, 1, 0, 0);
//   printf("Frametype-frame: %x\n", ((unsigned int)frameinfo&0xf0)/16);
//   printf("Frametype-codec: %x\n", (unsigned int)frameinfo&0xf);
    unsigned int pos=0;
    while(pos<size)
    {
      g_io_channel_read_chars(iochannel, (gchar*)pkt.data+pos, size-pos, &r, 0);
      pos+=r;
    }
    if((frameinfo&0xf)!=2){return 1;} // Not FLV1, get data but discard it
    if(!cam){printf("No cam found with ID '%s'\n", &buf[7]); return 1;}
    pkt.size=size;
    int gotframe;
    avcodec_send_packet(cam->vctx, &pkt);
    gotframe=avcodec_receive_frame(cam->vctx, cam->frame);
    if(gotframe){return 1;}

    if(cam->placeholder) // Remove the placeholder animation if it has it
    {
      g_source_remove(cam->placeholder);
      cam->placeholder=0;
    }
    // Scale and convert to RGB24 format
    unsigned int bufsize=av_image_get_buffer_size(AV_PIX_FMT_RGB24, camsize_scale.width, camsize_scale.height, 1);
    unsigned char* buf=malloc(bufsize);
    cam->dstframe->data[0]=buf;
    cam->dstframe->linesize[0]=camsize_scale.width*3;
    struct SwsContext* swsctx=sws_getContext(cam->frame->width, cam->frame->height, cam->frame->format, camsize_scale.width, camsize_scale.height, AV_PIX_FMT_RGB24, SWS_BICUBIC, 0, 0, 0);
    sws_scale(swsctx, (const uint8_t*const*)cam->frame->data, cam->frame->linesize, 0, cam->frame->height, cam->dstframe->data, cam->dstframe->linesize);
    sws_freeContext(swsctx);
    postprocess(&cam->postproc, cam->dstframe->data[0], camsize_scale.width, camsize_scale.height);

    GdkPixbuf* oldpixbuf=gtk_image_get_pixbuf(GTK_IMAGE(cam->cam));
    GdkPixbuf* gdkframe=gdk_pixbuf_new_from_data(cam->dstframe->data[0], GDK_COLORSPACE_RGB, 0, 8, camsize_scale.width, camsize_scale.height, cam->dstframe->linesize[0], freebuffer, 0);
    gtk_image_set_from_pixbuf(GTK_IMAGE(cam->cam), gdkframe);
    g_object_unref(oldpixbuf);
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
    g_io_channel_read_chars(iochannel, (gchar*)&frameinfo, 1, 0, 0);
    --size; // For the byte we read above
    AVPacket pkt;
    av_init_packet(&pkt);
    unsigned char databuf[size];
    pkt.data=databuf;
    pkt.size=size;
    unsigned int pos=0;
    while(pos<size)
    {
      g_io_channel_read_chars(iochannel, (gchar*)pkt.data+pos, size-pos, &r, 0);
      pos+=r;
    }
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
    // Find the camera representation for the given ID (for decoder context)
    struct camera* cam=camera_find(&buf[7]);
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
    if(outlen>0){camera_playsnd(data->audiopipe, cam, (short*)cam->frame->data[0], outlen);}
#endif
    return 1;
  }
  if(!strncmp(buf, "Currently online: ", 18))
  {
    printchat(buf, 0);
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
  if(!strncmp(buf, "Connection ID: ", 15)) // Our initial nickname is "guest-" plus our connection ID
  {
    write(tc_client_in[1], "/color\n", 7); // Check which random color tc_client picked
    unsigned int length=strlen(&buf[15]);
    nickname=malloc(length+strlen("guest-")+1);
    sprintf(nickname, "guest-%s", &(buf[15]));
    return 1;
  }
  if(!strncmp(buf, "Captcha: ", 9))
  {
    gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "main")));
    gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(gui, "captcha")));
    char link[snprintf(0,0,"Captcha: <a href=\"%s\">%s</a>", &buf[9], &buf[9])+1];
    sprintf(link, "Captcha: <a href=\"%s\">%s</a>", &buf[9], &buf[9]);
    gtk_label_set_markup(GTK_LABEL(gtk_builder_get_object(gui, "captcha_link")), link);
    return 1;
  }
  // Start streams once we're properly connected
  if(!strncmp(buf, "Currently on cam: ", 18))
  {
    printchat(buf, 0);
    if(!config_get_bool("autoopencams") && config_get_set("autoopencams")){return 1;}
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
  if(!strcmp(buf, "Password required"))
  {
    gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "main")));
    gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(gui, "channelpasswordwindow")));
    return 1;
  }
  if(buf[0]=='/') // For the /help text
  {
    printchat(buf, 0);
    return 1;
  }
  char* color=0;
  // Handle color at start of line (/color and /colors)
  if(buf[0]=='(' && buf[1]=='#')
  {
    char* end=strchr(buf, ')');
    if(end)
    {
      end[0]=0;
      color=strdup(&buf[1]);
      memmove(buf, &end[1], strlen(&end[1])+1);
    }
  }
  char* space=strchr(buf, ' ');
  // Timestamped events
  if(buf[0]=='['&&isdigit(buf[1])&&isdigit(buf[2])&&buf[3]==':'&&isdigit(buf[4])&&isdigit(buf[5])&&buf[6]==']'&&buf[7]==' ')
  {
    char* pm=0;
    char* nick=&buf[8];
    if(nick[0]=='(' && nick[1]=='#') // Message color
    {
      char* end=strchr(nick, ')');
      if(end)
      {
        end[0]=0;
        color=strdup(&nick[1]);
        memmove(nick, &end[1], strlen(&end[1])+1);
      }
    }
    space=strchr(nick, ' ');
    if(!space){return 1;}
    if(space[-1]==':')
    {
      if(config_get_bool("soundradio_cmd"))
      {
#ifdef _WIN32
        char* cmd=strdup(config_get_str("soundcmd"));
        w32_runcmd(cmd);
        free(cmd);
#else
        if(!fork())
        {
          execlp("sh", "sh", "-c", config_get_str("soundcmd"), (char*)0);
          _exit(0);
        }
#endif
      }
      if(!strncmp(space, " /mbs youTube ", 14) && config_get_bool("youtuberadio_cmd"))
      {
#ifndef _WIN32
        if(!fork())
#else
        char* spacetmp=space;
        space=strdup(space);
#endif
        {
// TODO: store the PID and make sure it's dead before starting a new video? and upon /mbc?
// TODO: only play videos from mods?
          char* id=&space[14];
          char* offset=strchr(id, ' ');
          if(!offset){_exit(1);}
          offset[0]=0;
          offset=&offset[1];
          char* end=strchr(offset, ' '); // Ignore any additional arguments after the offset (the modbot utility includes the video title here)
          if(end){end[0]=0;}
          // Handle format string
          const char* fmt=config_get_str("youtubecmd");
          int len=strlen(fmt)+1;
          len+=strcount(fmt, "%i")*(strlen(id)-2);
          len+=strcount(fmt, "%t")*(strlen(id)-2);
          char cmd[len];
          cmd[0]=0;
          while(fmt[0])
          {
            if(!strncmp(fmt, "%i", 2)){strcat(cmd, id); fmt=&fmt[2]; continue;}
            if(!strncmp(fmt, "%t", 2)){strcat(cmd, offset); fmt=&fmt[2]; continue;}
            for(len=0; fmt[len] && strncmp(&fmt[len], "%i", 2) && strncmp(&fmt[len], "%t", 2); ++len);
            strncat(cmd, fmt, len);
            fmt=&fmt[len];
          }
#ifdef _WIN32
          w32_runcmd(cmd);
          free(space);
          space=spacetmp;
#else
          execlp("sh", "sh", "-c", cmd, (char*)0);
          _exit(0);
#endif
        }
      }
      // Handle incoming PMs
      else if(!strncmp(space, " /msg ", 6))
      {
        char* msg=strchr(&space[6], ' ');
        if(msg)
        {
          memmove(space, msg, strlen(msg)+1);
          char* end=strchr(nick, ':');
          pm=malloc(end-nick+1);
          memcpy(pm, nick, end-nick);
          pm[end-nick]=0;
        }
      }
    }
    if(config_get_bool("enable_logging")){logger_write(buf, channel, pm);}
    // Insert new content
    printchat_color(buf, color, 8, pm);
    pm_highlight(pm);
    free(pm);
    if(space[-1]!=':') // Not a message
    {
      if(!strcmp(space, " entered the channel"))
      {
        if(config_get_bool("camdownonjoin"))
        {
          gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_camera")), 0);
        }
        space[0]=0;
        adduser(nick);
      }
      else if(!strcmp(space, " left the channel"))
      {
        space[0]=0;
        removeuser(nick);
        camera_remove(nick, 1);
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
        // If it was us, keep track of the new nickname
        if(!strcmp(nickname, nick))
        {
          free(nickname);
          nickname=strdup(&space[21]);
        }
      }
    }
    free(color);
    return 1;
  }
  if(!strcmp(buf, "Changed color") || !strncmp(buf, "Current color: ", 15))
  {
    printchat_color(buf, color, 0, 0);
    free((void*)mycolor);
    mycolor=color;
    return 1;
  }
  if(!strncmp(buf, "Color ", 6))
  {
    printchat_color(buf, color, 0, 0);
  }
  free(color);
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
    printchat(buf, 0);
    if(config_get_bool("autoopencams") || !config_get_set("autoopencams"))
    {
      space[0]=0;
      dprintf(tc_client_in[1], "/opencam %s\n", buf);
    }
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
    camera_remove(nick, 1); // Remove any duplicates
    struct camera* cam=camera_new(nick, id);
    cam->placeholder=g_timeout_add(100, camplaceholder_update, cam->id);
    cam->vctx=avcodec_alloc_context3(data->vdecoder);
    avcodec_open2(cam->vctx, data->vdecoder, 0);
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
    cam->actx=avcodec_alloc_context3(data->adecoder);
    avcodec_open2(cam->actx, data->adecoder, 0);
    cam->samples=0;
#endif
    updatescaling(0, 0, 1);
    gtk_widget_show_all(cam->box);
    return 1;
  }
  if(!strcmp(buf, "Starting outgoing media stream"))
  {
    struct camera* cam=camera_new(nickname, "out");
    cam->vctx=avcodec_alloc_context3(data->vencoder);
    cam->vctx->pix_fmt=AV_PIX_FMT_YUV420P;
    cam->vctx->time_base.num=1;
    cam->vctx->time_base.den=10;
    cam->vctx->width=camsize_out.width;
    cam->vctx->height=camsize_out.height;
    avcodec_open2(cam->vctx, data->vencoder, 0);
    cam->frame->data[0]=0;
    cam->frame->width=0;
    cam->frame->height=0;
    cam->dstframe->data[0]=0;

    cam->actx=0;
    updatescaling(0, 0, 1);
    gtk_widget_show_all(cam->box);
    return 1;
  }
  if(!strncmp(buf, "VideoEnd: ", 10))
  {
    camera_remove(&buf[10], 0);
    return 1;
  }
  if(!strncmp(buf, "Room topic: ", 12) ||
     (space && (!strcmp(space, " is not logged in") || !strncmp(space, " is logged in as ", 17))))
  {
    printchat(buf, 0);
    return 1;
  }
  if(!strcmp(buf, "Server disconnected"))
  {
    printchat(buf, 0);
    if(camproc)
    {
      g_source_remove(cameventsource);
      kill(camproc, SIGINT);
      camproc=0;
    }
    return 1;
  }
  printf("Got '%s'\n", buf);
  fflush(stdout);
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

void togglecam(GtkCheckMenuItem* item, struct viddata* data)
{
  if(!gtk_check_menu_item_get_active(item))
  {
    g_source_remove(cameventsource);
    kill(camproc, SIGINT);
    camproc=0;
    if(camera_find("out"))
    {
      dprintf(tc_client_in[1], "/camdown\n");
      dprintf(tc_client[1], "VideoEnd: out\n"); // Close our local display
    }
    return;
  }
  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "camselection"));
  gtk_widget_show_all(window);

  // Start a cam thread for the selected cam
  GtkComboBox* combo=GTK_COMBO_BOX(gtk_builder_get_object(gui, "camselect_combo"));
  GIOChannel* channel=camthread(gtk_combo_box_get_active_id(combo), data->vencoder, 100000);
  cameventsource=g_io_add_watch(channel, G_IO_IN, cam_encode, 0);
}

gboolean handleresize(GtkWidget* widget, GdkEventConfigure* event, struct viddata* data)
{
  char bottom=autoscroll_before(data->scroll);
  if(event->width!=gtk_widget_get_allocated_width(cambox))
  {
    updatescaling(event->width, 0, 0);
  }
#ifndef _WIN32 // For some reason scrolling as a response to resizing freezes windows
  if(bottom){autoscroll_after(data->scroll);}
#endif
  return 0;
}

void handleresizepane(GObject* obj, GParamSpec* spec, struct viddata* data)
{
  char bottom=autoscroll_before(data->scroll);
  updatescaling(0, gtk_paned_get_position(GTK_PANED(obj)), 0);
#ifndef _WIN32
  if(bottom){autoscroll_after(data->scroll);}
#endif
}

gboolean inputkeys(GtkWidget* widget, GdkEventKey* event, void* data)
{
  if(event->keyval==GDK_KEY_Up || event->keyval==GDK_KEY_Down)
  {
    inputhistory(GTK_ENTRY(widget), event);
    return 1;
  }
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

char sendingmsg=0;
void sendmessage(GtkEntry* entry, void* x)
{
  const char* msg=gtk_entry_get_text(entry);
  if(!msg[0]){return;} // Don't send empty lines
  if(sendingmsg){return;}
  sendingmsg=1;
  inputhistory_add(msg);
  char* pm=0;
  if(!strncmp(msg, "/pm ", 4))
  {
    pm_open(&msg[4], 1, data->scroll);
    gtk_entry_set_text(entry, "");
    sendingmsg=0;
    return;
  }
  else if(msg[0]!='/') // If we're in a PM tab, send messages as PMs
  {
    GtkNotebook* tabs=GTK_NOTEBOOK(gtk_builder_get_object(gui, "tabs"));
    int num=gtk_notebook_get_current_page(tabs);
    if(num>0)
    {
      GtkWidget* page=gtk_notebook_get_nth_page(tabs, num);
      struct user* user=user_find_by_tab(page);
      if(!user) // Person we were PMing with left
      {
        gtk_entry_set_text(entry, "");
        sendingmsg=0;
        return;
      }
      pm=strdup(user->nick);
      dprintf(tc_client_in[1], "/msg %s ", pm);
    }
  }
  dprintf(tc_client_in[1], "%s\n", msg);
  // Don't print commands
  if(!strcmp(msg, "/help") ||
     !strncmp(msg, "/color ", 7) ||
     !strcmp(msg, "/color") ||
     !strcmp(msg, "/colors") ||
     !strncmp(msg, "/nick ", 6) ||
//     !strncmp(msg, "/msg ", 5) || // except PM commands
     !strncmp(msg, "/opencam ", 9) ||
     !strncmp(msg, "/close ", 7) ||
     !strncmp(msg, "/ban ", 5) ||
     !strcmp(msg, "/banlist") ||
     !strncmp(msg, "/forgive ", 9) ||
     !strcmp(msg, "/names") ||
     !strcmp(msg, "/mute") ||
     !strcmp(msg, "/push2talk") ||
     !strcmp(msg, "/camup") ||
     !strcmp(msg, "/camdown") ||
     !strncmp(msg, "/video ", 7) ||
     !strncmp(msg, "/topic ", 7))
  {
    gtk_entry_set_text(entry, "");
    sendingmsg=0;
    return;
  }
  if(!strncmp(msg, "/msg ", 5))
  {
    const char* end=strchr(&msg[5], ' ');
    if(end)
    {
      pm=malloc(end-&msg[5]+1);
      memcpy(pm, &msg[5], end-&msg[5]);
      pm[end-&msg[5]]=0;
      struct user* user=finduser(pm);
      if(!user)
      {
        gtk_entry_set_text(entry, "");
        printchat("No such user", 0);
        free(pm);
        sendingmsg=0;
        return;
      }
    }
  }
  char text[strlen("[00:00] ")+strlen(nickname)+strlen(": ")+strlen(msg)+1];
  time_t timestamp=time(0);
  struct tm* t=localtime(&timestamp);
  sprintf(text, "[%02i:%02i] ", t->tm_hour, t->tm_min);
  if(pm && msg[0]=='/')
  {
    sprintf(&text[8], "%s: %s", nickname, &msg[6+strlen(pm)]);
  }else{
    sprintf(&text[8], "%s: %s", nickname, msg);
  }
  if(config_get_bool("enable_logging")){logger_write(text, channel, pm);}
  printchat_color(text, mycolor, 8, pm);
  gtk_entry_set_text(entry, "");
  free(pm);
  sendingmsg=0;
}

void startsession(GtkButton* button, void* x)
{
  if(x!=(void*)-1)
  {
    // Populate the quick-connect fields with saved data
    int channel_id=(int)(intptr_t)x;
    char buf[256];
    sprintf(buf, "channel%i_nick", channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_nick")), config_get_str(buf));

    sprintf(buf, "channel%i_name", channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_channel")), config_get_str(buf));

    sprintf(buf, "channel%i_password", channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_password")), config_get_str(buf));

    sprintf(buf, "channel%i_accuser", channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_username")), config_get_str(buf));

    sprintf(buf, "channel%i_accpass", channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_password")), config_get_str(buf));
    // TODO: if account password is empty, open a password entry window similar to the channel password one
  }
  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "startwindow")));
  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "channelconfig")));
  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "channelpasswordwindow")));
  gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(gui, "main")));
  const char* nick=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_nick")));
  channel=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_channel")));
  const char* chanpass=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "channelpassword")));
  if(!chanpass[0]){chanpass=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_password")));}
  const char* acc_user=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_username")));
  const char* acc_pass=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_password")));
#ifdef _WIN32
  char cmd[strlen("./tc_client --hexcolors --cookies tinychat.cookie -u    0")+strlen(acc_user)+strlen(channel)+strlen(nick)+strlen(chanpass)];
  strcpy(cmd, "./tc_client --hexcolors ");
  if(config_get_bool("storecookies"))
  {
    strcat(cmd, "--cookies tinychat.cookie ");
  }
  if(acc_user[0])
  {
    strcat(cmd, "-u ");
    strcat(cmd, acc_user);
    strcat(cmd, " ");
  }
  strcat(cmd, channel);
  strcat(cmd, " ");
  strcat(cmd, nick);
  strcat(cmd, " ");
  strcat(cmd, chanpass);
  strcat(cmd, " ");
  w32_runcmdpipes(cmd, tc_client_in, tc_client, coreprocess);
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
    if(config_get_bool("storecookies"))
    {
      const char* home=getenv("HOME");
      char filename[strlen(home)+strlen("/.tinychat.cookie0")];
      sprintf(filename, "%s/.tinychat.cookie", home);
      if(acc_user[0])
      {
        execlp(TC_CLIENT, TC_CLIENT, "--hexcolors", "-u", acc_user, "--cookies", filename, channel, nick, chanpass, (char*)0);
      }else{
        execlp(TC_CLIENT, TC_CLIENT, "--hexcolors", "--cookies", filename, channel, nick, chanpass, (char*)0);
      }
    }else{
      if(acc_user[0])
      {
        execlp(TC_CLIENT, TC_CLIENT, "--hexcolors", "-u", acc_user, channel, nick, chanpass, (char*)0);
      }else{
        execlp(TC_CLIENT, TC_CLIENT, "--hexcolors", channel, nick, chanpass, (char*)0);
      }
    }
  }
#endif
  if(acc_user[0]){dprintf(tc_client_in[1], "%s\n", acc_pass);}
  GIOChannel* tcchannel=g_io_channel_unix_new(tc_client[0]);
  g_io_channel_set_encoding(tcchannel, 0, 0);
  g_io_add_watch(tcchannel, G_IO_IN, handledata, data);
}

void captcha_done(GtkWidget* button, void* x)
{
  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "captcha")));
  gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(gui, "main")));
  write(tc_client_in[1], "\n", 1);
}

int main(int argc, char** argv)
{
  if(!strncmp(argv[0], "./", 2)){frombuild=1;}
  struct viddata datax={0,0,0,0,0};
  data=&datax;
  avcodec_register_all();
  data->vdecoder=avcodec_find_decoder(AV_CODEC_ID_FLV1);
  data->adecoder=avcodec_find_decoder(AV_CODEC_ID_NELLYMOSER);
#ifndef _WIN32
  signal(SIGCHLD, SIG_IGN);
#else
  frombuild=1;
#endif
  config_load();

#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
  #ifdef HAVE_AVRESAMPLE
  data->resamplectx=avresample_alloc_context();
  av_opt_set_int(data->resamplectx, "in_channel_layout", AV_CH_FRONT_CENTER, 0);
  av_opt_set_int(data->resamplectx, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
  // TODO: any way to get the sample rate from the frame/decoder? cam->frame->sample_rate seems to be 0
  av_opt_set_int(data->resamplectx, "in_sample_rate", 11025, 0);
  av_opt_set_int(data->resamplectx, "out_channel_layout", AV_CH_FRONT_CENTER, 0);
  av_opt_set_int(data->resamplectx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
  av_opt_set_int(data->resamplectx, "out_sample_rate", 22050, 0);
  avresample_open(data->resamplectx);
  #else
  data->swrctx=swr_alloc_set_opts(0, AV_CH_FRONT_CENTER, AV_SAMPLE_FMT_S16, 22050, AV_CH_FRONT_CENTER, AV_SAMPLE_FMT_FLT, 11025, 0, 0);
  swr_init(data->swrctx);
  #endif
  int audiopipe[2];
  pipe(audiopipe);
  data->audiopipe=audiopipe[1];
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
  if(frombuild)
  {
    gui=gtk_builder_new_from_file("gtkgui.glade");
  }else{
    gui=gtk_builder_new_from_file(PREFIX "/share/tc_client/gtkgui.glade");
  }
  gtk_builder_connect_signals(gui, 0);

  unsigned int i;
  GtkWidget* item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_broadcast_camera"));
  g_signal_connect(item, "toggled", G_CALLBACK(togglecam), data);
  data->vencoder=avcodec_find_encoder(AV_CODEC_ID_FLV1);
  // Set up cam selection and preview
  campreview.cam=GTK_WIDGET(gtk_builder_get_object(gui, "camselect_preview"));
  campreview.frame=av_frame_alloc();
  campreview.frame->data[0]=0;
  GtkComboBox* combo=GTK_COMBO_BOX(gtk_builder_get_object(gui, "camselect_combo"));
  g_signal_connect(combo, "changed", G_CALLBACK(camselect_change), data->vencoder);
  // Signals for cancelling
  item=GTK_WIDGET(gtk_builder_get_object(gui, "camselection"));
  g_signal_connect(item, "delete-event", G_CALLBACK(camselect_cancel), 0);
  item=GTK_WIDGET(gtk_builder_get_object(gui, "camselect_cancel"));
  g_signal_connect(item, "clicked", G_CALLBACK(camselect_cancel), 0);
  // Signals for switching from preview to streaming
  item=GTK_WIDGET(gtk_builder_get_object(gui, "camselect_ok"));
  g_signal_connect(item, "clicked", G_CALLBACK(camselect_accept), data->vencoder);
  // Enable the "img" camera
  cam_img_filepicker=camselect_file;
  // Populate list of cams
  unsigned int count;
  char** cams=cam_list(&count);
  GtkListStore* list=gtk_list_store_new(1, G_TYPE_STRING);
  for(i=0; i<count; ++i)
  {
    gtk_list_store_insert_with_values(list, 0, -1, 0, cams[i], -1);
    free(cams[i]);
  }
  free(cams);
  gtk_combo_box_set_model(combo, GTK_TREE_MODEL(list));
  g_object_unref(list);
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), gtk_cell_renderer_text_new(), 1);

  item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_options_settings"));
  g_signal_connect(item, "activate", G_CALLBACK(showsettings), gui);
  item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_options_settings2"));
  g_signal_connect(item, "activate", G_CALLBACK(showsettings), gui);

  cambox=GTK_WIDGET(gtk_builder_get_object(gui, "cambox"));
  userlistwidget=GTK_WIDGET(gtk_builder_get_object(gui, "userlistbox"));
  GtkWidget* chatview=GTK_WIDGET(gtk_builder_get_object(gui, "chatview"));
  data->scroll=gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(gtk_builder_get_object(gui, "chatscroll")));

  data->buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(chatview));
  buffer_setup_colors(data->buffer);

  GtkWidget* panes=GTK_WIDGET(gtk_builder_get_object(gui, "vpaned"));
  g_signal_connect(panes, "notify::position", G_CALLBACK(handleresizepane), data);
  gtk_paned_set_wide_handle(GTK_PANED(panes), 1);
  panes=GTK_WIDGET(gtk_builder_get_object(gui, "chatnick"));
  gtk_paned_set_wide_handle(GTK_PANED(panes), 1);

  GtkWidget* inputfield=GTK_WIDGET(gtk_builder_get_object(gui, "inputfield"));
  g_signal_connect(inputfield, "activate", G_CALLBACK(sendmessage), data);
  g_signal_connect(inputfield, "key-press-event", G_CALLBACK(inputkeys), data);

  // Sound
  GtkWidget* option=GTK_WIDGET(gtk_builder_get_object(gui, "soundradio_cmd"));
  g_signal_connect(option, "toggled", G_CALLBACK(toggle_soundcmd), gui);
  // Logging
  option=GTK_WIDGET(gtk_builder_get_object(gui, "enable_logging"));
  g_signal_connect(option, "toggled", G_CALLBACK(toggle_logging), gui);
  option=GTK_WIDGET(gtk_builder_get_object(gui, "save_settings"));
  g_signal_connect(option, "clicked", G_CALLBACK(savesettings), gui);
  // Youtube
  option=GTK_WIDGET(gtk_builder_get_object(gui, "youtuberadio_cmd"));
  g_signal_connect(option, "toggled", G_CALLBACK(toggle_youtubecmd), gui);
  // Misc
  option=GTK_WIDGET(gtk_builder_get_object(gui, "camdownonjoin"));

  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "main"));
  g_signal_connect(window, "configure-event", G_CALLBACK(handleresize), data);

  // Start window and channel password window signals
  GtkWidget* button=GTK_WIDGET(gtk_builder_get_object(gui, "channelpasswordbutton"));
  g_signal_connect(button, "clicked", G_CALLBACK(startsession), (void*)-1); // &data);
  button=GTK_WIDGET(gtk_builder_get_object(gui, "channelpassword"));
  g_signal_connect(button, "activate", G_CALLBACK(startsession), (void*)-1); // &data);
  GtkWidget* startwindow=GTK_WIDGET(gtk_builder_get_object(gui, "startwindow"));
  // Connect signal for quick connect
  item=GTK_WIDGET(gtk_builder_get_object(gui, "start_menu_connect"));
  struct channelopts cc_connect={-1,0};
  g_signal_connect(item, "activate", G_CALLBACK(channeldialog), &cc_connect);
  // Connect signal for the add option
  item=GTK_WIDGET(gtk_builder_get_object(gui, "start_menu_add"));
  struct channelopts cc_add={-1,1};
  g_signal_connect(item, "activate", G_CALLBACK(channeldialog), &cc_add);
  // Connect signal for tab changing (to un-highlight)
  item=GTK_WIDGET(gtk_builder_get_object(gui, "tabs"));
  g_signal_connect(item, "switch-page", G_CALLBACK(pm_select), 0);
  // Connect signal for captcha
  g_signal_connect(gtk_builder_get_object(gui, "captcha_done"), "clicked", G_CALLBACK(captcha_done), 0);
  // Connect signals for camera postprocessing
  g_signal_connect(gtk_builder_get_object(gui, "cam_menu_colors"), "activate", G_CALLBACK(gui_show_camcolors), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_min_brightness"), "value-changed", G_CALLBACK(camcolors_adjust_min), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_max_brightness"), "value-changed", G_CALLBACK(camcolors_adjust_max), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_auto"), "toggled", G_CALLBACK(camcolors_toggle_auto), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_flip_horizontal"), "toggled", G_CALLBACK(camcolors_toggle_flip), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_flip_vertical"), "toggled", G_CALLBACK(camcolors_toggle_flip), (void*)1);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreen_filechooser"), "file-set", G_CALLBACK(gui_set_greenscreen_img), 0);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreen_colorpicker"), "color-set", G_CALLBACK(gui_set_greenscreen_color), 0);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreen_tolerance_h"), "value-changed", G_CALLBACK(gui_set_greenscreen_tolerance), 0);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreen_tolerance_s"), "value-changed", G_CALLBACK(gui_set_greenscreen_tolerance), (void*)1);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreen_tolerance_v"), "value-changed", G_CALLBACK(gui_set_greenscreen_tolerance), (void*)2);
  // Connect signal for hiding cameras
  g_signal_connect(gtk_builder_get_object(gui, "cam_menu_hide"), "activate", G_CALLBACK(gui_hide_cam), 0);
  // Load placeholder animation for cameras (no video data yet or mic only)
  if(frombuild)
  {
    camplaceholder=gdk_pixbuf_animation_new_from_file("camplaceholder.gif", 0);
  }else{
    camplaceholder=gdk_pixbuf_animation_new_from_file(PREFIX "/share/tc_client/camplaceholder.gif", 0);
  }
  camplaceholder_iter=gdk_pixbuf_animation_get_iter(camplaceholder, 0);
  // Populate saved channels
  GtkWidget* startbox=GTK_WIDGET(gtk_builder_get_object(gui, "startbox"));
  int channelcount=config_get_int("channelcount");
  if(channelcount)
  {
    gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(gui, "channel_placeholder")));
  }
  char buf[256];
  for(i=0; i<channelcount; ++i)
  {
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    #ifdef GTK_STYLE_CLASS_LINKED
      GtkStyleContext* style=gtk_widget_get_style_context(box);
      gtk_style_context_add_class(style, GTK_STYLE_CLASS_LINKED);
    #endif
    sprintf(buf, "channel%i_name", i);
    const char* name=config_get_str(buf);
    GtkWidget* connectbutton=gtk_button_new_with_label(name);
    g_signal_connect(connectbutton, "clicked", G_CALLBACK(startsession), (void*)(intptr_t)i);
    gtk_box_pack_start(GTK_BOX(box), connectbutton, 1, 1, 0);
    GtkWidget* cfgbutton=gtk_button_new_from_icon_name("gtk-preferences", GTK_ICON_SIZE_BUTTON);
    struct channelopts* opts=malloc(sizeof(struct channelopts));
    opts->channel_id=i;
    opts->save=1;
    g_signal_connect(cfgbutton, "clicked", G_CALLBACK(channeldialog), opts);
    gtk_box_pack_start(GTK_BOX(box), cfgbutton, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(startbox), box, 0, 0, 2);
  }
  gtk_widget_show_all(startwindow);

  gtk_main();
 
#ifdef _WIN32
  if(coreprocess.hProcess)
  {
    TerminateProcess(coreprocess.hProcess, 0);
  }
  if(camproc)
  {
    TerminateProcess(camproc, 0);
  }
#endif
  camera_cleanup();
#ifdef HAVE_AVRESAMPLE
  avresample_free(&data->resamplectx);
#elif defined(HAVE_SWRESAMPLE)
  swr_free(&data->swrctx);
#endif
  return 0;
}
