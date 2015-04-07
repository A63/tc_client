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
  unsigned int len;
  while(item[0])
  {
    if(item[0]=='\r' || item[0]=='\n'){item=&item[1]; continue;} // Skip empty lines
    for(len=0; item[len] && item[len]!='\r' && item[len]!='\n'; ++len);
    for(i=0; i<list->itemcount; ++i)
    {
      if(!strncmp(list->items[i], item, len) && !list->items[i][len])
      {
        free(list->items[i]);
        --list->itemcount;
        memmove(&list->items[i], &list->items[i+1], sizeof(char*)*(list->itemcount-i));
      }
    }
    item=&item[len];
  }
}

void list_add(struct list* list, const char* item)
{
  list_del(list, item);
  unsigned int len;
  while(item[0])
  {
    if(item[0]=='\r' || item[0]=='\n'){item=&item[1]; continue;} // Skip empty lines
    for(len=0; item[len] && item[len]!='\r' && item[len]!='\n'; ++len);
    ++list->itemcount;
    list->items=realloc(list->items, sizeof(char*)*list->itemcount);
    list->items[list->itemcount-1]=strndup(item, len);
    item=&item[len];
  }
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
  unsigned int len;
  while(item[0])
  {
    if(item[0]=='\r' || item[0]=='\n'){item=&item[1]; continue;} // Skip empty lines
    for(len=0; item[len] && item[len]!='\r' && item[len]!='\n'; ++len);
    for(i=0; i<list->itemcount; ++i)
    {
      if(!strncmp(list->items[i], item, len) && !list->items[i][len]){return i;}
    }
    item=&item[len];
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

void list_movetofront(struct list* list, unsigned int pos)
{
  if(pos>=list->itemcount){return;}
  char* move=list->items[pos];
  memmove(&list->items[1], list->items, sizeof(char*)*pos);
  list->items[0]=move;
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

void getvidinfo(const char* vid, const char* type, char* buf, unsigned int len)
{
  int out[2];
  pipe(out);
  if(!fork())
  {
    close(out[0]);
    dup2(out[1], 1);
    execlp("youtube-dl", "youtube-dl", "--default-search", "auto", type, "--", vid, (char*)0);
    perror("execlp(youtube-dl)");
    _exit(1);
  }
  wait(0);
  close(out[1]);
  len=read(out[0], buf, len-1);
  if(len<0){len=0;}
  while(len>0 && (buf[len-1]=='\r' || buf[len-1]=='\n')){--len;} // Strip newlines
  buf[len]=0;
  close(out[0]);
}

unsigned int getduration(const char* vid)
{
  char timebuf[128];
  timebuf[0]=':'; // Sacrifice 1 byte to avoid having to deal with a special case later on, where no ':' is found and we go from the start of the string, but only once
  getvidinfo(vid, "--get-duration", &timebuf[1], 127);
  if(!timebuf[1]){printf("Failed to get video duration using youtube-dl, assuming 60s\n"); return 60;} // If using youtube-dl fails, assume videos are 1 minute long
  // youtube-dl prints it out in hh:mm:ss format, convert it to plain seconds
  unsigned int len;
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
  return len;
}

unsigned int waitskip=0;
void playnextvid()
{
  waitskip=0;
  playing=queue.items[0];
  --queue.itemcount;
  memmove(queue.items, &queue.items[1], sizeof(char*)*queue.itemcount);
  say(0, "/mbs youTube %s 0\n", playing);
  // Find out the video's length and schedule an alarm for then
  alarm(getduration(playing));
}

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
          waitskip=i;
          alarm(120);
          break;
        }
      }
      return;
    }else{
      say(0, "Skipping http://youtube.com/watch?v=%s because it is still not approved after 2 minutes\n", queue.items[0]);
      list_movetofront(&queue, waitskip);
      waitskip=0;
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
          char title[256];
          char vid[1024];
          getvidinfo(&msg[9], "--get-id", vid, 1024);
          if(!vid[0]){say(pm, "No video found, sorry\n"); continue;} // Nothing found
          char* plist;
          for(plist=vid; plist[0] && plist[0]!='\r' && plist[0]!='\n'; plist=&plist[1]);
          if(plist[0]) // Link was a playlist, do some trickery to get the title of the first video (instead of getting nothing)
          {
            strcpy(title, "Playlist, starting with ");
            plist[0]=0;
            getvidinfo(vid, "--get-title", &title[24], 256-24);
            plist[0]='\n';
          }else{
            plist=0;
            getvidinfo(vid, "--get-title", title, 256);
          }
          printf("Requested ID '%s' by '%s'\n", vid, nick);
          // Check if it's already queued and mention which spot it's in, or if it's marked as bad and shouldn't be queued
          int pos;
          if((pos=list_getpos(&queue, vid))>-1)
          {
            say(pm, "Video '%s' is already in queue (number %i)\n", title, pos);
            continue;
          }
          if(list_contains(&badvids, vid))
          {
            say(pm, "Video '%s' is marked as bad, won't add to queue\n", title);
            continue;
          }
          if(list_contains(&mods, nick)) // Auto-approve for mods
          {
            list_add(&goodvids, vid);
            list_del(&badvids, vid);
            list_save(&goodvids, "goodvids.txt");
            list_save(&badvids, "badvids.txt");
          }

          list_add(&queue, vid);
          if(!list_contains(&goodvids, vid))
          {
            if(plist)
            {
              say(pm, "Playlist '%s' is added to the queue but will need to be approved by a mod\n", title);
            }else{
              say(pm, "Video '%s' (%s) is added to the queue but will need to be approved by a mod\n", vid, title);
            }
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
        else if(!strcmp(msg, "!time")) // Debugging
        {
          unsigned int remaining=alarm(0);
          alarm(remaining);
          say(pm, "'%s' is scheduled to end in %u seconds\n", playing, remaining);
        }
        else if(!strcmp(msg, "!help"))
        {
          say(nick, "The following commands can be used:\n");
          usleep(100000);
          say(nick, "!request <link> = request a video to be played\n");
          usleep(100000);
          say(nick, "!queue          = get the number of songs in queue and which (if any) need to  be approved\n");
          usleep(100000);
          say(nick, "Mod commands:\n"); // TODO: don't bother filling non-mods' chats with these?
          usleep(100000);
          say(nick, "!playnext       = play the next video in queue without approving it (to see if it's ok)\n");
          usleep(100000);
          say(nick, "!approve        = mark the currently playing video as good, or if none is playing the next in queue\n");
          usleep(100000);
          say(nick, "!approve <link> = mark the specified video as okay\n");
          say(nick, "!approve next   = mark the next not yet approved video as okay\n");
          say(nick, "!approve entire queue = approve all videos in queue (for playlists)\n");
          usleep(100000);
          say(nick, "!badvid         = stop playing the current video and mark it as bad\n");
          usleep(100000);
          say(nick, "!badvid <link>  = mark the specified video as bad, preventing it from ever being queued again\n");
          say(nick, "You can also just play videos manually and they will be marked as good.\n");
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
              if(list_contains(&goodvids, playing) && !list_contains(&badvids, playing)){say(pm, "'%s' is already approved, use !approve <ID> to approve another video\n", playing); continue;}
              list_add(&goodvids, playing);
              list_del(&badvids, playing);
              list_save(&goodvids, "goodvids.txt");
              list_save(&badvids, "badvids.txt");
            }else if(queue.itemcount>0){
              if(list_contains(&goodvids, queue.items[0]) && !list_contains(&badvids, queue.items[0])){say(pm, "'%s' is already approved, use !approve <ID> to approve another video\n", queue.items[0]); continue;}
              list_add(&goodvids, queue.items[0]);
              list_del(&badvids, queue.items[0]);
              list_save(&goodvids, "goodvids.txt");
              list_save(&badvids, "badvids.txt");
              playnext(0);
            }else{say(pm, "Approve what? please specify a video\n");}
          }
          else if(!strncmp(msg, "!approve ", 9))
          {
            char* vid=&msg[9];
            if(!vid[0]){continue;} // No video specified
            char vidbuf[256];
            if(!strcmp(vid, "next"))
            {
              unsigned int i;
              for(i=0; i<queue.itemcount; ++i)
              {
                if(!list_contains(&goodvids, queue.items[i])){vid=queue.items[i]; break;}
              }
              if(i==queue.itemcount){say(pm, "Nothing more to approve :)\n"); continue;}
            }
            else if(!strcmp(vid, "entire queue"))
            {
              char approved=0;
              unsigned int i;
              for(i=0; i<queue.itemcount; ++i)
              {
                if(list_contains(&goodvids, queue.items[i])){continue;}
                list_add(&goodvids, queue.items[i]);
                approved=1;
              }
              if(approved)
              {
                list_save(&goodvids, "goodvids.txt");
                if(!playing){playnext(0);} // Next in queue just got approved, so play it
              }else{
                say(0, "%s: there is nothing in the queue that isn't already approved, please do not overuse this function\n", nick);
              }
              continue;
            }else{
              getvidinfo(vid, "--get-id", vidbuf, 256);
              vid=vidbuf;
            }
            list_add(&goodvids, vid);
            list_del(&badvids, vid);
            list_save(&goodvids, "goodvids.txt");
            list_save(&badvids, "badvids.txt");
            if(!playing && queue.itemcount>0 && !strcmp(vid, queue.items[0])){playnext(0);} // Next in queue just got approved, so play it
          }
          else if(!strcmp(msg, "!badvid") || !strncmp(msg, "!badvid ", 8))
          {
            if(!msg[7] && !playing){say(pm, "Nothing is playing, please use !badvid <URL/ID> instead\n"); continue;}
            char vid[1024];
            if(msg[7])
            {
              getvidinfo(&msg[8], "--get-id", vid, 256);
            }else{strncpy(vid, playing, 1023); vid[1023]=0;}
            if(!vid[0]){say(pm, "Video not found, sorry\n");}
            list_del(&queue, vid);
            list_del(&goodvids, vid);
            list_add(&badvids, vid);
            list_save(&goodvids, "goodvids.txt");
            list_save(&badvids, "badvids.txt");
            if(!strcmp(vid, playing)){say(0, "/mbc youTube\n"); playnext(0);}
          }
          else if(!strncmp(msg, "/mbs youTube ", 13))
          {
            // Someone manually started a video, mark that video as good, remove it from queue, and set an alarm for when it's modbot's turn to play stuff again
            char* vid=&msg[13];
            char* end=strchr(vid, ' ');
            if(end){end[0]=0;}
            list_del(&queue, vid);
            list_add(&goodvids, vid);
            list_save(&goodvids, "goodvids.txt");
            free(playing);
            playing=strdup(vid);
            alarm(getduration(vid));
          }
          else if(!strcmp(msg, "/mbc youTube")){playnext(0);} // Video cancelled
          else if(!strncmp(msg, "/mbsk youTube ", 14)) // Seeking
          {
            unsigned int pos=strtol(&msg[14], 0, 0)/1000;
            alarm(getduration(playing)-pos);
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
