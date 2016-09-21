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
#include <stdint.h>
#include <gtk/gtk.h>
#include "config.h"
#include "logging.h"
#include "compat.h"
#include "userlist.h"
#include "media.h"
#include "gui.h"

extern void startsession(GtkButton* button, void* x);
extern int tc_client_in[2];
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
  // Font
  GtkWidget* option=GTK_WIDGET(gtk_builder_get_object(gui, "fontsize"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(option), config_get_set("fontsize")?config_get_double("fontsize"):8);
  // Sound
  option=GTK_WIDGET(gtk_builder_get_object(gui, "soundradio_cmd"));
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
  // Cameras
  option=GTK_WIDGET(gtk_builder_get_object(gui, "camdownonjoin"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(option), config_get_bool("camdownonjoin"));
  option=GTK_WIDGET(gtk_builder_get_object(gui, "autoopencams"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(option), config_get_set("autoopencams")?config_get_bool("autoopencams"):1);
  // Misc/cookies
  option=GTK_WIDGET(gtk_builder_get_object(gui, "storecookiecheckbox"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(option), config_get_bool("storecookies"));
}

void showsettings(GtkMenuItem* item, GtkBuilder* gui)
{
  settings_reset(gui);
  GtkWidget* w=GTK_WIDGET(gtk_builder_get_object(gui, "settings"));
  gtk_widget_show_all(w);
}

void savesettings(GtkButton* button, GtkBuilder* gui)
{
  // Font
  GtkSpinButton* fontsize=GTK_SPIN_BUTTON(gtk_builder_get_object(gui, "fontsize"));
  config_set_double("fontsize", gtk_spin_button_get_value(fontsize));
  fontsize_set(gtk_spin_button_get_value(fontsize));
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
  // Cameras
  GtkWidget* option=GTK_WIDGET(gtk_builder_get_object(gui, "camdownonjoin"));
  config_set("camdownonjoin", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(option))?"True":"False");
  option=GTK_WIDGET(gtk_builder_get_object(gui, "autoopencams"));
  config_set("autoopencams", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(option))?"True":"False");
  // Misc/cookies
  option=GTK_WIDGET(gtk_builder_get_object(gui, "storecookiecheckbox"));
  config_set("storecookies", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(option))?"True":"False");

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
    GtkWidget* placeholder=GTK_WIDGET(gtk_builder_get_object(gui, "channel_placeholder"));
    if(placeholder){gtk_widget_destroy(placeholder);}
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

void pm_close(GtkButton* btn, GtkWidget* tab)
{
  gtk_widget_destroy(tab);
  struct user* user=user_find_by_tab(tab);
  if(!user){return;}
  user->pm_tab=0;
  user->pm_tablabel=0;
  user->pm_buffer=0;
  user->pm_scroll=0;
  user->pm_highlight=0;
}

void pm_open(const char* nick, char select, GtkAdjustment* scroll)
{
  struct user* user=finduser(nick);
  if(!user){return;}
  GtkNotebook* tabs=GTK_NOTEBOOK(gtk_builder_get_object(gui, "tabs"));
  if(user->pm_tab)
  {
    if(!select){return;}
    int num=gtk_notebook_page_num(tabs, user->pm_tab);
    gtk_notebook_set_current_page(tabs, num);
    return;
  }
  char bottom=autoscroll_before(scroll); // If PM tabs (with close buttons) are taller we need to make sure pushing down the chat field doesn't make it stop scrolling
  GtkWidget* textview=gtk_text_view_new();
  user->pm_tab=gtk_scrolled_window_new(0, 0);
  user->pm_buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
  user->pm_scroll=gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(user->pm_tab));
  user->pm_tablabel=gtk_label_new(nick);
  buffer_setup_colors(user->pm_buffer);
  gtk_container_add(GTK_CONTAINER(user->pm_tab), textview);
  GtkWidget* tabbox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#if GTK_MAJOR_VERSION<3 || (GTK_MAJOR_VERSION==3 && GTK_MINOR_VERSION<10)
  GtkWidget* closebtn=gtk_button_new_from_icon_name("gtk-close", GTK_ICON_SIZE_BUTTON);
#else
  GtkWidget* closebtn=gtk_button_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON);
