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
#include <gtk/gtk.h>

struct channelopts
{
  int channel_id; // for editing an existing channel, otherwise pass -1
  char save;
};

extern char autoscroll_before(GtkAdjustment* scroll);
extern void autoscroll_after(GtkAdjustment* scroll);
extern void settings_reset(GtkBuilder* gui);
extern void showsettings(GtkMenuItem* item, GtkBuilder* gui);
extern void savesettings(GtkButton* button, GtkBuilder* gui);
extern void toggle_soundcmd(GtkToggleButton* button, GtkBuilder* gui);
extern void toggle_logging(GtkToggleButton* button, GtkBuilder* gui);
extern void toggle_youtubecmd(GtkToggleButton* button, GtkBuilder* gui);
extern void deletechannel(GtkButton* button, void* x);
extern void channeldialog(GtkButton* button, struct channelopts* opts);
extern void pm_open(const char* nick, char select, GtkAdjustment* scroll);
extern void pm_highlight(const char* nick);
extern char pm_select(GtkNotebook* tabs, GtkWidget* tab, int num, void* x);
extern void buffer_setup_colors(GtkTextBuffer* buffer);
extern void buffer_updatesize(GtkTextBuffer* buffer);
extern void fontsize_set(double size);
extern gboolean gui_show_cam_menu(GtkWidget* widget, GdkEventButton* event, const char* id);
extern void gui_show_camcolors(GtkMenuItem* menuitem, void* x);
extern void camcolors_adjust_min(GtkAdjustment* adjustment, void* x);
extern void camcolors_adjust_max(GtkAdjustment* adjustment, void* x);
extern void camcolors_toggle_auto(GtkToggleButton* button, void* x);
extern void camcolors_toggle_flip(GtkToggleButton* button, void* vertical);
extern void gui_hide_cam(GtkMenuItem* menuitem, void* x);

extern GtkBuilder* gui;
