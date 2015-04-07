/*
    modbot, a bot for tc_client that queues and plays videos
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdarg.h>

struct list
{
  char** items;
  unsigned int itemcount;
};

struct list mods={0,0};
struct list queue={0,0};
struct list goodvids={0,0}; // pre-approved videos
struct list badvids={0,0}; // not allowed, essentially banned
char* playing=0;
int tc_client;

void list_del(struct list* list, const char* item)
{
  unsigned int i;
  for(i=0; i<list->itemcount; ++i)
  {
    if(!strcmp(list->items[i], item))
    {
      free(list->items[i]);
      --list->itemcount;
      memmove(&list->items[i], &list->items[i+1], sizeof(char*)*list->itemcount);
    }
  }
}

void list_add(struct list* list, const char* item)
{
  list_del(list, item);
  ++list->itemcount;
  list->items=realloc(list->items, sizeof(char*)*list->itemcount);
  list->items[list->itemcount-1]=strdup(item);
}

void list_switch(struct list* list, char* olditem, char* newitem)
{
  unsigned int i;
  for(i=0; i<list->itemcount; ++i)
  {
    if(!strcmp(list->items[i], olditem))
    {
      free(list->items[i]);
      list->items[i]=strdup(newitem);
    }
  }
}

int list_getpos(struct list* list, char* item)
{
  int i;
  for(i=0; i<list->itemcount; ++i)
  {
    if(!strcmp(list->items[i], item)){return i;}
  }
  return -1;
}

char list_contains(struct list* list, char* item)
{
  return (list_getpos(list, item)!=-1);
}

void list_load(struct list* list, const char* file)
{
  struct stat st;
  if(stat(file, &st)){return;}
  int f=open(file, O_RDONLY);
  char buf[st.st_size+1];
  read(f, buf, st.st_size);
  buf[st.st_size]=0;
  close(f);
  char* start=buf;
  char* end;
  while((end=strchr(start, '\n'))) // TODO: support \r?
  {
    end[0]=0;
    list_add(list, start);
    start=&end[1];
  }
  printf("Loaded %u lines from %s\n", list->itemcount, file);
}

void list_save(struct list* list, const char* file)
{
  int f=open(file, O_WRONLY|O_TRUNC|O_CREAT, 0600);
  unsigned int i;
  for(i=0; i<list->itemcount; ++i)
  {
    write(f, list->items[i], strlen(list->items[i]));
    write(f, "\n", 1);
  }
  close(f);
}

void list_movetoback(struct list* list, const char* item)
{
  char tmp[strlen(item)+1];
  strcpy(tmp, item);
  list_del(list, tmp);
  list_add(list, tmp);
}

char* getyoutube(char* string) // Extract youtube ID from URL/other forms
{
  char* x;
  if((x=strstr(string, "?v=")))
  {
    return &x[3];
  }
  return string; // last resort: assume it's already an ID
}

void say(const char* pm, const char* fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  unsigned int len=vsnprintf(0,0,fmt,va);
  va_end(va);
  va_start(va, fmt);
  char buf[len+1+(pm?strlen("/msg  ")+strlen(pm):0)];
  char* msg=buf;
  if(pm)
  {
    msg=&buf[strlen("/msg  ")+strlen(pm)];
    sprintf(buf, "/msg %s ", pm);
  }
  vsprintf(msg, fmt, va);
  va_end(va);
  write(tc_client, buf, strlen(buf));
}

void playnextvid()
{
  playing=queue.items[0];
  --queue.itemcount;
  memmove(queue.items, &queue.items[1], sizeof(char*)*queue.itemcount);
  say(0, "/mbs youTube %s 0\n", playing);
  // Find out the video's length and schedule an alarm for then
  int out[2];
  pipe(out);
  if(!fork())
  {
    close(out[0]);
    dup2(out[1], 1);
    close(2); // Ignore youtube-dl errors/warnings
    write(1, ":", 1);
    execlp("youtube-dl", "youtube-dl", "--get-duration", playing, (char*)0);
    perror("execlp(youtube-dl)");
    _exit(1);
  }
  close(out[1]);
  wait(0);
  char timebuf[128];
  int len=read(out[0], timebuf, 127);
  if(len<1){alarm(60); return;}
  timebuf[len]=0;
  close(out[0]);
  // youtube-dl prints it out in hh:mm:ss format, convert it to plain seconds
  // Seconds
  char* sep=strrchr(timebuf, ':');
  if(sep){sep[0]=0; len=atoi(&sep[1]);}
  // Minutes
  sep=strrchr(timebuf, ':');
  if(sep){sep[0]=0; len+=atoi(&sep[1])*60;}
  // Hours
  sep=strrchr(timebuf, ':');
  if(sep){sep[0]=0; len+=atoi(&sep[1])*60;}
  // Days
  sep=strrchr(timebuf, ':');
  if(sep){sep[0]=0; len+=atoi(&sep[1])*24;}
//  printf("Estimated video length to %u sec\n", len);
  alarm(len);
}

char waitskip=0;
void playnext(int x)
{
  free(playing);
  playing=0;
  if(queue.itemcount<1){alarm(0); printf("Nothing more to play\n"); return;} // Nothing more to play
  if(!list_contains(&goodvids, queue.items[0]))
  {
    if(!waitskip)
    {
      say(0, "Next video (http://youtube.com/watch?v=%s) is not yet approved by mods\n", queue.items[0]);
      unsigned int i;
      for(i=1; i<queue.itemcount; ++i)
      {
        if(list_contains(&goodvids, queue.items[i]))
        {
          waitskip=1;
          alarm(120);
          break;
        }
      }
      return;
    }else{
      waitskip=0;
      say(0, "Skipping http://youtube.com/watch?v=%s because it is still not approved after 2 minutes\n", queue.items[0]);
      list_movetoback(&queue, queue.items[0]);
      alarm(1);
    }
  }
  playnextvid();
}

int main(int argc, char** argv)
{
  int in[2];
  int out[2];
  pipe(in);
  pipe(out);
  if(!fork())
  {
    close(in[1]);
    close(out[0]);
    dup2(in[0], 0);
    dup2(out[1], 1);
    execv("./tc_client", argv);
    _exit(1);
  }
  close(in[0]);
  close(out[1]);
  tc_client=in[1];
  signal(SIGALRM, playnext);
  list_load(&goodvids, "goodvids.txt");
  list_load(&badvids, "badvids.txt");
  char buf[1024];
  int len=0;
  while(1)
  {
    if(read(out[0], &buf[len], 1)<1){break;}
    if(len<1023&&buf[len]!='\r'&&buf[len]!='\n'){++len; continue;}
    if(!len){continue;}
    buf[len]=0;
    char* esc;
    while((esc=strstr(buf, "\x1b["))) // Strip out ANSI colors
    {
      for(len=2; isdigit(esc[len])||esc[len]==';'; ++len);
      memmove(esc, &esc[len+1], strlen(&esc[len]));
    }
    len=0;
    // printf("Got line '%s'\n", buf);
    char* space=strchr(buf, ' ');
    if(!space){continue;}
    if(!strcmp(space, " is a moderator."))
    {
      // If there are not-yet-approved videos in the queue when a mod joins, ask them to review them
      space[0]=0;
      list_add(&mods, buf);
      continue;
    }
    if(!strcmp(space, " is no longer a moderator."))
    {
      space[0]=0;
      list_del(&mods, buf);
      continue;
    }
    space[0]=0;
    if(buf[0]=='['&&isdigit(buf[1])&&isdigit(buf[2])&&buf[3]==':') // Timestamp
    {
      char* nick=&space[1];
      space=strchr(nick, ' ');
      if(!space){continue;}
      if(space[-1]==':') // Sent a message
      {
        space[-1]=0;
        char* msg=&space[1];
        // Handle commands sent in PMs
        char* pm=0;
        if(!strncmp(msg, "/msg ", 5))
        {
          msg=strchr(&msg[5], ' ');
          if(!msg){continue;}
          msg=&msg[1];
          pm=nick;
        }
        if(!strncmp(msg, "!request ", 9))
        {
          char* vid=getyoutube(&msg[9]);
          printf("Requested ID '%s' by '%s'\n", vid, nick);
          // Check if it's already queued and mention which spot it's in
          int pos;
          if((pos=list_getpos(&queue, vid))>-1)
          {
            say(pm, "That video is already in queue (number %i)\n", pos);
          }
          if(list_contains(&mods, nick))
          {
// printf("is mod, video auto-approved\n");
            list_add(&goodvids, vid);
            list_del(&badvids, vid);
            list_save(&goodvids, "goodvids.txt");
            list_save(&badvids, "badvids.txt");
            list_add(&queue, vid);
          }else{ // Not a mod
            if(list_contains(&badvids, vid))
            {
              write(tc_client, "Video is marked as bad, won't add to queue\n", 43);
              continue;
            }
            list_add(&queue, vid);
          }
          if(!list_contains(&goodvids, vid))
          {
            say(pm, "Video '%s' is added to the queue but will need to be approved by mods\n", vid);
          }
          else if(!playing){playnext(0);}
          else{say(pm, "Added to queue\n");}
        }
        else if(!strcmp(msg, "!queue"))
        {
          unsigned int notapproved=0;
          unsigned int len=0;
          unsigned int i;
          for(i=0; i<queue.itemcount; ++i)
          {
            if(!list_contains(&goodvids, queue.items[i])){++notapproved; len+=strlen(queue.items[i])+2;}
          }
          if(notapproved)
          {
            char buf[len];
            buf[0]=0;
            for(i=0; i<queue.itemcount; ++i)
            {
              if(!list_contains(&goodvids, queue.items[i]))
              {
                if(buf[0]){strcat(buf, ", ");}
                strcat(buf, queue.items[i]);
              }
            }
            say(pm, "%u videos in queue, %u of which are not yet approved by mods (%s)\n", queue.itemcount, notapproved, buf);
          }else{
            say(pm, "%u videos in queue\n", queue.itemcount);
          }
        }
        else if(!strcmp(msg, "!help"))
        {
          say(nick, "The following commands can be used:\n");
          say(nick, "!request <link> = request a video to be played\n");
          say(nick, "!queue          = get the number of songs in queue and which (if any) need to  be approved\n");
          say(nick, "Mod commands:\n"); // TODO: don't bother filling non-mods' chats with these?
          say(nick, "!playnext       = play the next video in queue without approving it (to see if it's ok)\n");
          say(nick, "!approve        = mark the currently playing video, or if none is playing the next in queue\n");
          say(nick, "!approve <link> = mark the specified video as okay \n");
          say(nick, "!badvid         = stop playing the current video and mark it as bad\n");
          say(nick, "!badvid <link>  = mark the specified video as bad, preventing it from ever being queued again\n");
        }
        else if(list_contains(&mods, nick)) // Mods-only commands
        {
          if(!strcmp(msg, "!playnext"))
          {
            if(playing){say(pm, "A video (%s) is already playing\n", playing); continue;}
            if(queue.itemcount<1){say(pm, "There are no videos in queue, sorry\n"); continue;}
            playnextvid();
          }
          else if(!strcmp(msg, "!approve"))
          {
            if(playing)
            {
              list_add(&goodvids, playing);
              list_del(&badvids, playing);
              list_save(&goodvids, "goodvids.txt");
              list_save(&badvids, "badvids.txt");
            }else if(queue.itemcount>0){
              list_add(&goodvids, queue.items[0]);
              list_del(&badvids, queue.items[0]);
              list_save(&goodvids, "goodvids.txt");
              list_save(&badvids, "badvids.txt");
              playnext(0);
            }else{write(tc_client, "Approve what? please specify a video\n", 37);}
          }
          else if(!strncmp(msg, "!approve ", 9))
          {
            char* vid=getyoutube(&msg[9]);
            if(!vid[0]){vid=playing; if(!vid){continue;}}
            list_add(&goodvids, vid);
            list_save(&goodvids, "goodvids.txt");
            if(!playing && queue.itemcount>0 && !strcmp(vid, queue.items[0])){playnext(0);} // Next in queue just got approved, so play it
          }
          else if(!strcmp(msg, "!badvid") || !strncmp(msg, "!badvid ", 8))
          {
            char* vid=(msg[7]?&msg[8]:playing);
            if(!vid){say(pm, "Nothing is playing, please use !badvid <URL/ID> instead\n"); continue;}
            list_del(&queue, vid);
            list_del(&goodvids, vid);
            list_add(&badvids, vid);
            list_save(&goodvids, "goodvids.txt");
            list_save(&badvids, "badvids.txt");
            if(vid==playing){say(0, "/mbc youTube\n"); playnext(0);}
          }
        }
      }else{ // Actions
        if(!strncmp(space, " changed nickname to ", 21))
        {
          space[0]=0;
          if(list_contains(&mods, nick))
          {
            list_switch(&mods, nick, &space[21]);
            nick=&space[21];
            unsigned int i;
            for(i=0; i<queue.itemcount; ++i)
            {
              if(!list_contains(&goodvids, queue.items[i]))
              {
                say(nick, "there are 1 or more videos in queue that are not yet approved, please type !queue to review them\n");
                break;
              }
            }
          }
          continue;
        }
      }
    }
  }
  return 0;
}
