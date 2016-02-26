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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

char** inputhistory_lines=0;
unsigned int inputhistory_count=0;
unsigned int inputhistory_current=0;

void inputhistory(GtkEntry* entry, GdkEventKey* event)
{
  if(event->type!=GDK_KEY_PRESS){return;}
  if(event->keyval==GDK_KEY_Up)
  {
    if(inputhistory_current>0)
    {
      --inputhistory_current;
      gtk_entry_set_text(entry, inputhistory_lines[inputhistory_current]);
      gtk_editable_set_position(GTK_EDITABLE(entry), strlen(inputhistory_lines[inputhistory_current]));
    }
  }
  else if(event->keyval==GDK_KEY_Down)
  {
    if(inputhistory_current<inputhistory_count)
    {
      ++inputhistory_current;
      if(inputhistory_current==inputhistory_count)
      {
        gtk_entry_set_text(entry, "");
      }else{
        gtk_entry_set_text(entry, inputhistory_lines[inputhistory_current]);
        gtk_editable_set_position(GTK_EDITABLE(entry), strlen(inputhistory_lines[inputhistory_current]));
      }
    }
  }
}

void inputhistory_add(const char* line)
{
  ++inputhistory_count;
  inputhistory_lines=realloc(inputhistory_lines, sizeof(char*)*inputhistory_count);
  inputhistory_lines[inputhistory_count-1]=strdup(line);
  inputhistory_current=inputhistory_count;
}
