/*
    cursedchat, a simple curses interface for tc_client
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
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <locale.h>
#include <curses.h>
#include <readline/readline.h>

#define HALFSCREEN (LINES>4?(LINES-3)/2:1)
WINDOW* topic;
WINDOW* chat;
WINDOW* input;
int chatscroll=-1;
int to_app;

// Translate ANSI escape codes to curses commands and write the text to a window
void waddansi(WINDOW* w, char* str)
{
  while(str[0])
  {
    char* esc=strstr(str, "\x1b[");
    if(esc==str)
    {
      str=&str[2];
      while(str[0]!='m')
      {
        if(str[0]=='3'&&str[1]!='m') // Color
        {
          unsigned int c=strtoul(&str[1], &str, 10);
          wattron(w, COLOR_PAIR(c+1));
        }
        else if(str[0]=='1') // Bold
        {
          wattron(w, A_BOLD);
          str=&str[1];
        }
        else if(str[0]=='0') // Reset
        {
          wattroff(w, COLOR_PAIR(1));
          wattroff(w, A_BOLD);
          str=&str[1];
        }
        else{str=&str[1];}
      }
      str=&str[1];
      continue;
    }
    if(esc)
    {
      waddnstr(w, str, esc-str);
      str=esc;
    }else{
      waddstr(w, str);
      return;
    }
  }
}

void drawchat(void)
{
  prefresh(chat, (chatscroll>-1?chatscroll:getcury(chat)-LINES+4), 0, 1, 0, LINES-3, COLS);
}

void gotline(char* line)
{
  if(!line){close(to_app); return;} // TODO: handle EOF on stdin better?
// TODO: handle commands (/pm, /help addition)

  write(to_app, line, strlen(line));
  write(to_app, "\n", 1);
// TODO: grab user's nick for this
  wprintw(chat, "\n%s: %s", "You", line);
  drawchat();
}

unsigned int bytestochars(const char* buf, unsigned int buflen, unsigned int bytes)
{
  unsigned int pos=0;
  unsigned int i;
  for(i=0; i<bytes; ++pos)
  {
    i+=mbtowc(0,&buf[i],buflen-i);
  }
  return pos;
}

unsigned int charstobytes(const char* buf, unsigned int buflen, unsigned int chars)
{
  unsigned int pos;
  unsigned int i=0;
  for(pos=0; pos<chars; ++pos)
  {
    i+=mbtowc(0,&buf[i],buflen-i);
  }
  return i;
}

int escinput(int a, int byte)
{
  char buf[4];
  read(0, buf, 2);
  buf[2]=0;
  if(!strcmp(buf, "[A")||!strcmp(buf, "OA")){return 0;} // TODO: history?
  if(!strcmp(buf, "[B")||!strcmp(buf, "OB")){return 0;} // TODO: history?
  if(!strcmp(buf, "[C")||!strcmp(buf, "OC")){rl_forward(1,27);return 0;}
  if(!strcmp(buf, "[D")||!strcmp(buf, "OD")){rl_backward(1,27);return 0;}
  if(!strcmp(buf, "[H")||!strcmp(buf, "OH")){rl_beg_of_line(1,27);return 0;}
  if(!strcmp(buf, "[F")||!strcmp(buf, "OF")){rl_end_of_line(1,27);return 0;}
  if(!strcmp(buf, "[3")&&read(0, buf, 1)&&buf[0]=='~'){rl_delete(1,27);return 0;}
  if(!strcmp(buf, "[5")) // Page up
  {
    read(0, buf, 1);
    if(chatscroll<0){chatscroll=getcury(chat)-LINES+4;}
    chatscroll-=HALFSCREEN; // (LINES-3)/2;
    if(chatscroll<0){chatscroll=0;}
    drawchat();
    return 0;
  }
  if(!strcmp(buf, "[6")) // Page down
  {
    read(0, buf, 1);
    if(chatscroll<0){return 0;} // Already at the bottom
    chatscroll+=HALFSCREEN; // (LINES-3)/2;
    if(chatscroll>getcury(chat)-LINES+3){chatscroll=-1;}
    drawchat();
    return 0;
  }
//  wprintw(chat, "\nbuf: %s", buf);
  return 0;
}

void drawinput(void)
{
  werase(input);
  unsigned int pos=bytestochars(rl_line_buffer, rl_end, rl_point);

  waddstr(input, "> ");
  int cursor_row=(pos+2)/COLS;
  int end_row=(rl_end+2)/COLS;
  // Figure out how much of the buffer to print to not scroll past the cursor
  unsigned int eol=charstobytes(rl_line_buffer, rl_end, (cursor_row+2)*COLS-3); // -2 for cursor, -1 to avoid wrapping
  waddnstr(input, rl_line_buffer, eol);

  wmove(input, cursor_row==end_row && cursor_row>0, (pos+2)%COLS); // +2 for prompt
  wrefresh(input);
}

void resizechat(int sig)
{
  struct winsize size;
  ioctl(0, TIOCGWINSZ, &size);
  if(size.ws_row<3){return;} // Too small, would result in negative numbers breaking the chat window
  resize_term(size.ws_row, size.ws_col);
  wresize(topic, 1, COLS);
  wresize(chat, chat->_maxy+1, COLS);
  wresize(input, 2, COLS);
  mvwin(input, LINES-2, 0);
  redrawwin(chat);
  redrawwin(topic);
  redrawwin(input);
  drawchat();
  wrefresh(topic);
  drawinput();
}

int main(int argc, char** argv)
{
  if(argc<3){execv("./tc_client", argv); return 1;}
  setlocale(LC_ALL, "");
  WINDOW* w=initscr();
  signal(SIGWINCH, resizechat);
  start_color();
  cbreak();
  noecho();
  keypad(w, 1);
  use_default_colors();
  topic=newwin(1, COLS, 0, 0);
  init_pair(1, COLOR_WHITE, COLOR_BLUE);

  // Define colors mapped to ANSI color codes (at least the ones tc_client uses)
  init_pair(2, COLOR_RED, -1);
  init_pair(3, COLOR_GREEN, -1);
  init_pair(4, COLOR_YELLOW, -1);
  init_pair(5, COLOR_BLUE, -1);
  init_pair(6, COLOR_MAGENTA, -1);
  init_pair(7, COLOR_CYAN, -1);

  wbkgd(topic, COLOR_PAIR(1)|' ');
  chat=newpad(2048, COLS);
  scrollok(chat, 1);
  input=newwin(2, COLS, LINES-2, 0);
  scrollok(input, 1);
  rl_initialize();
  rl_callback_handler_install(0, gotline);
  rl_bind_key('\x1b', escinput);
  wprintw(input, "> ");
  wrefresh(topic);
  wrefresh(input);
  int app_in[2];
  int app_out[2];
  pipe(app_in);
  pipe(app_out);
  if(!fork())
  {
    close(app_in[1]);
    close(app_out[0]);
    dup2(app_in[0],0);
    dup2(app_out[1],1);
    argv[0]="./tc_client";
    execv("./tc_client", argv);
    _exit(1);
  }
  close(app_in[0]);
  close(app_out[1]);
  to_app=app_in[1];
  struct pollfd p[2]={{.fd=0, .events=POLLIN, .revents=0},
                      {.fd=app_out[0], .events=POLLIN, .revents=0}};
  while(1)
  {
    poll(p, 2, -1);
    if(p[1].revents) // Getting data from tc_client
    {
      p[1].revents=0;
      char buf[1024];
      size_t len=0;
      while(len<1023)
      {
        if(read(app_out[0], &buf[len], 1)!=1){len=-1; break;}
        if(buf[len]=='\r'||buf[len]=='\n'){break;}
        ++len;
      }
      if(len==-1){break;} // Bad read
      buf[len]=0;
      if(!strncmp(buf, "Room topic: ", 12))
      {
        werase(topic);
        waddstr(topic, &buf[12]);
        wrefresh(topic);
      }
      waddstr(chat, "\n");
      waddansi(chat, buf);
      drawchat();
      wrefresh(input);
      continue;
    }
    if(!p[0].revents){continue;}
    p[0].revents=0;
    rl_callback_read_char();
    drawinput();
  }
  rl_callback_handler_remove();
  endwin();
  return 0;
}
