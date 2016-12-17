/*
    tc_client-gtk, a graphical user interface for tc_client
    Copyright (C) 2016  alicia@ion.nu

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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libswscale/swscale.h>
#if LIBAVUTIL_VERSION_MAJOR>50 || (LIBAVUTIL_VERSION_MAJOR==50 && LIBAVUTIL_VERSION_MINOR>37)
  #include <libavutil/imgutils.h>
#else
  #include <libavcore/imgutils.h>
#endif
#include "../compat.h"
#ifndef NO_PRCTL
  #include <sys/prctl.h>
#endif
#include "compat.h"
#include "gui.h"
#include "main.h"
#include "configfile.h"
#include "userlist.h"
#include "greenroom.h"

int greenroompipe[2];
int greenroompipe_in[2]={-1,-1};
// TODO: Handle outgoing cam, sending to greenroom if we don't have the broadcast password yet
// TODO: Option to show greenroom as non-mod

struct greenmap
{
  const char* id;
  const char* nick;
  const char* camid;
};
static struct greenmap* users=0;
static unsigned int usercountgr=0;
static GtkWidget* menu=0;
static GtkWidget* menuitem=0;

static void greenroom_updatecount(void)
{
  // Ignore any leftover users that we didn't find the nickname for
  unsigned int count=0;
  unsigned int i;
  for(i=0; i<usercountgr; ++i){count+=!!users[i].nick;}
  if(count)
  {
    char buf[snprintf(0, 0, "Greenroom (%u)", count)+1];
    sprintf(buf, "Greenroom (%u)", count);
    gtk_menu_item_set_label(GTK_MENU_ITEM(menuitem), buf);
  }else{
    gtk_menu_item_set_label(GTK_MENU_ITEM(menuitem), "Greenroom");
  }
}

static void greenroom_remove(const char* id, char cam)
{
  unsigned int i;
  for(i=0; i<usercountgr; ++i)
  {
    const char* str=(cam?users[i].camid:users[i].id);
    if(str && !strcmp(str, id))
    {
      free((void*)users[i].id);
      free((void*)users[i].nick);
      free((void*)users[i].camid);
      --usercountgr;
      memmove(&users[i], &users[i+1], sizeof(struct greenmap)*(usercountgr-i));
      greenroom_updatecount();
      return;
    }
  }
}

static void greenroom_allow(GtkWidget* menuitem, void* x)
{
  unsigned int i;
  for(i=0; i<usercountgr; ++i)
  {
    if(!strcmp(users[i].id, x))
    {
      dprintf(tc_client_in[1], "/allow %s\n", users[i].nick);
    }
  }
}

static gboolean greenroom_handleline(GIOChannel* iochannel, GIOCondition condition, gpointer x)
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
  char* space=strchr(buf, ' ');
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
    camera_decode(cam, &pkt, 160, 120);
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
#ifdef HAVE_LIBAO
    if(gtk_widget_is_visible(menu)) // Only play audio when looking at the greenroom
    {
      // Find the camera representation for the given ID (for decoder context)
      struct camera* cam=camera_find(&buf[7]);
      if(!cam){printf("No cam found with ID '%s'\n", &buf[7]); return 1;}
      if(!cam->actx && camera_init_audio(cam, frameinfo)){return 1;}
      mic_decode(cam, &pkt);
    }
#endif
    return 1;
  }
  if(!strncmp(buf, "VideoEnd: ", 10))
  {
    // TODO: Deal with potential collisions with cams in the main channel
    camera_remove(&buf[10], 0);
    greenroom_remove(&buf[10], 1);
    return 1;
  }
  // For both "currently on cam" and "cammed up", add to a list and reply with a "/getnick", whose response will complete the item and then we can announce it
  if(space && menu && !strcmp(space, " cammed up"))
  {
    // Find out who this cam belongs to
    space[0]=0;
    dprintf(tc_client_in[1], "/getnick %s\n", buf);
    ++usercountgr;
    users=realloc(users, sizeof(struct greenmap)*usercountgr);
    users[usercountgr-1].id=strdup(buf);
    users[usercountgr-1].nick=0;
    users[usercountgr-1].camid=0;
    return 1;
  }
  if(menu && !strncmp(buf, "Currently on cam: ", 18))
  {
    char* next=&buf[16];
    while(next)
    {
      char* user=&next[2];
      next=strstr(user, ", ");
      if(!user[0]){continue;}
      if(next){next[0]=0;}
      dprintf(tc_client_in[1], "/getnick %s\n", user);
      ++usercountgr;
      users=realloc(users, sizeof(struct greenmap)*usercountgr);
      users[usercountgr-1].id=strdup(user);
      users[usercountgr-1].nick=0;
      users[usercountgr-1].camid=0;
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
    struct camera* cam=camera_new(nick, id, CAMFLAG_GREENROOM);
    unsigned int i;
    for(i=0; i<usercountgr; ++i)
    {
      if(!strcmp(users[i].id, nick))
      {
        cam->label=gtk_label_new(users[i].nick);
        users[i].camid=strdup(id);
      }
    }
    // Add it to the greenroom menu
    cam->box=gtk_menu_item_new();
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(box), cam->cam, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(box), cam->label, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(cam->box), box);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), cam->box);
    g_signal_connect(cam->box, "activate", G_CALLBACK(greenroom_allow), cam->nick);
    gtk_widget_show_all(cam->box);
    greenroom_updatecount();
    return 1;
  }
  if(!strncmp(buf, "Captcha: ", 9))
  {
    // If we're a mod, don't bother with the captcha (since we're only here to look at cams)
    if(user_ismod(nickname))
    {
      write(greenroompipe_in[1], "\n", 1);
    }else{
      gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(gui, "captcha")));
      char link[snprintf(0,0,"Captcha: <a href=\"%s\">%s</a>", &buf[9], &buf[9])+1];
      sprintf(link, "Captcha: <a href=\"%s\">%s</a>", &buf[9], &buf[9]);
      gtk_label_set_markup(GTK_LABEL(gtk_builder_get_object(gui, "captcha_link")), link);
    }
    return 1;
  }
  if(buf[0]=='['&&isdigit(buf[1])&&isdigit(buf[2])&&buf[3]==':'&&isdigit(buf[4])&&isdigit(buf[5])&&buf[6]==']'&&buf[7]==' ')
  {
    space=strchr(&buf[8], ' ');
    if(!strcmp(space, " left the channel"))
    {
      space[0]=0;
      camera_remove(&buf[8], 1);
      greenroom_remove(&buf[8], 0);
    }
    return 1;
  }
  return 1;
}

char greenroom_gotnick(const char* id, const char* nick)
{
  unsigned int i;
  for(i=0; i<usercountgr; ++i)
  {
    if(!strcmp(users[i].id, id))
    {
      free((void*)users[i].nick); // just in case
      users[i].nick=strdup(nick);
      dprintf(greenroompipe_in[1], "/opencam %s\n", id);
      return 1;
    }
  }
  return 0;
}

void greenroom_join(const char* id)
{
  // Only add greenroom menu if mod (or optionally see greenroom anyway?)
  if(user_ismod(nickname) || config_get_bool("showgreenroom"))
  {
    menuitem=gtk_menu_item_new_with_label("Greenroom");
    gtk_menu_shell_append(GTK_MENU_SHELL(gtk_builder_get_object(gui, "menubar")), menuitem);
    menu=gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
    gtk_widget_show_all(menuitem);
    gtk_widget_show_all(menu);
  }
#ifdef _WIN32
  char cmd[strlen("./tc_client --greenroom --cookies tinychat_no_account.cookie  0")+strlen(channel)+strlen(id)];
  strcpy(cmd, "./tc_client --greenroom ");
  if(config_get_bool("storecookies"))
  {
    strcat(cmd, "--cookies tinychat_no_account.cookie");
  }
  strcat(cmd, channel);
  strcat(cmd, " ");
  strcat(cmd, id);
  w32_runcmdpipes(cmd, greenroompipe_in, greenroompipe, coreprocess);
#else
  pipe(greenroompipe);
  pipe(greenroompipe_in);
  if(!fork())
  {
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    close(greenroompipe[0]);
    close(greenroompipe_in[1]);
    dup2(greenroompipe[1], 1);
    dup2(greenroompipe_in[0], 0);
    if(config_get_bool("storecookies"))
    {
      const char* home=getenv("HOME");

      char filename[strlen(home)+strlen("/.config/tc_client-gtk.cookies/no_account0")];
      sprintf(filename, "%s/.config", home);
      mkdir(filename, 0700);
      sprintf(filename, "%s/.config/tc_client-gtk.cookies", home);
      mkdir(filename, 0700);
      sprintf(filename, "%s/.config/tc_client-gtk.cookies/no_account", home);
      execlp(TC_CLIENT, TC_CLIENT, "--greenroom", "--cookies", filename, channel, id, (char*)0);
    }else{
      execlp(TC_CLIENT, TC_CLIENT, "--greenroom", channel, id, (char*)0);
    }
    _exit(1);
  }
#endif
  GIOChannel* tcchannel=g_io_channel_unix_new(greenroompipe[0]);
  g_io_channel_set_encoding(tcchannel, 0, 0);
  g_io_add_watch(tcchannel, G_IO_IN, greenroom_handleline, 0);
}

void greenroom_changenick(const char* from, const char* to)
{
  unsigned int i;
  for(i=0; i<usercountgr; ++i)
  {
    if(users[i].nick && !strcmp(users[i].nick, from))
    {
      free((void*)users[i].nick);
      users[i].nick=strdup(to);
      struct camera* cam=camera_find(users[i].camid);
      if(cam){gtk_label_set_text(GTK_LABEL(cam->label), to);}
    }
  }
}
