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

extern char autoscroll_before(GtkAdjustment* scroll);
extern void autoscroll_after(GtkAdjustment* scroll);
extern void settings_reset(GtkBuilder* gui);
extern void showsettings(GtkMenuItem* item, GtkBuilder* gui);
extern void savesettings(GtkButton* button, GtkBuilder* gui);
extern void toggle_soundcmd(GtkToggleButton* button, GtkBuilder* gui);
extern void toggle_logging(GtkToggleButton* button, GtkBuilder* gui);
extern void toggle_youtubecmd(GtkToggleButton* button, GtkBuilder* gui);
