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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gui.h"
#include "config.h"
#include "logging.h"
#include "compat.h"

extern void startsession(GtkButton* button, void* x);
GtkBuilder* gui;

char autoscroll_before(GtkAdjustment* scroll)
{
  // Figure out if we're at the bottom and should autoscroll with new content
  int upper=gtk_adjustment_get_upper(scroll);
  int size=gtk_adjustment_get_page_size(scroll);
  int value=gtk_adjustment_get_value(scroll);
  return (value+size+20>=upper);
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

void savechannel(GtkButton* button, void* x)
{
  int channelid;
  if(x==(void*)-1)
  {
    channelid=config_get_int("channelcount");
    config_set_int("channelcount", channelid+1);
  }else{
    channelid=(int)(intptr_t)x;
  }
  char buf[256];
  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(gui, "channelconfig")));
  const char* nick=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_nick")));
  sprintf(buf, "channel%i_nick", channelid);
  config_set(buf, nick);

  const char* channel=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_channel")));
  sprintf(buf, "channel%i_name", channelid);
  config_set(buf, channel);

  const char* chanpass=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_password")));
  sprintf(buf, "channel%i_password", channelid);
  config_set(buf, chanpass);

  const char* acc_user=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_username")));
  sprintf(buf, "channel%i_accuser", channelid);
  config_set(buf, acc_user);

  const char* acc_pass=gtk_entry_get_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_password")));
  sprintf(buf, "channel%i_accpass", channelid);
  config_set(buf, acc_pass);
  config_save();

  if(x==(void*)-1)
  {
    GtkWidget* startbox=GTK_WIDGET(gtk_builder_get_object(gui, "startbox"));
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    #ifdef GTK_STYLE_CLASS_LINKED
      GtkStyleContext* style=gtk_widget_get_style_context(box);
      gtk_style_context_add_class(style, GTK_STYLE_CLASS_LINKED);
    #endif
    GtkWidget* connectbutton=gtk_button_new_with_label(channel);
    g_signal_connect(connectbutton, "clicked", G_CALLBACK(startsession), (void*)(intptr_t)channelid);
    gtk_box_pack_start(GTK_BOX(box), connectbutton, 1, 1, 0);
    GtkWidget* cfgbutton=gtk_button_new_from_icon_name("gtk-preferences", GTK_ICON_SIZE_BUTTON);
    struct channelopts* opts=malloc(sizeof(struct channelopts));
    opts->channel_id=channelid;
    opts->save=1;
    g_signal_connect(cfgbutton, "clicked", G_CALLBACK(channeldialog), opts);
    gtk_box_pack_start(GTK_BOX(box), cfgbutton, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(startbox), box, 0, 0, 2);
    gtk_widget_show_all(box);
  }
}

char deletechannel_found;
int deletechannel_id;
void deletechannel_check(GtkWidget* widget, const char* name)
{
  if(!GTK_IS_CONTAINER(widget)){return;}
  GList* list=gtk_container_get_children(GTK_CONTAINER(widget));
  if(GTK_IS_BUTTON(list->data))
  {
    if(!strcmp(gtk_button_get_label(GTK_BUTTON(list->data)), name))
    {
      deletechannel_found=1;
      gtk_widget_destroy(widget);
    }
    else if(deletechannel_found)
    {
      // After the channel we deleted we have to increment the numbers (data for events) for subsequent buttons, first remove existing events
      g_signal_handlers_disconnect_matched(list->data, G_SIGNAL_MATCH_FUNC, 0, 0, 0, startsession, 0);
      g_signal_handlers_disconnect_matched(list->next->data, G_SIGNAL_MATCH_FUNC, 0, 0, 0, channeldialog, 0);
      // Bind new events
      g_signal_connect(list->data, "clicked", G_CALLBACK(startsession), (void*)(intptr_t)deletechannel_id);
      struct channelopts* opts=malloc(sizeof(struct channelopts));
      opts->channel_id=deletechannel_id;
      opts->save=1;
      g_signal_connect(list->next->data, "clicked", G_CALLBACK(channeldialog), opts);
      ++deletechannel_id;
    }
  }
  g_list_free(list);
}

