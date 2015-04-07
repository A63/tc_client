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
#include <termios.h>
#include <locale.h>
#include <curses.h>
#include <readline/readline.h>
#include "../compat.h"
#include "../list.h"
#include "buffer.h"

#define HALFSCREEN (LINES>4?(LINES-3)/2:1)

WINDOW* topic;
char* channeltopic;
WINDOW* input;
int to_app;
struct list userlist={0,0};

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
  WINDOW* w=buffers[currentbuf].pad;
  int scroll=buffers[currentbuf].scroll;
  prefresh(w, (scroll>-1?scroll:getcury(w)-LINES+4), 0, 1, 0, LINES-3, COLS);
}

void drawtopic(void)
{
  werase(topic);
  unsigned int i;
  for(i=1; i<buffercount && buffers[i].seen; ++i);
  if(i<buffercount)
  {
    waddstr(topic, "Unread PMs from: ");
    char first=1;
    for(i=1; i<buffercount; ++i)
    {
      if(!buffers[i].seen)
      {
        if(first){first=0;}else{waddstr(topic, ", ");}
        waddstr(topic, buffers[i].name);
      }
    }
  }
  else if(currentbuf)
  {
    waddstr(topic, "To return to public chat type: /pm");
  }else{
    waddstr(topic, channeltopic);
  }
  wrefresh(topic);
}

void gotline(char* line)
{
  if(!line){close(to_app); return;} // TODO: handle EOF on stdin better?
  if(!strcmp(line, "/pm"))
  {
    currentbuf=0;
    drawchat();
    drawtopic();
    return;
  }
  else if(!strncmp(line, "/pm ", 4))
  {
    currentbuf=findbuffer(&line[4]);
    if(!currentbuf){currentbuf=createbuffer(&line[4]);}
    buffers[currentbuf].seen=1;
    drawchat();
    drawtopic();
    return;
  }
  else if(!strncmp(line, "/buffer ", 8))
  {
    unsigned int num=atoi(&line[8]);
    if(num<0 || num>=buffercount)
    {
      wprintw(buffers[currentbuf].pad, "\nInvalid buffer number: %u", num);
    }else{
      currentbuf=num;
      buffers[currentbuf].seen=1;
      drawtopic();
    }
    drawchat();
    return;
  }
  else if(!strcmp(line, "/bufferlist"))
  {
    unsigned int i;
    for(i=0; i<buffercount; ++i)
    {
      wprintw(buffers[currentbuf].pad, "\n% 3i: %s", i, i?buffers[i].name:"");
    }
    drawchat();
    return;
  }
  else if(!strncmp(line, "/msg ", 5))
  {
    char* name=&line[5];
    char* msg=strchr(name, ' ');
    if(!msg){return;}
    msg[0]=0;
    currentbuf=findbuffer(name);
    if(!currentbuf){currentbuf=createbuffer(name);}
    buffers[currentbuf].seen=1;
    drawtopic();
    memmove(line, &msg[1], strlen(&msg[1])+1);
  }
  else if(!strcmp(line, "/help"))
  {
    waddstr(buffers[0].pad, "\nFor cursedchat:\n"
      "/pm <name>    = switch to the PM buffer for <name>\n"
      "/pm           = return to the channel/public chat's buffer\n"
      "/buffer <num> = switch to buffer by number\n"
      "/bufferlist   = list open buffers, their numbers and associated names\n"
      "\nFor tc_client (through cursedchat):");
    write(to_app, line, strlen(line));
    write(to_app, "\n", 1);
    return;
  }

  if(currentbuf) // We're in a PM window, make the message a PM
  {
    dprintf(to_app, "/msg %s ", buffers[currentbuf].name);
  }
  write(to_app, line, strlen(line));
  write(to_app, "\n", 1);
// TODO: grab user's nick for this
  wprintw(buffers[currentbuf].pad, "\n%s: %s", "You", line);
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
    struct buffer* b=&buffers[currentbuf];
    if(b->scroll<0){b->scroll=getcury(b->pad)-LINES+4;}
    b->scroll-=HALFSCREEN;
    if(b->scroll<0){b->scroll=0;}
    drawchat();
    return 0;
  }
  if(!strcmp(buf, "[6")) // Page down
  {
    read(0, buf, 1);
    struct buffer* b=&buffers[currentbuf];
    if(b->scroll<0){return 0;} // Already at the bottom
    b->scroll+=HALFSCREEN;
    if(b->scroll>getcury(b->pad)-LINES+3){b->scroll=-1;}
    drawchat();
    return 0;
  }
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
  clear();
  refresh();
  wresize(topic, 1, COLS);
  unsigned int i;
  for(i=0; i<buffercount; ++i)
  {
    wresize(buffers[i].pad, buffers[i].pad->_maxy+1, COLS);
  }
  wresize(input, 2, COLS);
  mvwin(input, LINES-2, 0);
  redrawwin(buffers[currentbuf].pad);
  redrawwin(topic);
  redrawwin(input);
  drawchat();
  drawtopic();
  drawinput();
}