#endif
  g_signal_connect(closebtn, "clicked", G_CALLBACK(pm_close), user->pm_tab);
  gtk_box_pack_start(GTK_BOX(tabbox), user->pm_tablabel, 1, 1, 0);
  gtk_box_pack_start(GTK_BOX(tabbox), closebtn, 0, 0, 0);
  int num=gtk_notebook_append_page(tabs, user->pm_tab, tabbox);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), 0);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), 0);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_CHAR);
  gtk_widget_show_all(user->pm_tab);
  gtk_widget_show_all(tabbox);
  if(select)
  {
    gtk_notebook_set_current_page(tabs, num);
  }
  if(bottom){autoscroll_after(scroll);}
}

void pm_highlight(const char* nick)
{
  if(!nick){return;}
  struct user* user=finduser(nick);
  if(!user || !user->pm_tablabel){return;}
  // Only highlight tabs we're not on
  GtkNotebook* tabs=GTK_NOTEBOOK(gtk_builder_get_object(gui, "tabs"));
  GtkWidget* page=gtk_notebook_get_nth_page(tabs, gtk_notebook_get_current_page(tabs));
  if(page==user->pm_tab){return;}
  char* markup=g_markup_printf_escaped("<span color=\"red\">%s</span>", user->nick);
  gtk_label_set_markup(GTK_LABEL(user->pm_tablabel), markup);
  g_free(markup);
  user->pm_highlight=1;
}

char pm_select(GtkNotebook* tabs, GtkWidget* tab, int num, void* x)
{
  if(num<1){return 0;} // Don't try to unhighlight Main
  // Reset highlighting
  GtkContainer* box=GTK_CONTAINER(gtk_notebook_get_tab_label(tabs, tab));
  GList* list=gtk_container_get_children(box);
  GtkLabel* label=list->data; // Should be the first item
  g_list_free(list);
  const char* text=gtk_label_get_text(label);
  gtk_label_set_text(label, text);
  struct user* user=user_find_by_tab(tab);
  if(user){user->pm_highlight=0;}
  return 0;
}

void buffer_setup_colors(GtkTextBuffer* buffer)
{
  #define colormap(code, color) gtk_text_buffer_create_tag(buffer, code, "foreground", color, (char*)0)
  colormap("[31", "#821615");
  colormap("[31;1", "#c53332");
  colormap("[33", "#a08f23");
  //colormap("[33", "#a78901");
  colormap("[33;1", "#919104");
  colormap("[32;1", "#7bb224");
  //colormap("[32;1", "#7db257");
  colormap("[32", "#487d21");
  colormap("[36", "#00a990");
  colormap("[34;1", "#32a5d9");
  //colormap("[34;1", "#1d82eb");
  colormap("[34", "#1965b6");
  colormap("[35", "#5c1a7a");
  colormap("[35;1", "#9d5bb5");
  //colormap("[35;1", "#c356a3");
  //colormap("[35;1", "#b9807f");
  colormap("timestamp", "#808080");
  gtk_text_buffer_create_tag(buffer, "nickname", "weight", PANGO_WEIGHT_BOLD, "weight-set", TRUE, (char*)0);
  // Set size if it's set in config
  if(config_get_set("fontsize"))
  {
    gtk_text_buffer_create_tag(buffer, "size", "size-points", config_get_double("fontsize"), "size-set", TRUE, (char*)0);
  }else{
    gtk_text_buffer_create_tag(buffer, "size", "size-set", FALSE, (char*)0);
  }
}

void buffer_updatesize(GtkTextBuffer* buffer)
{
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  gtk_text_buffer_remove_tag_by_name(buffer, "size", &start, &end);
  gtk_text_buffer_apply_tag_by_name(buffer, "size", &start, &end);
}

