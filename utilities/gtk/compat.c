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
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include "compat.h"

#ifdef _WIN32
SECURITY_ATTRIBUTES sa={
  .nLength=sizeof(SECURITY_ATTRIBUTES),
  .bInheritHandle=1,
  .lpSecurityDescriptor=0
};
#endif

#if GTK_MAJOR_VERSION<3
  GtkWidget* gtk_box_new(int vertical, int spacing)
  {
    if(vertical)
    {
      return gtk_vbox_new(1, spacing);
    }else{
      return gtk_hbox_new(1, spacing);
    }
  }
  int gtk_widget_get_allocated_width(GtkWidget* widget)
  {
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    return alloc.width;
  }
  int gtk_widget_get_allocated_height(GtkWidget* widget)
  {
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    return alloc.height;
  }
  char* newline(char* line)
  {
    unsigned int i;
    for(i=0; line[i] && line[i]!='\n' && line[i]!='\r'; ++i);
    return &line[i];
  }
  // Hack to let us load a glade GUI designed for gtk+-3.x
  GtkBuilder* gtk_builder_new_from_file(const char* filename)
  {
    struct stat st;
    if(stat(filename, &st)){return 0;}
    char buf[st.st_size+10];
    int f=open(filename, O_RDONLY);
    read(f, buf, st.st_size);
    close(f);
    buf[st.st_size]=0;
    char* pos;
    if((pos=strstr(buf, "<requires "))) // Don't require anything
    {
      char* end=newline(pos);
      memmove(pos, end, strlen(end)+1);
    }
    // Convert orientation properties into GtkV/GtkH object types
    char* orientation;
    while((orientation=strstr(buf, "<property name=\"orientation\">")))
    {
      char dir=toupper(orientation[29]);
      pos=newline(orientation);
      memmove(orientation, pos, strlen(pos)+1);
      pos=orientation;
      while(pos>buf && strncmp(pos, "class=\"Gtk", 10)){--pos;}
      if(pos>buf)
      {
        memmove(&pos[11], &pos[10], strlen(&pos[10])+1);
        pos[10]=dir;
      }
    }
    // Convert remaining GtkBoxes and GtkPaneds with the default orientation
    while((pos=strstr(buf, "class=\"GtkBox\"")) || (pos=strstr(buf, "class=\"GtkPaned\"")))
    {
      memmove(&pos[11], &pos[10], strlen(&pos[10])+1);
      pos[10]='H'; // Default is horizontal
    }
    GtkBuilder* gui=gtk_builder_new();
    GError* error=0;
    if(!gtk_builder_add_from_string(gui, buf, -1, &error)){g_error("%s\n", error->message);}
    return gui;
  }
#elif GTK_MAJOR_VERSION==3 && GTK_MINOR_VERSION<10
  GtkBuilder* gtk_builder_new_from_file(const char* filename)
  {
    GtkBuilder* gui=gtk_builder_new();
    gtk_builder_add_from_file(gui, filename, 0);
    return gui;
  }
#endif
