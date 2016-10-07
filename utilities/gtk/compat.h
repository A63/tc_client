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
#ifdef _WIN32
#include <wtypes.h>
#include <fcntl.h>
extern SECURITY_ATTRIBUTES sa;
#define kill(pid, x) TerminateProcess(pid, 0)
#define w32_runcmd(cmd) \
  if(cmd[0]) \
  { \
    char* arg=strchr(cmd,' '); \
    if(arg){arg[0]=0; arg=&arg[1];} \
    ShellExecute(0, "open", cmd, arg, 0, SW_SHOWNORMAL); \
  }
#define w32_runcmdpipes(cmd, pipein, pipeout, procinfo) \
  { \
    HANDLE h_pipe_in0, h_pipe_in1; \
    if(pipein) \
    { \
      CreatePipe(&h_pipe_in0, &h_pipe_in1, &sa, 0); \
      pipein[0]=_open_osfhandle(h_pipe_in0, _O_RDONLY); \
      pipein[1]=_open_osfhandle(h_pipe_in1, _O_WRONLY); \
    } \
    HANDLE h_pipe_out0, h_pipe_out1; \
    if(pipeout) \
    { \
      CreatePipe(&h_pipe_out0, &h_pipe_out1, &sa, 0); \
      pipeout[0]=_open_osfhandle(h_pipe_out0, _O_RDONLY); \
      pipeout[1]=_open_osfhandle(h_pipe_out1, _O_WRONLY); \
    } \
    STARTUPINFO startup; \
    GetStartupInfo(&startup); \
    startup.dwFlags|=STARTF_USESTDHANDLES; \
    if(pipein){startup.hStdInput=h_pipe_in0;} \
    if(pipeout){startup.hStdOutput=h_pipe_out1;} \
    CreateProcess(0, cmd, 0, 0, 1, DETACHED_PROCESS, 0, 0, &startup, &procinfo); \
  }
#define pipe(fds) \
  { \
    HANDLE pipe0, pipe1; \
    CreatePipe(&pipe0, &pipe1, &sa, 0); \
    fds[0]=_open_osfhandle(pipe0, _O_RDONLY); \
    fds[1]=_open_osfhandle(pipe1, _O_WRONLY); \
  }
#endif
#if GTK_MAJOR_VERSION<3
  #define GTK_ORIENTATION_HORIZONTAL 0
  #define GTK_ORIENTATION_VERTICAL 1
  #define gtk_widget_set_halign(x,y)
  #define gtk_widget_get_preferred_size(x,y,z) (y)->height=gtk_widget_get_allocated_height(x)
  typedef struct{double red; double green; double blue; double alpha;} GdkRGBA;
  #define gtk_color_chooser_set_rgba(x,c) \
  {\
    GdkColor gdkc={.red=(c)->red*65535,\
                   .green=(c)->green*65535,\
                   .blue=(c)->blue*65535};\
    gtk_color_button_set_color(x,&gdkc);\
  }
  #define gtk_color_chooser_get_rgba(x,c) \
  {\
    GdkColor gdkc;\
    gtk_color_button_get_color(x,&gdkc);\
    (c)->red=(double)gdkc.red/65535;\
    (c)->green=(double)gdkc.green/65535;\
    (c)->blue=(double)gdkc.blue/65535;\
  }
  #define GTK_COLOR_CHOOSER GTK_COLOR_BUTTON
  #define GtkColorChooser GtkColorButton
  extern GtkWidget* gtk_box_new(int vertical, int spacing);
  extern int gtk_widget_get_allocated_width(GtkWidget* widget);
  extern int gtk_widget_get_allocated_height(GtkWidget* widget);
  extern const char* gtk_combo_box_get_active_id(GtkComboBox* combo);
#endif
#if GTK_MAJOR_VERSION<3 || (GTK_MAJOR_VERSION==3 && GTK_MINOR_VERSION<10)
  #define gtk_button_new_from_icon_name(name, size) gtk_button_new_from_stock(name)
  extern GtkBuilder* gtk_builder_new_from_file(const char* filename);
#endif
#if GTK_MAJOR_VERSION<3 || (GTK_MAJOR_VERSION==3 && GTK_MINOR_VERSION<16)
extern void gtk_paned_set_wide_handle(void*, char);
#endif
#if GDK_PIXBUF_MAJOR<2 || (GDK_PIXBUF_MAJOR==2 && GDK_PIXBUF_MINOR<32)
  #define gdk_pixbuf_read_pixels gdk_pixbuf_get_pixels
#endif