void fontsize_set(double size)
{
  GtkWidget* chatview=GTK_WIDGET(gtk_builder_get_object(gui, "chatview"));
  GtkTextBuffer* buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(chatview));
  GtkTextTagTable* table=gtk_text_buffer_get_tag_table(buffer);
  GtkTextTag* tag=gtk_text_tag_table_lookup(table, "size");
  g_object_set(tag, "size-points", size, "size-set", TRUE, (char*)0);
  buffer_updatesize(buffer);
  unsigned int i;
  for(i=0; i<usercount; ++i)
  {
    if(!userlist[i].pm_buffer){continue;}
    table=gtk_text_buffer_get_tag_table(userlist[i].pm_buffer);
    tag=gtk_text_tag_table_lookup(table, "size");
    g_object_set(tag, "size-points", size, "size-set", TRUE, (char*)0);
    buffer_updatesize(userlist[i].pm_buffer);
  }
}

const char* menu_context_cam=0;
gboolean gui_show_cam_menu(GtkWidget* widget, GdkEventButton* event, const char* id)
{
  if(event->button!=3){return 0;} // Only act on right-click
  struct camera* cam=camera_find(id);
  free((void*)menu_context_cam);
  menu_context_cam=strdup(cam->id);
  GtkMenu* menu=GTK_MENU(gtk_builder_get_object(gui, "cam_menu"));
  gtk_menu_popup(menu, 0, 0, 0, 0, event->button, event->time);
  return 1;
}

void gui_show_camcolors(GtkMenuItem* menuitem, void* x)
{
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(gui, "cam_colors")));
  GtkAdjustment* adjustment=GTK_ADJUSTMENT(gtk_builder_get_object(gui, "camcolors_min_brightness"));
  gtk_adjustment_set_value(adjustment, cam->postproc.min_brightness);
  adjustment=GTK_ADJUSTMENT(gtk_builder_get_object(gui, "camcolors_max_brightness"));
  gtk_adjustment_set_value(adjustment, cam->postproc.max_brightness);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(gui, "camcolors_auto")), cam->postproc.autoadjust);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(gui, "camcolors_flip_horizontal")), cam->postproc.flip_horizontal);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(gui, "camcolors_flip_vertical")), cam->postproc.flip_vertical);
}

void camcolors_adjust_min(GtkAdjustment* adjustment, void* x)
{
  GtkAdjustment* other=GTK_ADJUSTMENT(gtk_builder_get_object(gui, "camcolors_max_brightness"));
  double value=gtk_adjustment_get_value(adjustment);
  if(value>gtk_adjustment_get_value(other))
  {
    gtk_adjustment_set_value(other, value);
  }
  if(!menu_context_cam){return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  cam->postproc.min_brightness=value;
}

void camcolors_adjust_max(GtkAdjustment* adjustment, void* x)
{
  GtkAdjustment* other=GTK_ADJUSTMENT(gtk_builder_get_object(gui, "camcolors_min_brightness"));
  double value=gtk_adjustment_get_value(adjustment);
  if(value<gtk_adjustment_get_value(other))
  {
    gtk_adjustment_set_value(other, value);
  }
  if(!menu_context_cam){return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  cam->postproc.max_brightness=value;
}

void camcolors_toggle_auto(GtkToggleButton* button, void* x)
{
  if(!menu_context_cam){return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  cam->postproc.autoadjust=gtk_toggle_button_get_active(button);
}

void camcolors_toggle_flip(GtkToggleButton* button, void* vertical)
{
  if(!menu_context_cam){return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  char v=gtk_toggle_button_get_active(button);
  if(vertical)
  {
    cam->postproc.flip_vertical=v;
  }else{
    cam->postproc.flip_horizontal=v;
  }
}

void gui_hide_cam(GtkMenuItem* menuitem, void* x)
{
  if(!menu_context_cam){return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  dprintf(tc_client_in[1], "/closecam %s\n", cam->nick);
  camera_remove(menu_context_cam, 0);
}
