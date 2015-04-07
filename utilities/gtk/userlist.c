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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
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

struct user* adduser(const char* nick)
{
  struct user* user=finduser(nick);
  if(user){return user;} // User already existed (this might happen when running /names)
  ++usercount;
  userlist=realloc(userlist, sizeof(struct user)*usercount);
  userlist[usercount-1].nick=strdup(nick);
  userlist[usercount-1].label=gtk_label_new(nick); // TODO: some kind of menubutton for actions?
#if GTK_MAJOR_VERSION>=3
  gtk_widget_set_halign(userlist[usercount-1].label, GTK_ALIGN_START);
#endif
  userlist[usercount-1].ismod=0;
  gtk_box_pack_start(GTK_BOX(userlistwidget), userlist[usercount-1].label, 0, 0, 0);
  gtk_widget_show(userlist[usercount-1].label);
  return &userlist[usercount-1];
}

void renameuser(const char* old, const char* newnick)
{
  struct user* user=finduser(old);
  if(!user){return;}
  free(user->nick);
  user->nick=strdup(newnick);
  if(user->ismod)
  {
    char newlabel[strlen(newnick)+2];
    newlabel[0]='@';
    strcpy(&newlabel[1], newnick);
    gtk_label_set_text(GTK_LABEL(user->label), newlabel);
  }else{
    gtk_label_set_text(GTK_LABEL(user->label), newnick);
  }
}

void removeuser(const char* nick)
{
  unsigned int i;
  for(i=0; i<usercount; ++i)
  {
    if(!strcmp(userlist[i].nick, nick))
    {
      free(userlist[i].nick);
      gtk_widget_destroy(userlist[i].label);
      --usercount;
      memmove(&userlist[i], &userlist[i+1], (usercount-i)*sizeof(struct user));
      return;
    }
  }
}
