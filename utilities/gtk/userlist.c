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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gui.h"
#include "compat.h"
#include "main.h"
#include "configfile.h"
#include "userlist.h"

struct user* userlist=0;
unsigned int usercount=0;
GtkWidget* userlistwidget=0;

struct user* finduser(const char* nick)
{
  unsigned int i;
  for(i=0; i<usercount; ++i)
  {
    if(!strcmp(userlist[i].nick, nick)){return &userlist[i];}
  }
  return 0;
}

struct user* user_find_by_tab(GtkWidget* tab)
{
  unsigned int i;
  for(i=0; i<usercount; ++i)
  {
    if(userlist[i].pm_tab==tab){return &userlist[i];}
  }
  return 0;
}

struct user* adduser(const char* nick)
{
  struct user* user=finduser(nick);
  if(user){return user;} // User already existed (this might happen when running /names)
  ++usercount;
  userlist=realloc(userlist, sizeof(struct user)*usercount);
  userlist[usercount-1].nick=strdup(nick);
  userlist[usercount-1].label=gtk_label_new(nick); // TODO: some kind of menubutton for actions?
  userlist[usercount-1].item=userlist[usercount-1].label;
  userlist[usercount-1].pm_tab=0;
  userlist[usercount-1].pm_tablabel=0;
  userlist[usercount-1].pm_chatview=0;
  userlist[usercount-1].pm_highlight=0;
#if GTK_MAJOR_VERSION>=3
  gtk_widget_set_halign(userlist[usercount-1].label, GTK_ALIGN_START);
#endif
  userlist[usercount-1].ismod=0;
  gtk_box_pack_start(GTK_BOX(userlistwidget), userlist[usercount-1].label, 0, 0, 0);
  gtk_widget_show(userlist[usercount-1].label);
  userlist_sort();
  return &userlist[usercount-1];
}

void renameuser(const char* old, const char* newnick)
{
  struct user* user=finduser(old);
  if(!user){return;}
  free(user->nick);
  user->nick=strdup(newnick);
  gtk_label_set_text(GTK_LABEL(user->label), newnick);
  if(user->pm_tablabel)
  {
    if(user->pm_highlight)
    {
      pm_highlight(newnick);
    }else{
      gtk_label_set_text(GTK_LABEL(user->pm_tablabel), newnick);
    }
  }
  userlist_sort();
}

void removeuser(const char* nick)
{
  unsigned int i;
  for(i=0; i<usercount; ++i)
  {
    if(!strcmp(userlist[i].nick, nick))
    {
      free(userlist[i].nick);
      gtk_widget_destroy(userlist[i].item);
      --usercount;
      memmove(&userlist[i], &userlist[i+1], (usercount-i)*sizeof(struct user));
      return;
    }
  }
}

void usersetmod(const char* nick, char mod)
{
  struct user* user=finduser(nick);
  if(!user || mod==user->ismod){return;}
  user->ismod=mod;
  g_object_ref(user->label); // Keep label from getting destroyed
  if(mod)
  { // Add icon to symbolize being a moderator
    gtk_container_remove(GTK_CONTAINER(userlistwidget), user->label);
    user->item=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(user->item), user->label, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(user->item), gtk_image_new_from_pixbuf(modicon), 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(userlistwidget), user->item, 0, 0, 0);
    gtk_widget_show_all(user->item);
  }else{
    gtk_widget_destroy(user->item);
    gtk_box_pack_start(GTK_BOX(userlistwidget), user->label, 0, 0, 0);
    user->item=user->label;
  }
  g_object_unref(user->label);
  userlist_sort();
}

signed char usercmp(const struct user* a, const struct user* b)
{
  if(a==b){return 0;}
  signed char r=strcasecmp(a->nick, b->nick); // Primarily sort case-insensitively
  if(!r){r=strcmp(a->nick, b->nick);} // But fall back to case-sensitive if they match
  if(config_get_bool("userlist_sort_mod") && a->ismod^b->ismod){r=b->ismod-a->ismod;}
  if(config_get_bool("userlist_sort_self"))
  {
    if(!strcmp(a->nick, nickname)){r=-1;}
    else if(!strcmp(b->nick, nickname)){r=1;}
  }
  return r;
}

void userlist_sort(void)
{
  unsigned int i;
  for(i=0; i<usercount; ++i)
  {
    unsigned int position=0;
    unsigned int i2;
    for(i2=0; i2<usercount; ++i2)
    {
      if(usercmp(&userlist[i], &userlist[i2])>0){++position;}
    }
    gtk_box_reorder_child(GTK_BOX(userlistwidget), userlist[i].item, position);
  }
}

char user_ismod(const char* nick)
{
  struct user* user=finduser(nick);
  if(!user){return 0;}
  return user->ismod;
}
