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
#endif
#if GTK_MAJOR_VERSION<3
  #define GTK_ORIENTATION_HORIZONTAL 0
  #define GTK_ORIENTATION_VERTICAL 1
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
