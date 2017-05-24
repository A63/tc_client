/*
    tc_client-gtk, a graphical user interface for tc_client
    Copyright (C) 2015-2017  alicia@ion.nu
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
  #include <sys/wait.h>
#endif
#include <ctype.h>
#include <sys/stat.h>
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
#elif defined(HAVE_SWRESAMPLE)
  #include <libswresample/swresample.h>
#endif
#if defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)
  #ifdef HAVE_PULSEAUDIO
    #include <pulse/simple.h>
  #elif defined(HAVE_LIBAO)
    #include <ao/ao.h>
  #endif
#endif
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#ifdef HAVE_WEBKITGTK
#include <webkit2/webkit2.h>
#endif
#include "../compat.h"
#include "../compat_av.h"
#ifndef NO_PRCTL
  #include <sys/prctl.h>
#endif
#include "../libcamera/camera.h"
#include "userlist.h"
#include "media.h"
#include "compat.h"
#include "configfile.h"
#include "gui.h"
#include "logging.h"
#include "../stringutils.h"
#include "inputhistory.h"
#include "playmedia.h"
#include "greenroom.h"
#include "main.h"

struct viddata
{
  AVCodec* vdecoder;
  AVCodec* vencoder;
};
struct viddata* data;

int tc_client[2];
int tc_client_in[2];
const char* channel=0;
const char* mycolor=0;
char* nickname=0;
char hasgreenroom=0;
char frombuild=0; // Running from the build directory
#ifdef _WIN32
  PROCESS_INFORMATION coreprocess={.hProcess=0};
  PROCESS_INFORMATION grprocess={.hProcess=0}; // Greenroom
#endif

void printchat(const char* text, const char* color, unsigned int offset, const char* pm)
{
  struct chatview* chatview;
  struct user* user;
  if(pm && (user=finduser(pm)))
  {
    pm_open(pm, 0);
    chatview=user->pm_chatview;
  }else{
    chatview=mainchat;
  }
  GtkTextBuffer* buffer=gtk_text_view_get_buffer(chatview->textview);
  // Insert new content
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buffer, &end);
  gtk_text_buffer_insert(buffer, &end, "\n", -1);
  int startnum=gtk_text_iter_get_offset(&end);
  // Insert links and regular text separately
  const char* linktext=text;
  const char* link;
  while((link=strstr(linktext, "://")))
  {
    while(link>linktext && link[-1]!=' '){link=&link[-1];}
    unsigned int linklen;
    for(linklen=0; link[linklen] && link[linklen]!=' '; ++linklen);
    if(linklen<6){continue;}
    gtk_text_buffer_insert(buffer, &end, linktext, link-linktext);
    gui_insert_link(buffer, &end, link, linklen);
    linktext=&link[linklen];
  }
  gtk_text_buffer_insert(buffer, &end, linktext, -1);
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
  chatview_autoscroll(chatview);
}

#ifdef HAVE_WEBKITGTK
static void removefrom(GtkWidget* widget, void* parent)
{
  gtk_container_remove(parent, widget);
}
#endif

void captcha_done(void* x, void* y)
{
  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "captcha"));
  gtk_widget_hide(window);
  gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(gui, "main")));
  write(tc_client_in[1], "\n", 1);
  if(greenroompipe_in[1]>-1){write(greenroompipe_in[1], "\n", 1);}
#ifdef HAVE_WEBKITGTK // Get rid of the webkit view when we're done with it
  gtk_container_foreach(GTK_CONTAINER(window), removefrom, window);
}

void captchajs(WebKitWebView* webview, WebKitLoadEvent event, gpointer data)
{
  // Hide unrelated parts of the page (tinychat-specific)
  webkit_web_view_run_javascript(webview, "document.getElementById('footer').style.display='none'; document.getElementById('tinychat').style.display='none';", 0, 0, 0);
  if(event!=WEBKIT_LOAD_FINISHED){return;}
  g_signal_handlers_disconnect_by_data(webview, data);
  // Add a note about loading the captcha (which is pretty slow)
  webkit_web_view_run_javascript(webview, "var loaddiv=document.createElement('div'); loaddiv.appendChild(document.createTextNode('Loading captcha...')); loaddiv.style.background='rgb(255,255,255)'; document.body.appendChild(loaddiv); CaptchaSolvedSuccessfully=window.close;", 0, 0, 0);
  const char* js=data;
  // Put the javascript into the body tag's onload event, which is still slightly later than WEBKIT_LOAD_FINISHED
  char buf[strlen("document.body.onload=function(){};0")+strlen(js)];
  sprintf(buf, "document.body.onload=function(){%s};", js);
  webkit_web_view_run_javascript(webview, buf, 0, 0, 0);
  free(data);
#endif
}

unsigned int cameventsource=0;
gboolean handledata(GIOChannel* iochannel, GIOCondition condition, gpointer x)
{
  static char buf[1024];
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
    unsigned int size=strtoul(&sizestr[1], 0, 0);
    if(!size){return 1;}
    // Mostly ignore the first byte (contains frame type (e.g. keyframe etc.) in 4 bits and codec in the other 4)
    --size;
    AVPacket pkt;
    av_init_packet(&pkt);
    unsigned char databuf[size+4];
    pkt.data=databuf;
    pkt.size=size;
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
    // Find the camera representation for the given ID
    struct camera* cam=camera_find(&buf[7]);
    if(!cam){printf("No cam found with ID '%s'\n", &buf[7]); return 1;}
    camera_decode(cam, &pkt, camsize_scale.width, camsize_scale.height);
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
    if(!cam->actx && camera_init_audio(cam, frameinfo)){return 1;}
    mic_decode(cam, &pkt);
#endif
    return 1;
  }
  if(!strncmp(buf, "Nickname of connection ", 23))
  {
    char* nick=strstr(&buf[23], ": ");
    if(nick)
    {
      nick[0]=0;
      if(greenroom_gotnick(&buf[23], &nick[2]))
      {
        char buf[strlen(&nick[2])+strlen(" is waiting in the greenroom0")];
        strcpy(buf, &nick[2]);
        strcat(buf, " is waiting in the greenroom");
        printchat(buf, 0, 0, 0);
      }else{
        nick[0]=':';
        printchat(buf, 0, 0, 0);
      }
    }
    return 1;
  }
  if(!strncmp(buf, "Currently online: ", 18))
  {
    printchat(buf, 0, 0, 0);
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
    if(config_get_bool("disablesnapshots")){write(tc_client_in[1], "/disablesnapshots\n", 18);}
    write(tc_client_in[1], "/color\n", 7); // Check which random color tc_client picked
    unsigned int length=strlen(&buf[15]);
    free(nickname);
    nickname=malloc(length+strlen("guest-")+1);
    sprintf(nickname, "guest-%s", &(buf[15]));
    if(hasgreenroom){greenroom_join(&buf[15]);}
    return 1;
  }
  if(!strncmp(buf, "Captcha: ", 9))
  {
    gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "main")));
    GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "captcha"));
    gtk_widget_show_all(window);
  #ifdef HAVE_WEBKITGTK
    // Resize to fit the captcha and clear the window
    gtk_window_resize(GTK_WINDOW(window), 450, 600);
    gtk_container_foreach(GTK_CONTAINER(window), removefrom, window);
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    // Set up webkit webview
    GtkWidget* webview=webkit_web_view_new();
    g_signal_connect(webview, "close", G_CALLBACK(captcha_done), 0);
    gtk_box_pack_start(GTK_BOX(box), webview, 1, 1, 0);
    // Add a "Done" button, for skipping the captcha or if the captcha doesn't close the window itself
    GtkWidget* button=gtk_button_new_with_label("Done");
    g_signal_connect(button, "clicked", G_CALLBACK(captcha_done), 0);
    gtk_box_pack_start(GTK_BOX(box), button, 0, 0, 0);
    gtk_widget_show_all(box);
    // Load the captcha page, and run the javascript if any was specified
    char* js=strstr(&buf[9], " + javascript:");
    if(js){js[0]=0;}
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(webview), &buf[9]);
    if(js)
    {
      js=&js[14];
      // Wait for it to load
      g_signal_connect(webview, "load-changed", G_CALLBACK(captchajs), strdup(js));
    }
  #else
    char link[snprintf(0,0,"Captcha: <a href=\"%s\">%s</a>", &buf[9], &buf[9])+1];
    sprintf(link, "Captcha: <a href=\"%s\">%s</a>", &buf[9], &buf[9]);
    gtk_label_set_markup(GTK_LABEL(gtk_builder_get_object(gui, "captcha_link")), link);
  #endif
    return 1;
  }
  // Start streams once we're properly connected
  if(!strncmp(buf, "Currently on cam: ", 18))
  {
    printchat(buf, 0, 0, 0);
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
  if(!strcmp(buf, "Authenticated to broadcast"))
  {
    greenroom_allowed();
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
    printchat(buf, 0, 0, 0);
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
      char nickbuf[space-nick];
      memcpy(nickbuf, nick, &space[-1]-nick);
      nickbuf[&space[-1]-nick]=0;
      struct user* user=finduser(nickbuf);
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
      if(user && user->ismod && !strncmp(space, " /mbs youTube ", 14))
      {
        if(config_get_bool("youtuberadio_cmd"))
        {
          #ifndef _WIN32
          if(!fork())
          #else
          char* spacetmp=space;
          space=strdup(space);
          #endif
          {
// TODO: store the PID and make sure it's dead before starting a new video? and upon /mbc?
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
#ifdef HAVE_AVFORMAT
        else if(config_get_bool("youtuberadio_embed")){media_play(&space[6]);}
      }
      else if(user && user->ismod && !strncmp(space, " /mbsk youTube ", 15) && config_get_bool("youtuberadio_embed"))
      {
        media_seek(strtol(&space[15], 0, 0));
      }
      else if(user && user->ismod && !strncmp(space, " /mbc ", 6) && config_get_bool("youtuberadio_embed"))
      {
        media_close();
#endif
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
    if(space[-1]==':' || !config_get_bool("hide_notifications"))
    {
      printchat(buf, color, 8, pm);
    }
    pm_highlight(pm);
    free(pm);
    if(space[-1]!=':') // Not a message
    {
      if(!strcmp(space, " entered the channel"))
      {
        if(config_get_bool("camdownonjoin"))
        {
          stopbroadcasting(0, 0);
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
        greenroom_changenick(nick, &space[21]);
      }
    }
    free(color);
    return 1;
  }
  if(!strcmp(buf, "Changed color") || !strncmp(buf, "Current color: ", 15))
  {
    printchat(buf, color, 0, 0);
    free((void*)mycolor);
    mycolor=color;
    return 1;
  }
  if(!strncmp(buf, "Color ", 6))
  {
    printchat(buf, color, 0, 0);
  }
  free(color);
  if(space && !strcmp(space, " is a moderator."))
  {
    space[0]=0;
    usersetmod(buf, 1);
    return 1;
  }
  if(space && !strcmp(space, " is no longer a moderator."))
  {
    space[0]=0;
    usersetmod(buf, 0);
    return 1;
  }
  // Start a stream when someone cams up
  if(space && !strcmp(space, " cammed up"))
  {
    printchat(buf, 0, 0, 0);
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
    struct camera* cam=camera_new(nick, id, CAMFLAG_NONE);
    updatescaling(0, 0, 1);
    gtk_widget_show_all(cam->box);
    return 1;
  }
  if(!strcmp(buf, "Starting outgoing media stream"))
  {
    if(camera_find("out")){return 1;} // In case we're just moving out of greenroom
    struct camera* cam=camera_new(nickname, "out", CAMFLAG_NONE);
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
  if(!strcmp(buf, "Outgoing media stream was closed"))
  {
    stopbroadcasting(0, 0);
    return 1;
  }
  if(!strncmp(buf, "Room topic: ", 12) ||
     (space && (!strcmp(space, " is not logged in") || !strncmp(space, " is logged in as ", 17))))
  {
    printchat(buf, 0, 0, 0);
    return 1;
  }
  if(!strncmp(buf, "Captcha not completed,", 22))
  {
    printchat(buf, 0, 0, 0);
    gui_disableinputs();
    return 1;
  }
  if(!strcmp(buf, "Channel has greenroom"))
  {
    hasgreenroom=1;
    return 1;
  }
  if(!strcmp(buf, "Server disconnected"))
  {
    printchat(buf, 0, 0, 0);
    if(camout_cam)
    {
      cam_close(camout_cam);
      camout_cam=0;
    }
    return 1;
  }
  printf("Got '%s'\n", buf);
  fflush(stdout);
  return 1;
}

#if (defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE)) && (defined(HAVE_PULSEAUDIO) || defined(HAVE_LIBAO))
extern void justwait(int);
void* audiothread(void* fdp)
{
  int fd=*(int*)fdp;
  char buf[2048];
  size_t len;
  #ifdef HAVE_PULSEAUDIO
  pa_simple* pulse;
  pa_sample_spec pulsespec;
  pulsespec.format=PA_SAMPLE_S16NE;
  pulsespec.channels=1;
  pulsespec.rate=SAMPLERATE_OUT;
  signal(SIGCHLD, SIG_DFL); // Briefly revert to the default handler to not break pulseaudio's autospawn feature
  pulse=pa_simple_new(0, "tc_client-gtk", PA_STREAM_PLAYBACK, 0, channel, &pulsespec, 0, 0, 0);
  signal(SIGCHLD, justwait);
  if(!pulse){return 0;}
  while((len=read(fd, buf, 2048))>0)
  {
    if(pa_simple_write(pulse, buf, len, 0)<0){break;}
  }
  pa_simple_free(pulse);
  close(fd);
  #else
  ao_initialize();
  ao_sample_format samplefmt;
  samplefmt.bits=16;
  samplefmt.rate=SAMPLERATE_OUT;
  samplefmt.channels=1;
  samplefmt.byte_format=AO_FMT_NATIVE; // I'm guessing libavcodec decodes it to native
  samplefmt.matrix=0;
  ao_option clientname={.key="client_name", .value="tc_client-gtk", .next=0};
  ao_device* dev=ao_open_live(ao_default_driver_id(), &samplefmt, &clientname);
  while((len=read(fd, buf, 2048))>0)
  {
    ao_play(dev, buf, len);
  }
  ao_close(dev);
  #endif
  return 0;
}
#endif

void togglecam(GtkCheckMenuItem* item, void* x)
{
  if(!gtk_check_menu_item_get_active(item))
  {
    if(!camout_cam){return;}
    cam_close(camout_cam);
    camout_cam=0;
    if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_mic")))){return;}
    // TODO: if switching from cam+mic to just mic, send something to tell other clients to drop the last video frame and show a mic instead
    if(camera_find("out"))
    {
      int fd=((hasgreenroom && !greenroom_gotpass)?greenroompipe_in[1]:tc_client_in[1]);
      dprintf(fd, "/camdown\n");
      camera_remove("out", 0); // Close our local display
    }
    return;
  }
  camselect_open(startcamout, togglecam_cancel);
}

#ifdef HAVE_PULSEAUDIO
void togglemic(GtkCheckMenuItem* item, void* x)
{
  static int micpipe[2];
  static int eventsource=0;
  int fd=((hasgreenroom && !greenroom_gotpass)?greenroompipe_in[1]:tc_client_in[1]);
  if(!gtk_check_menu_item_get_active(item)) // Stop mic broadcast
  {
    gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "pushtotalk")));
    pushtotalk_enabled=0;
    if(!eventsource){return;}
    close(micpipe[0]);
    close(micpipe[1]);
    g_source_remove(eventsource);
    eventsource=0;
    if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_camera")))){return;}
    if(camera_find("out"))
    {
      dprintf(fd, "/camdown\n");
      camera_remove("out", 0); // Close our local display
    }
    return;
  }
  // Choose between push-to-talk and open mic
  int choice=gtk_dialog_run(GTK_DIALOG(gtk_builder_get_object(gui, "pushtotalkdialog")));
  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "pushtotalkdialog")));
  if(choice==GTK_RESPONSE_DELETE_EVENT)
  {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_mic")), 0);
    return;
  }
  if(choice) // 1=push-to-talk, 0=open mic
  {
    gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(gui, "pushtotalk")));
    pushtotalk_enabled=1;
    pushtotalk_pushed=0;
  }
  // Start mic broadcast
  if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_camera"))))
  { // Only /camup if we're not already broadcasting video
    dprintf(fd, "/camup\n");
  }
  pipe(micpipe);
  g_thread_new("audio_in", audiothread_in, &micpipe[1]);
  // Attach micpipe[0] to mic_encode()
  GIOChannel* channel=g_io_channel_unix_new(micpipe[0]);
  g_io_channel_set_encoding(channel, 0, 0);
  eventsource=g_io_add_watch(channel, G_IO_IN, mic_encode, 0);
}
#endif

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
  if(event->keyval==GDK_KEY_Page_Up || event->keyval==GDK_KEY_KP_Page_Up ||
     event->keyval==GDK_KEY_Page_Down || event->keyval==GDK_KEY_KP_Page_Down)
  {
    // Get the relevant chatview
    GtkNotebook* tabs=GTK_NOTEBOOK(gtk_builder_get_object(gui, "tabs"));
    int num=gtk_notebook_get_current_page(tabs);
    struct user* user=0;
    if(num>0)
    {
      GtkWidget* page=gtk_notebook_get_nth_page(tabs, num);
      user=user_find_by_tab(page);
    }
    struct chatview* chatview;
    if(user && user->pm_chatview)
    {
      chatview=user->pm_chatview;
    }else{
      chatview=mainchat;
    }
    // By just passing along the event we also get smooth scrolling, which GTK+ does not expose an API for
    gtk_widget_event(GTK_WIDGET(chatview->textview), (GdkEvent*)event);
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
    pm_open(&msg[4], 1);
    gtk_entry_set_text(entry, "");
    sendingmsg=0;
    return;
  }
#ifdef HAVE_AVFORMAT
  else if(!strncmp(msg, "/mbs ", 5) && config_get_bool("youtuberadio_embed"))
  {
    media_play(&msg[5]);
  }
  else if(!strncmp(msg, "/mbsk ", 6) && config_get_bool("youtuberadio_embed"))
  {
    media_seek(strtol(&msg[6], 0, 0));
  }
  else if(!strncmp(msg, "/mbc ", 5) && config_get_bool("youtuberadio_embed"))
  {
    media_close();
  }
#endif
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
     !strncmp(msg, "/topic ", 7) ||
     !strncmp(msg, "/getnick ", 9))
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
        printchat("No such user", 0, 0, 0);
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
  printchat(text, mycolor, 8, pm);
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
  GtkWidget* mainwindow=GTK_WIDGET(gtk_builder_get_object(gui, "main"));
  const char* nick=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_nick")));
  free(nickname);
  nickname=strdup(nick);
  channel=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_channel")));
  const char* chanpass=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "channelpassword")));
  if(!chanpass[0]){chanpass=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_password")));}
  const char* acc_user=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_username")));
  const char* acc_pass=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_password")));
  const char* cookiename=(acc_user[0]?acc_user:"no_account");
  char title[strlen("tc_client ()0")+strlen(channel)];
  sprintf(title, "tc_client (%s)", channel);
  gtk_window_set_title(GTK_WINDOW(mainwindow), title);
  gtk_widget_show_all(mainwindow);
  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "pushtotalk")));
#ifdef _WIN32
  char cmd[strlen("./tc_client --hexcolors --cookies tinychat_.cookie -u    0")+strlen(cookiename)+strlen(acc_user)+strlen(channel)+strlen(nick)+strlen(chanpass)];
  strcpy(cmd, "./tc_client --hexcolors ");
  if(config_get_bool("storecookies"))
  {
    strcat(cmd, "--cookies tinychat_");
    strcat(cmd, cookiename);
    strcat(cmd, ".cookie ");
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

      char filename[strlen(home)+strlen("/.config/tc_client-gtk.cookies/0")+strlen(cookiename)];
      sprintf(filename, "%s/.config", home);
      mkdir(filename, 0700);
      sprintf(filename, "%s/.config/tc_client-gtk.cookies", home);
      mkdir(filename, 0700);
      sprintf(filename, "%s/.config/tc_client-gtk.cookies/%s", home, cookiename);
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
#if (defined(HAVE_PULSEAUDIO) || defined(HAVE_LIBAO)) && (defined(HAVE_AVRESAMPLE) || defined(HAVE_SWRESAMPLE))
  static int audiopipe[2]={-1,-1};
  if(audiopipe[0]==-1)
  {
    pipe(audiopipe);
    g_thread_new("audio", audiothread, audiopipe);
    g_timeout_add(40, audiomixer, &audiopipe[1]);
  }
#endif
}

#ifndef _WIN32
void justwait(int x){waitpid(-1, 0, WNOHANG);}
#endif

int main(int argc, char** argv)
{
  if(!strncmp(argv[0], "./", 2)){frombuild=1;}
  struct viddata datax={0,0};
  data=&datax;
  avcodec_register_all();
  data->vdecoder=avcodec_find_decoder(AV_CODEC_ID_FLV1);
  data->vencoder=avcodec_find_encoder(AV_CODEC_ID_FLV1);
#ifndef _WIN32
  signal(SIGCHLD, justwait); // SIG_IGN causes trouble with libpulse's autospawn
#else
  frombuild=1;
#endif
  config_load();

  gtk_init(&argc, &argv);
  gui_init(frombuild);
  campreview.frame=av_frame_alloc();
  campreview.frame->data[0]=0;
  gtk_main();
  camera_cleanup();
  write(tc_client_in[1], "/quit\n", 6);
  write(greenroompipe_in[1], "/quit\n", 6);
  sleep(1);
 
#ifdef _WIN32
  if(coreprocess.hProcess)
  {
    TerminateProcess(coreprocess.hProcess, 0);
  }
  if(grprocess.hProcess)
  {
    TerminateProcess(grprocess.hProcess, 0);
  }
#endif
  return 0;
}