void dontprintmatches(char** matches, int num, int maxlen)
{
}

unsigned int completionmatch;
char* completenicks(const char* text, int state)
{
  // text is the word we're completing on, state is the iteration count (one iteration per matching name, until we return 0)
  if(!state){completionmatch=0;}
  while(completionmatch<userlist.itemcount)
  {
    if(!strncmp(userlist.items[completionmatch], text, strlen(text)))
    {
      char* completion=malloc(strlen(userlist.items[completionmatch])+2);
      strcpy(completion, userlist.items[completionmatch]);
      // Check if we're on the first word and only add the ":" if we are
      if(strlen(text)>=rl_point){strcat(completion, ":");}
      ++completionmatch;
      return completion;
    }
    ++completionmatch;
  }
  return 0;
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
  createbuffer(0);
  input=newwin(2, COLS, LINES-2, 0);
  scrollok(input, 1);
  rl_initialize();
  rl_callback_handler_install(0, gotline);
  rl_bind_key('\x1b', escinput);
  rl_completion_display_matches_hook=dontprintmatches;
  rl_completion_entry_function=completenicks;
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
      unsigned int buffer=0;
      if(!strncmp(buf, "Room topic: ", 12))
      {
        free(channeltopic);
        channeltopic=strdup(&buf[12]);
        drawtopic();
      }
      else if(!strncmp(buf, "Currently online: ", 18))
      {
        // Populate the userlist
        char* name=&buf[16];
        while(name)
        {
          name=&name[2];
          char* next=strstr(name, ", ");
          if(next){next[0]=0;}
          list_add(&userlist, name);
          if(next){next[0]=',';}
          name=next;
        }
      }
      else if(buf[0]=='['&&isdigit(buf[1])&&isdigit(buf[2])&&buf[3]==':'&&isdigit(buf[4])&&isdigit(buf[5])&&buf[6]==']'&&buf[7]==' ')
      {
        char* nick=&buf[8];
        char* msg=strchr(nick, ' ');
        if(msg[-1]==':')
        {
          nick=strchr(nick, 'm')+1;
          char* nickend=&msg[-1];
          msg=&msg[1];
          if(!strncmp(msg, "/msg ", 5)) // message is a PM
          {
            char* pm=strchr(&msg[5], ' ');
            if(!pm){waddstr(buffers[0].pad, "\npm is null!"); continue;}
            pm=&pm[1];
            nickend[0]=0;
            buffer=findbuffer(nick);
            if(!buffer){buffer=createbuffer(nick);}
            nickend[0]=':';
            memmove(msg, pm, strlen(pm)+1);
            if(buffer!=currentbuf)
            {
              buffers[buffer].seen=0;
              drawtopic();
            }
          }
        }
        else if(!strncmp(msg, " changed nickname to ", 21))
        {
          msg[0]=0;
          // Update name in userlist
          list_switch(&userlist, nick, &msg[21]);
          unsigned int i;
          // Prevent duplicate names for buffers, and all the issues that would bring
          if((i=findbuffer(&msg[21])))
          {
            renamebufferunique(i);
          }
          for(i=1; i<buffercount; ++i)
          {
            if(!strcmp(buffers[i].name, nick))
            {
              free(buffers[i].name);
              buffers[i].name=strdup(&msg[21]);
            }
          }
          msg[0]=' ';
        }
        else if(!strcmp(msg, " entered the channel"))
        {
          msg[0]=0;
          // Add to the userlist
          list_add(&userlist, nick);
          msg[0]=' ';
        }
        else if(!strcmp(msg, " left the channel"))
        {
          msg[0]=0;
          // Remove from the userlist
          list_del(&userlist, nick);
          msg[0]=' ';
        }
      }
      waddstr(buffers[buffer].pad, "\n");
      waddansi(buffers[buffer].pad, buf);
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