void deletechannel(GtkButton* button, void* x)
{
  GtkContainer* startbox=GTK_CONTAINER(gtk_builder_get_object(gui, "startbox"));
  int channelid=(int)(intptr_t)x;
  char buf[256];
  sprintf(buf, "channel%i_name", channelid);
  // Delete the buttons from the startbox
  deletechannel_found=0;
  deletechannel_id=channelid;
  gtk_container_foreach(startbox, (GtkCallback)deletechannel_check, (void*)config_get_str(buf));
  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "channelconfig"));
  gtk_widget_hide(window);
  // Delete from the configuration
  char buf2[256];
  int channelcount=config_get_int("channelcount")-1;
  for(;channelid<channelcount; ++channelid)
  {
    #define move_channel_var(x) \
    sprintf(buf, "channel%i_"x, channelid); \
    sprintf(buf2, "channel%i_"x, channelid+1); \
    config_set(buf, config_get_str(buf2));
    move_channel_var("name");
    move_channel_var("nick");
    move_channel_var("password");
    move_channel_var("accuser");
    move_channel_var("accpass");
  }
  // Clear entries from the now unused channel ID
  sprintf(buf, "channel%i_name", channelcount);
  config_set(buf, "");
  sprintf(buf, "channel%i_nick", channelcount);
  config_set(buf, "");
  sprintf(buf, "channel%i_password", channelcount);
  config_set(buf, "");
  sprintf(buf, "channel%i_accuser", channelcount);
  config_set(buf, "");
  sprintf(buf, "channel%i_accpass", channelcount);
  config_set(buf, "");
  config_set_int("channelcount", channelcount);
  config_save();
}

void channeldialog(GtkButton* button, struct channelopts* opts)
{
  // If we're not going from an existing channel, clear the fields
  if(opts->channel_id==-1)
  {
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_nick")), "");
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_channel")), "");
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_password")), "");
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_username")), "");
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_password")), "");
  }else{ // But if we are, populate the fields
    char buf[256];
    sprintf(buf, "channel%i_nick", opts->channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_nick")), config_get_str(buf));

    sprintf(buf, "channel%i_name", opts->channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_channel")), config_get_str(buf));

    sprintf(buf, "channel%i_password", opts->channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "cc_password")), config_get_str(buf));

    sprintf(buf, "channel%i_accuser", opts->channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_username")), config_get_str(buf));

    sprintf(buf, "channel%i_accpass", opts->channel_id);
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(gui, "acc_password")), config_get_str(buf));
  }
  GObject* obj=gtk_builder_get_object(gui, "cc_savebutton");
  // Connect the save button either to startsession or savechannel depending on if we're adding/editing or quick-connecting
  g_signal_handlers_disconnect_matched(obj, G_SIGNAL_MATCH_FUNC, 0, 0, 0, savechannel, 0);
  g_signal_handlers_disconnect_matched(obj, G_SIGNAL_MATCH_FUNC, 0, 0, 0, startsession, 0);
  if(opts->save)
  {
    gtk_button_set_label(GTK_BUTTON(obj), "Save");
    g_signal_connect(obj, "clicked", G_CALLBACK(savechannel), (void*)(intptr_t)opts->channel_id);
  }else{
    gtk_button_set_label(GTK_BUTTON(obj), "Connect");
    g_signal_connect(obj, "clicked", G_CALLBACK(startsession), (void*)-1);
  }
  obj=gtk_builder_get_object(gui, "channelconfig");
  gtk_window_set_transient_for(GTK_WINDOW(obj), GTK_WINDOW(gtk_builder_get_object(gui, "startwindow")));
  gtk_widget_show_all(GTK_WIDGET(obj));

  obj=gtk_builder_get_object(gui, "cc_delete");
  if(opts->channel_id==-1)
  {
    gtk_widget_hide(GTK_WIDGET(obj));
  }else{
    // Connect signal for channel delete button
    g_signal_handlers_disconnect_matched(obj, G_SIGNAL_MATCH_FUNC, 0, 0, 0, deletechannel, 0);
    g_signal_connect(obj, "clicked", G_CALLBACK(deletechannel), (void*)(intptr_t)opts->channel_id);
  }
}
