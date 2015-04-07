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
#include "gui.h"
#include "config.h"
#include "logging.h"

char autoscroll_before(GtkAdjustment* scroll)
{
  // Figure out if we're at the bottom and should autoscroll with new content
  int upper=gtk_adjustment_get_upper(scroll);
  int size=gtk_adjustment_get_page_size(scroll);
  int value=gtk_adjustment_get_value(scroll);
  return (value+size==upper);
}

void autoscroll_after(GtkAdjustment* scroll)
{
  while(gtk_events_pending()){gtk_main_iteration();} // Make sure the textview's new size affects scroll's "upper" value first
  int upper=gtk_adjustment_get_upper(scroll);
  int size=gtk_adjustment_get_page_size(scroll);
  gtk_adjustment_set_value(scroll, upper-size);
}

void settings_reset(GtkBuilder* gui)
{
  // Sound
  GtkWidget* option=GTK_WIDGET(gtk_builder_get_object(gui, "soundradio_cmd"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(option), config_get_bool("soundradio_cmd"));
  option=GTK_WIDGET(gtk_builder_get_object(gui, "soundcmd"));
  gtk_entry_set_text(GTK_ENTRY(option), config_get_str("soundcmd"));
  // Logging
  option=GTK_WIDGET(gtk_builder_get_object(gui, "enable_logging"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(option), config_get_bool("enable_logging"));
  GtkWidget* logpath=GTK_WIDGET(gtk_builder_get_object(gui, "logpath_channel"));
  gtk_entry_set_text(GTK_ENTRY(logpath), config_get_str("logpath_channel"));
  logpath=GTK_WIDGET(gtk_builder_get_object(gui, "logpath_pm"));
  gtk_entry_set_text(GTK_ENTRY(logpath), config_get_str("logpath_pm"));
  // Youtube
  option=GTK_WIDGET(gtk_builder_get_object(gui, "youtuberadio_cmd"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(option), config_get_bool("youtuberadio_cmd"));
  option=GTK_WIDGET(gtk_builder_get_object(gui, "youtubecmd"));
  gtk_entry_set_text(GTK_ENTRY(option), config_get_str("youtubecmd"));
}

void showsettings(GtkMenuItem* item, GtkBuilder* gui)
{
  settings_reset(gui);
  GtkWidget* w=GTK_WIDGET(gtk_builder_get_object(gui, "settings"));
  gtk_widget_show_all(w);
}

void savesettings(GtkButton* button, GtkBuilder* gui)
{
  // Sound
  GtkWidget* soundcmd=GTK_WIDGET(gtk_builder_get_object(gui, "soundcmd"));
  config_set("soundcmd", gtk_entry_get_text(GTK_ENTRY(soundcmd)));
  GtkWidget* soundradio_cmd=GTK_WIDGET(gtk_builder_get_object(gui, "soundradio_cmd"));
  config_set("soundradio_cmd", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(soundradio_cmd))?"True":"False");
  // Logging
  GtkWidget* logging=GTK_WIDGET(gtk_builder_get_object(gui, "enable_logging"));
  config_set("enable_logging", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(logging))?"True":"False");
  if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(logging))){logger_close_all();}
  GtkWidget* logpath=GTK_WIDGET(gtk_builder_get_object(gui, "logpath_channel"));
  config_set("logpath_channel", gtk_entry_get_text(GTK_ENTRY(logpath)));
  logpath=GTK_WIDGET(gtk_builder_get_object(gui, "logpath_pm"));
  config_set("logpath_pm", gtk_entry_get_text(GTK_ENTRY(logpath)));
  // Youtube
  GtkWidget* youtubecmd=GTK_WIDGET(gtk_builder_get_object(gui, "youtubecmd"));
  config_set("youtubecmd", gtk_entry_get_text(GTK_ENTRY(youtubecmd)));
  GtkWidget* youtuberadio_cmd=GTK_WIDGET(gtk_builder_get_object(gui, "youtuberadio_cmd"));
  config_set("youtuberadio_cmd", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(youtuberadio_cmd))?"True":"False");

  config_save();
  GtkWidget* settings=GTK_WIDGET(gtk_builder_get_object(gui, "settings"));
  gtk_widget_hide(settings);
}

void toggle_soundcmd(GtkToggleButton* button, GtkBuilder* gui)
{
  GtkWidget* field=GTK_WIDGET(gtk_builder_get_object(gui, "soundcmd"));
  gtk_widget_set_sensitive(field, gtk_toggle_button_get_active(button));
}

void toggle_logging(GtkToggleButton* button, GtkBuilder* gui)
{
  GtkWidget* field1=GTK_WIDGET(gtk_builder_get_object(gui, "logpath_channel"));
  GtkWidget* field2=GTK_WIDGET(gtk_builder_get_object(gui, "logpath_pm"));
  gtk_widget_set_sensitive(field1, gtk_toggle_button_get_active(button));
  gtk_widget_set_sensitive(field2, gtk_toggle_button_get_active(button));
}

void toggle_youtubecmd(GtkToggleButton* button, GtkBuilder* gui)
{
  GtkWidget* field=GTK_WIDGET(gtk_builder_get_object(gui, "youtubecmd"));
  gtk_widget_set_sensitive(field, gtk_toggle_button_get_active(button));
}
