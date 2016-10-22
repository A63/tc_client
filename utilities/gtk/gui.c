/*
    tc_client-gtk, a graphical user interface for tc_client
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
#include <stdint.h>
#include <gtk/gtk.h>
#include "config.h"
#include "logging.h"
#include "compat.h"
#include "userlist.h"
#include "media.h"
#include "main.h"
#include "gui.h"

extern void startsession(GtkButton* button, void* x);
extern int tc_client_in[2];
GtkBuilder* gui;
GtkWidget* gui_greenscreen_preview_img=0;
unsigned int gui_greenscreen_preview_event=0;
extern gboolean gui_greenscreen_preview(void* x);
GdkCursor* gui_cursor_text;
GdkCursor* gui_cursor_link;
struct chatview* mainchat;

void settings_reset(GtkBuilder* gui)
{
  // Text
  GtkWidget* option=GTK_WIDGET(gtk_builder_get_object(gui, "fontsize"));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(option), config_get_set("fontsize")?config_get_double("fontsize"):8);
  // Find the matching combo box item
  option=GTK_WIDGET(gtk_builder_get_object(gui, "linewrap"));
  GtkTreeModel* model=gtk_combo_box_get_model(GTK_COMBO_BOX(option));
  GtkTreeIter iter;
  if(config_get_set("linewrap_mode"))
  {
    char next=gtk_tree_model_get_iter_first(model, &iter);
    while(next)
    {
      GtkWrapMode mode;
      gtk_tree_model_get(model, &iter, 1, &mode, -1);
      if(mode==config_get_int("linewrap_mode"))
      {
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(option), &iter);
        break;
      }
      next=gtk_tree_model_iter_next(model, &iter);
    }
  }
  else if(gtk_tree_model_get_iter_first(model, &iter))
  {
    gtk_combo_box_set_active_iter(GTK_COMBO_BOX(option), &iter);
  }
  option=GTK_WIDGET(gtk_builder_get_object(gui, "bluelinks"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(option), config_get_bool("blue_links"));
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
  // Text
  GtkSpinButton* fontsize=GTK_SPIN_BUTTON(gtk_builder_get_object(gui, "fontsize"));
  config_set_double("fontsize", gtk_spin_button_get_value(fontsize));
  fontsize_set(gtk_spin_button_get_value(fontsize));
  GtkWidget* option=GTK_WIDGET(gtk_builder_get_object(gui, "linewrap"));
  GtkTreeModel* model=gtk_combo_box_get_model(GTK_COMBO_BOX(option));
  GtkTreeIter iter;
  if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(option), &iter))
  {
    GtkWrapMode mode;
    gtk_tree_model_get(model, &iter, 1, &mode, -1);
    config_set_int("linewrap_mode", mode);
    GtkTextView* chatview=GTK_TEXT_VIEW(gtk_builder_get_object(gui, "chatview"));
    gtk_text_view_set_wrap_mode(chatview, mode);
    unsigned int i;
    for(i=0; i<usercount; ++i)
    {
      if(!userlist[i].pm_chatview){continue;}
      gtk_text_view_set_wrap_mode(userlist[i].pm_chatview->textview, mode);
    }
  }
  option=GTK_WIDGET(gtk_builder_get_object(gui, "bluelinks"));
  config_set("blue_links", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(option))?"True":"False");
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
  option=GTK_WIDGET(gtk_builder_get_object(gui, "camdownonjoin"));
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
  free(user->pm_chatview);
  user->pm_tab=0;
  user->pm_tablabel=0;
  user->pm_chatview=0;
  user->pm_highlight=0;
}

void pm_open(const char* nick, char select)
{
  struct user* user=finduser(nick);
  if(!user){return;}
  GtkNotebook* tabs=GTK_NOTEBOOK(gtk_builder_get_object(gui, "tabs"));
  if(!user->pm_tab)
  {
    user->pm_chatview=chatview_new(0);
    user->pm_tab=user->pm_chatview->scrolledwindow;
    user->pm_tablabel=gtk_label_new(nick);
    GtkWidget* tabbox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    #if GTK_MAJOR_VERSION<3 || (GTK_MAJOR_VERSION==3 && GTK_MINOR_VERSION<10)
      GtkWidget* closebtn=gtk_button_new_from_icon_name("gtk-close", GTK_ICON_SIZE_BUTTON);
    #else
      GtkWidget* closebtn=gtk_button_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON);
    #endif
    g_signal_connect(closebtn, "clicked", G_CALLBACK(pm_close), user->pm_tab);
    gtk_box_pack_start(GTK_BOX(tabbox), user->pm_tablabel, 1, 1, 0);
    gtk_box_pack_start(GTK_BOX(tabbox), closebtn, 0, 0, 0);
    gtk_notebook_append_page(tabs, user->pm_tab, tabbox);
    gtk_widget_show_all(user->pm_tab);
    gtk_widget_show_all(tabbox);
  }
  if(select)
  {
    int num=gtk_notebook_page_num(tabs, user->pm_tab);
    gtk_notebook_set_current_page(tabs, num);
  }
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
    if(!userlist[i].pm_chatview){continue;}
    buffer=gtk_text_view_get_buffer(userlist[i].pm_chatview->textview);
    table=gtk_text_buffer_get_tag_table(buffer);
    tag=gtk_text_tag_table_lookup(table, "size");
    g_object_set(tag, "size-points", size, "size-set", TRUE, (char*)0);
    buffer_updatesize(buffer);
  }
}

const char* menu_context_cam=0;
gboolean gui_show_cam_menu(GtkWidget* widget, GdkEventButton* event, const char* id)
{
  struct camera* cam=camera_find(id);
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(gui, "greenscreen_eyedropper"))))
  {
    gui_set_greenscreen_color_cam(event->x, event->y, cam);
    return 1;
  }
  if(event->button!=3){return 0;} // Only act on right-click
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
  // Set brightness controls
  GtkAdjustment* adjustment=GTK_ADJUSTMENT(gtk_builder_get_object(gui, "camcolors_min_brightness"));
  gtk_adjustment_set_value(adjustment, cam->postproc.min_brightness);
  adjustment=GTK_ADJUSTMENT(gtk_builder_get_object(gui, "camcolors_max_brightness"));
  gtk_adjustment_set_value(adjustment, cam->postproc.max_brightness);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(gui, "camcolors_auto")), cam->postproc.autoadjust);
  // Flip controls
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(gui, "camcolors_flip_horizontal")), cam->postproc.flip_horizontal);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(gui, "camcolors_flip_vertical")), cam->postproc.flip_vertical);
  // Greenscreen controls
  // Set the appropriate child widget for the greenscreenbutton and start the preview if applicable
  GtkWidget* button=GTK_WIDGET(gtk_builder_get_object(gui, "greenscreenbutton"));
  GtkWidget* child=gtk_bin_get_child(GTK_BIN(button));
  if(child){gtk_widget_destroy(child);}
  if(cam->postproc.greenscreen)
  {
    gui_greenscreen_preview_img=gtk_image_new();
    gtk_container_add(GTK_CONTAINER(button), gui_greenscreen_preview_img);
    if(!gui_greenscreen_preview_event)
    {
      gui_greenscreen_preview_event=g_timeout_add(100, gui_greenscreen_preview, 0);
    }
  }else{
    gui_greenscreen_preview_img=0;
    gtk_container_add(GTK_CONTAINER(button), gtk_label_new("Choose a greenscreen background"));
  }
  GdkRGBA color={.red=(double)cam->postproc.greenscreen_color[0]/255,
                  .green=(double)cam->postproc.greenscreen_color[1]/255,
                  .blue=(double)cam->postproc.greenscreen_color[2]/255,
                  .alpha=1.0};
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(gtk_builder_get_object(gui, "greenscreen_colorpicker")), &color);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_builder_get_object(gui, "greenscreen_tolerance_h")), cam->postproc.greenscreen_tolerance[0]);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_builder_get_object(gui, "greenscreen_tolerance_s")), cam->postproc.greenscreen_tolerance[1]);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_builder_get_object(gui, "greenscreen_tolerance_v")), cam->postproc.greenscreen_tolerance[2]);

  gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(gui, "cam_colors")));
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

gboolean gui_greenscreen_preview(void* x)
{
  struct camera* cam;
  if(!gui_greenscreen_preview_img || !menu_context_cam ||
     !(cam=camera_find(menu_context_cam)) || !cam->postproc.greenscreen)
  {
    gui_greenscreen_preview_event=0;
    return G_SOURCE_REMOVE;
  }
  GdkPixbuf* oldpixbuf=gtk_image_get_pixbuf(GTK_IMAGE(gui_greenscreen_preview_img));
  GdkPixbuf* gdkframe=scaled_gdk_pixbuf_from_cam(cam->postproc.greenscreen, cam->postproc.greenscreen_size.width, cam->postproc.greenscreen_size.height, 160, 120);
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui_greenscreen_preview_img), gdkframe);
  if(oldpixbuf){g_object_unref(oldpixbuf);}
  return G_SOURCE_CONTINUE;
}

void gui_set_greenscreen_img_accept(CAM* img)
{
  if(!menu_context_cam){cam_close(img); return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){cam_close(img); return;}
  if(cam->postproc.greenscreen){cam_close(cam->postproc.greenscreen);}
  cam->postproc.greenscreen=img;
  cam->postproc.greenscreen_size.width=340;
  cam->postproc.greenscreen_size.height=240;
  cam_resolution(img, &cam->postproc.greenscreen_size.width, &cam->postproc.greenscreen_size.height);
  GtkWidget* button=GTK_WIDGET(gtk_builder_get_object(gui, "greenscreenbutton"));
  GtkWidget* child=gtk_bin_get_child(GTK_BIN(button));
  if(child){gtk_widget_destroy(child);}
  // Add a GtkImage to the button and start a g_timeout to show a preview on it
  gui_greenscreen_preview_img=gtk_image_new();
  gtk_container_add(GTK_CONTAINER(button), gui_greenscreen_preview_img);
  gtk_widget_show(gui_greenscreen_preview_img);
  if(!gui_greenscreen_preview_event)
  {
    gui_greenscreen_preview_event=g_timeout_add(100, gui_greenscreen_preview, 0);
  }
}

void gui_set_greenscreen_img_cancel(void)
{
  if(gui_greenscreen_preview_event)
  {
    g_source_remove(gui_greenscreen_preview_event);
    gui_greenscreen_preview_event=0;
  }
  GtkWidget* button=GTK_WIDGET(gtk_builder_get_object(gui, "greenscreenbutton"));
  GtkWidget* child=gtk_bin_get_child(GTK_BIN(button));
  if(child){gtk_widget_destroy(child);}
  gui_greenscreen_preview_img=0;
  GtkWidget* label=gtk_label_new("Choose a greenscreen background");
  gtk_container_add(GTK_CONTAINER(button), label);
  gtk_widget_show(label);
  // Remove from camera
  if(!menu_context_cam){return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  if(cam->postproc.greenscreen)
  {
    cam_close(cam->postproc.greenscreen);
    cam->postproc.greenscreen=0;
  }
}

void gui_set_greenscreen_img(GtkButton* button, void* x)
{
  if(!menu_context_cam){return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  camselect_open(gui_set_greenscreen_img_accept, gui_set_greenscreen_img_cancel);
}

void gui_set_greenscreen_color(GtkColorButton* button, void* x)
{
  if(!menu_context_cam){return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  GdkRGBA color;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
  // Convert to HSV but also keep the RGB for later
  cam->postproc.greenscreen_color[0]=(color.red*255);
  cam->postproc.greenscreen_color[1]=(color.green*255);
  cam->postproc.greenscreen_color[2]=(color.blue*255);
  rgb_to_hsv(cam->postproc.greenscreen_color[0], cam->postproc.greenscreen_color[1], cam->postproc.greenscreen_color[2], &cam->postproc.greenscreen_hsv[0], &cam->postproc.greenscreen_hsv[1], &cam->postproc.greenscreen_hsv[2]);
}

void gui_set_greenscreen_color_cam(unsigned int x, unsigned int y, struct camera* cam)
{
  GdkPixbuf* pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(cam->cam));
  unsigned int rowstride=gdk_pixbuf_get_rowstride(pixbuf);
  const guint8* pixels=gdk_pixbuf_read_pixels(pixbuf);
  GdkRGBA color={
    .red=(double)pixels[y*rowstride+x*3]/255,
    .green=(double)pixels[y*rowstride+x*3+1]/255,
    .blue=(double)pixels[y*rowstride+x*3+2]/255,
    .alpha=1.0
  };
  GtkColorChooser* button=GTK_COLOR_CHOOSER(gtk_builder_get_object(gui, "greenscreen_colorpicker"));
  gtk_color_chooser_set_rgba(button, &color);
  gui_set_greenscreen_color(GTK_COLOR_BUTTON(button), 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(gui, "greenscreen_eyedropper")), 0);
}

void gui_set_greenscreen_tolerance(GtkAdjustment* adjustment, void* x)
{
  if(!menu_context_cam){return;}
  struct camera* cam=camera_find(menu_context_cam);
  if(!cam){return;}
  cam->postproc.greenscreen_tolerance[(intptr_t)x]=gtk_adjustment_get_value(adjustment);
}

void gui_insert_link(GtkTextBuffer* buffer, GtkTextIter* iter, const char* url, int length)
{
  int startnum=gtk_text_iter_get_offset(iter);
  gtk_text_buffer_insert(buffer, iter, url, length);
  // Make it look like a link and store the URL for later
  GtkTextTag* tag;
  if(config_get_bool("blue_links"))
  {
    tag=gtk_text_buffer_create_tag(buffer, 0, "underline", PANGO_UNDERLINE_SINGLE, "foreground", "#0000ff", (char*)0);
  }else{
    tag=gtk_text_buffer_create_tag(buffer, 0, "underline", PANGO_UNDERLINE_SINGLE, (char*)0);
  }
  g_object_set_data(G_OBJECT(tag), "url", strndup(url, length));
  GtkTextIter start;
  gtk_text_buffer_get_iter_at_offset(buffer, &start, startnum);
  gtk_text_buffer_apply_tag(buffer, tag, &start, iter);
}

const char* gui_find_link(GtkTextView* textview, int xpos, int ypos)
{
  int x;
  int y;
  gtk_text_view_window_to_buffer_coords(textview, GTK_TEXT_WINDOW_TEXT, xpos, ypos, &x, &y);
  GtkTextIter iter;
  gtk_text_view_get_iter_at_location(textview, &iter, x, y);
  const char* url=0;
  GSList* tags=gtk_text_iter_get_tags(&iter);
  GSList* i;
  for(i=tags; i; i=g_slist_next(i))
  {
    url=g_object_get_data(G_OBJECT(i->data), "url");
    if(url){break;}
  }
  g_slist_free(tags);
  return url;
}

gboolean gui_click_link(GtkTextView* textview, GdkEventButton* event, void* data)
{
  const char* url=gui_find_link(textview, event->x, event->y);
  if(url && event->button!=3) // Not right-click, just open it
  {
    gtk_show_uri(gtk_widget_get_screen(GTK_WIDGET(textview)), url, gtk_get_current_event_time(), 0);
    return 1;
  }
  return 0;
}

static const char* gui_link_menu_url=0;
gboolean gui_rightclick_link(GtkTextView* textview, GdkEventButton* event, void* data)
{
  const char* url=gui_find_link(textview, event->x, event->y);
  if(url && event->button==3)
  {
    // Show a menu with options to either open the link or copy it
    gui_link_menu_url=url;
    GtkMenu* menu=GTK_MENU(gtk_builder_get_object(gui, "link_menu"));
    gtk_menu_popup(menu, 0, 0, 0, 0, event->button, event->time);
    return 1;
  }
  return 0;
}

gboolean gui_hover_link(GtkTextView* textview, GdkEventMotion* event, void* data)
{
  const char* url=gui_find_link(textview, event->x, event->y);
  GdkWindow* window=gtk_text_view_get_window(textview, GTK_TEXT_WINDOW_TEXT);
  GdkCursor* cursor;
  if(url)
  {
    cursor=gui_cursor_link;
  }else{
    cursor=gui_cursor_text;
  }
  gdk_window_set_cursor(window, cursor);
  return 0;
}

void gui_link_menu_open(GtkWidget* menuitem, void* x)
{
  if(!gui_link_menu_url){return;}
  gtk_show_uri(gtk_widget_get_screen(GTK_WIDGET(menuitem)), gui_link_menu_url, gtk_get_current_event_time(), 0);
}

void gui_link_menu_copy(GtkWidget* menuitem, void* x)
{
  if(!gui_link_menu_url){return;}
  GtkClipboard* clipboard=gtk_clipboard_get_for_display(gtk_widget_get_display(menuitem), GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text(clipboard, gui_link_menu_url, -1);
}

void chatview_scrolled(GtkAdjustment* adj, struct chatview* cv)
{
  double value=gtk_adjustment_get_value(adj);
  double upper=gtk_adjustment_get_upper(adj);
  double pagesize=gtk_adjustment_get_page_size(adj);
  char bottom=(value+pagesize>=upper);
  // To keep smooth scrolling from messing up autoscroll,
  // don't change state if we're already marked as being at the
  // bottom and scrolling down.
  if(bottom || value<cv->oldscrollposition)
  {
    cv->atbottom=bottom;
    cv->oldscrollposition=value;
  }
}

struct chatview* chatview_new(GtkTextView* existing_textview)
{
  struct chatview* this=malloc(sizeof(struct chatview));
  if(existing_textview)
  {
    this->textview=existing_textview;
    this->scrolledwindow=gtk_widget_get_parent(GTK_WIDGET(this->textview));
  }else{
    this->textview=GTK_TEXT_VIEW(gtk_text_view_new());
    this->scrolledwindow=gtk_scrolled_window_new(0, 0);
    gtk_container_add(GTK_CONTAINER(this->scrolledwindow), GTK_WIDGET(this->textview));
  }
  this->atbottom=1;
  gtk_text_view_set_editable(this->textview, 0);
  gtk_text_view_set_cursor_visible(this->textview, 0);
  if(config_get_set("linewrap_mode"))
  {
    gtk_text_view_set_wrap_mode(this->textview, config_get_int("linewrap_mode"));
  }else{
    gtk_text_view_set_wrap_mode(this->textview, GTK_WRAP_WORD_CHAR);
  }
  GtkAdjustment* scroll=gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(this->scrolledwindow));
  g_signal_connect(this->textview, "button-release-event", G_CALLBACK(gui_click_link), 0);
  g_signal_connect(this->textview, "button-press-event", G_CALLBACK(gui_rightclick_link), 0);
  g_signal_connect(this->textview, "motion-notify-event", G_CALLBACK(gui_hover_link), 0);
  g_signal_connect(scroll, "value-changed", G_CALLBACK(chatview_scrolled), this);

  // Set up the buffer
  GtkTextBuffer* buffer=gtk_text_view_get_buffer(this->textview);
  gtk_text_buffer_create_tag(buffer, "timestamp", "foreground", "#808080", (char*)0);
  gtk_text_buffer_create_tag(buffer, "nickname", "weight", PANGO_WEIGHT_BOLD, "weight-set", TRUE, (char*)0);
  // Set size if it's set in config
  if(config_get_set("fontsize"))
  {
    gtk_text_buffer_create_tag(buffer, "size", "size-points", config_get_double("fontsize"), "size-set", TRUE, (char*)0);
  }else{
    gtk_text_buffer_create_tag(buffer, "size", "size-set", FALSE, (char*)0);
  }
  // And en 'end' mark for scrolling
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buffer, &end);
  gtk_text_buffer_create_mark(buffer, "end", &end, 0);

  return this;
}

void chatview_autoscroll(struct chatview* cv)
{
  if(!cv->atbottom){return;}
  GtkTextBuffer* buffer=gtk_text_view_get_buffer(cv->textview);
  GtkTextMark* mark=gtk_text_buffer_get_mark(buffer, "end");
  gtk_text_view_scroll_to_mark(cv->textview, mark, 0, 0, 0, 0);
}

void togglecam_cancel(void)
{
  GtkCheckMenuItem* item=GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_camera"));
  gtk_check_menu_item_set_active(item, 0);
}

void stopbroadcasting(GtkMenuItem* x, void* y)
{
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_camera")), 0);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_builder_get_object(gui, "menuitem_broadcast_mic")), 0);
}

#ifdef HAVE_PULSEAUDIO
gboolean mic_pushtotalk(GtkWidget* button, GdkEvent* event, void* pushed)
{
  pushtotalk_pushed=!!pushed;
  return 0;
}
#endif

gboolean handleresize(GtkWidget* widget, GdkEventConfigure* event, void* data)
{
  if(event->width!=gtk_widget_get_allocated_width(cambox))
  {
    updatescaling(event->width, 0, 0);
  }
  // Fix scrolling
  chatview_autoscroll(mainchat);
  unsigned int i;
  for(i=0; i<usercount; ++i)
  {
    if(userlist[i].pm_chatview)
    {
      chatview_autoscroll(userlist[i].pm_chatview);
    }
  }
  return 0;
}

void handleresizepane(GObject* obj, GParamSpec* spec, void* data)
{
  updatescaling(0, gtk_paned_get_position(GTK_PANED(obj)), 0);
  // Fix scrolling
  chatview_autoscroll(mainchat);
  unsigned int i;
  for(i=0; i<usercount; ++i)
  {
    if(userlist[i].pm_chatview)
    {
      chatview_autoscroll(userlist[i].pm_chatview);
    }
  }
}

static struct channelopts cc_connect={-1,0};
static struct channelopts cc_add={-1,1};
void gui_init(char frombuild)
{
  if(frombuild)
  {
    gui=gtk_builder_new_from_file("gtkgui.glade");
  }else{
    gui=gtk_builder_new_from_file(PREFIX "/share/tc_client/gtkgui.glade");
  }
  gtk_builder_connect_signals(gui, 0);

  unsigned int i;
  GtkWidget* item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_broadcast_camera"));
  g_signal_connect(item, "toggled", G_CALLBACK(togglecam), 0);
  item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_broadcast_mic"));
  #ifdef HAVE_PULSEAUDIO
  g_signal_connect(item, "toggled", G_CALLBACK(togglemic), 0);
  item=GTK_WIDGET(gtk_builder_get_object(gui, "pushtotalk"));
  g_signal_connect(item, "button-press-event", G_CALLBACK(mic_pushtotalk), (void*)1);
  g_signal_connect(item, "button-release-event", G_CALLBACK(mic_pushtotalk), 0);
  #else
  gtk_widget_destroy(item);
  #endif
  item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_broadcast_stop"));
  g_signal_connect(item, "activate", G_CALLBACK(stopbroadcasting), 0);
  // Set up cam selection and preview
  campreview.cam=GTK_WIDGET(gtk_builder_get_object(gui, "camselect_preview"));
  GtkComboBox* combo=GTK_COMBO_BOX(gtk_builder_get_object(gui, "camselect_combo"));
  g_signal_connect(combo, "changed", G_CALLBACK(camselect_change), 0);
  // Signals for cancelling
  item=GTK_WIDGET(gtk_builder_get_object(gui, "camselection"));
  g_signal_connect(item, "delete-event", G_CALLBACK(camselect_cancel), 0);
  item=GTK_WIDGET(gtk_builder_get_object(gui, "camselect_cancel"));
  g_signal_connect(item, "clicked", G_CALLBACK(camselect_cancel), 0);
  // Signals for switching from preview to streaming
  item=GTK_WIDGET(gtk_builder_get_object(gui, "camselect_ok"));
  g_signal_connect(item, "clicked", G_CALLBACK(camselect_accept), 0);
  // Enable the "img" camera
  cam_img_filepicker=camselect_file;
  // Populate list of cams
  unsigned int count;
  char** cams=cam_list(&count);
  GtkListStore* list=gtk_list_store_new(1, G_TYPE_STRING);
  for(i=0; i<count; ++i)
  {
    gtk_list_store_insert_with_values(list, 0, -1, 0, cams[i], -1);
    free(cams[i]);
  }
  free(cams);
  gtk_combo_box_set_model(combo, GTK_TREE_MODEL(list));
  g_object_unref(list);
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), gtk_cell_renderer_text_new(), 1);

  item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_options_settings"));
  g_signal_connect(item, "activate", G_CALLBACK(showsettings), gui);
  item=GTK_WIDGET(gtk_builder_get_object(gui, "menuitem_options_settings2"));
  g_signal_connect(item, "activate", G_CALLBACK(showsettings), gui);

  cambox=GTK_WIDGET(gtk_builder_get_object(gui, "cambox"));
  userlistwidget=GTK_WIDGET(gtk_builder_get_object(gui, "userlistbox"));
  GtkWidget* chatview=GTK_WIDGET(gtk_builder_get_object(gui, "chatview"));
  mainchat=chatview_new(GTK_TEXT_VIEW(chatview));
  GdkDisplay* display=gtk_widget_get_display(chatview);
  gui_cursor_text=gdk_cursor_new_from_name(display, "text");
  gui_cursor_link=gdk_cursor_new_from_name(display, "pointer");

  GtkWidget* panes=GTK_WIDGET(gtk_builder_get_object(gui, "vpaned"));
  g_signal_connect(panes, "notify::position", G_CALLBACK(handleresizepane), 0);
  gtk_paned_set_wide_handle(GTK_PANED(panes), 1);
  panes=GTK_WIDGET(gtk_builder_get_object(gui, "chatnick"));
  gtk_paned_set_wide_handle(GTK_PANED(panes), 1);

  GtkWidget* inputfield=GTK_WIDGET(gtk_builder_get_object(gui, "inputfield"));
  g_signal_connect(inputfield, "activate", G_CALLBACK(sendmessage), 0);
  g_signal_connect(inputfield, "key-press-event", G_CALLBACK(inputkeys), 0);

  // Sound
  GtkWidget* option=GTK_WIDGET(gtk_builder_get_object(gui, "soundradio_cmd"));
  g_signal_connect(option, "toggled", G_CALLBACK(toggle_soundcmd), gui);
  // Logging
  option=GTK_WIDGET(gtk_builder_get_object(gui, "enable_logging"));
  g_signal_connect(option, "toggled", G_CALLBACK(toggle_logging), gui);
  option=GTK_WIDGET(gtk_builder_get_object(gui, "save_settings"));
  g_signal_connect(option, "clicked", G_CALLBACK(savesettings), gui);
  // Youtube
  option=GTK_WIDGET(gtk_builder_get_object(gui, "youtuberadio_cmd"));
  g_signal_connect(option, "toggled", G_CALLBACK(toggle_youtubecmd), gui);
  // Misc
  option=GTK_WIDGET(gtk_builder_get_object(gui, "camdownonjoin"));

  GtkWidget* window=GTK_WIDGET(gtk_builder_get_object(gui, "main"));
  g_signal_connect(window, "configure-event", G_CALLBACK(handleresize), 0);

  // Start window and channel password window signals
  GtkWidget* button=GTK_WIDGET(gtk_builder_get_object(gui, "channelpasswordbutton"));
  g_signal_connect(button, "clicked", G_CALLBACK(startsession), (void*)-1);
  button=GTK_WIDGET(gtk_builder_get_object(gui, "channelpassword"));
  g_signal_connect(button, "activate", G_CALLBACK(startsession), (void*)-1);
  GtkWidget* startwindow=GTK_WIDGET(gtk_builder_get_object(gui, "startwindow"));
  // Connect signal for quick connect
  item=GTK_WIDGET(gtk_builder_get_object(gui, "start_menu_connect"));
  g_signal_connect(item, "activate", G_CALLBACK(channeldialog), &cc_connect);
  // Connect signal for the add option
  item=GTK_WIDGET(gtk_builder_get_object(gui, "start_menu_add"));
  g_signal_connect(item, "activate", G_CALLBACK(channeldialog), &cc_add);
  // Connect signal for tab changing (to un-highlight)
  item=GTK_WIDGET(gtk_builder_get_object(gui, "tabs"));
  g_signal_connect(item, "switch-page", G_CALLBACK(pm_select), 0);
  // Connect signal for captcha
  g_signal_connect(gtk_builder_get_object(gui, "captcha_done"), "clicked", G_CALLBACK(captcha_done), 0);
  // Connect signals for link menus
  g_signal_connect(gtk_builder_get_object(gui, "link_menu_open"), "activate", G_CALLBACK(gui_link_menu_open), 0);
  g_signal_connect(gtk_builder_get_object(gui, "link_menu_copy"), "activate", G_CALLBACK(gui_link_menu_copy), 0);
  // Connect signals for camera postprocessing
  g_signal_connect(gtk_builder_get_object(gui, "cam_menu_colors"), "activate", G_CALLBACK(gui_show_camcolors), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_min_brightness"), "value-changed", G_CALLBACK(camcolors_adjust_min), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_max_brightness"), "value-changed", G_CALLBACK(camcolors_adjust_max), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_auto"), "toggled", G_CALLBACK(camcolors_toggle_auto), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_flip_horizontal"), "toggled", G_CALLBACK(camcolors_toggle_flip), 0);
  g_signal_connect(gtk_builder_get_object(gui, "camcolors_flip_vertical"), "toggled", G_CALLBACK(camcolors_toggle_flip), (void*)1);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreenbutton"), "clicked", G_CALLBACK(gui_set_greenscreen_img), 0);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreen_colorpicker"), "color-set", G_CALLBACK(gui_set_greenscreen_color), 0);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreen_tolerance_h"), "value-changed", G_CALLBACK(gui_set_greenscreen_tolerance), 0);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreen_tolerance_s"), "value-changed", G_CALLBACK(gui_set_greenscreen_tolerance), (void*)1);
  g_signal_connect(gtk_builder_get_object(gui, "greenscreen_tolerance_v"), "value-changed", G_CALLBACK(gui_set_greenscreen_tolerance), (void*)2);
  // Connect signal for hiding cameras
  g_signal_connect(gtk_builder_get_object(gui, "cam_menu_hide"), "activate", G_CALLBACK(gui_hide_cam), 0);
  // Load placeholder animation for cameras (no video data yet or mic only)
  if(frombuild)
  {
    camplaceholder=gdk_pixbuf_animation_new_from_file("camplaceholder.gif", 0);
  }else{
    camplaceholder=gdk_pixbuf_animation_new_from_file(PREFIX "/share/tc_client/camplaceholder.gif", 0);
  }
  camplaceholder_iter=gdk_pixbuf_animation_get_iter(camplaceholder, 0);
  // Populate saved channels
  GtkWidget* startbox=GTK_WIDGET(gtk_builder_get_object(gui, "startbox"));
  int channelcount=config_get_int("channelcount");
  if(channelcount)
  {
    gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(gui, "channel_placeholder")));
  }
  char buf[256];
  for(i=0; i<channelcount; ++i)
  {
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    #ifdef GTK_STYLE_CLASS_LINKED
      GtkStyleContext* style=gtk_widget_get_style_context(box);
      gtk_style_context_add_class(style, GTK_STYLE_CLASS_LINKED);
    #endif
    sprintf(buf, "channel%i_name", i);
    const char* name=config_get_str(buf);
    GtkWidget* connectbutton=gtk_button_new_with_label(name);
    g_signal_connect(connectbutton, "clicked", G_CALLBACK(startsession), (void*)(intptr_t)i);
    gtk_box_pack_start(GTK_BOX(box), connectbutton, 1, 1, 0);
    GtkWidget* cfgbutton=gtk_button_new_from_icon_name("gtk-preferences", GTK_ICON_SIZE_BUTTON);
    struct channelopts* opts=malloc(sizeof(struct channelopts));
    opts->channel_id=i;
    opts->save=1;
    g_signal_connect(cfgbutton, "clicked", G_CALLBACK(channeldialog), opts);
    gtk_box_pack_start(GTK_BOX(box), cfgbutton, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(startbox), box, 0, 0, 2);
  }
  gtk_widget_show_all(startwindow);
}
