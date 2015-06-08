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
#ifdef _WIN32
#include <wtypes.h>
extern SECURITY_ATTRIBUTES sa;
#endif
#if GTK_MAJOR_VERSION<3
  #define GTK_ORIENTATION_HORIZONTAL 0
  #define GTK_ORIENTATION_VERTICAL 1
  extern GtkWidget* gtk_box_new(int vertical, int spacing);
  extern int gtk_widget_get_allocated_width(GtkWidget* widget);
  extern int gtk_widget_get_allocated_height(GtkWidget* widget);
#endif
#if GTK_MAJOR_VERSION<3 || (GTK_MAJOR_VERSION==3 && GTK_MINOR_VERSION<10)
  #define gtk_button_new_from_icon_name(name, size) gtk_button_new_from_stock(name)
  extern GtkBuilder* gtk_builder_new_from_file(const char* filename);
#endif
