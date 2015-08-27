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
#include <time.h>
#include <termios.h>
#include <curl/curl.h>
#include "../list.h"
#include "queue.h"

struct list mods={0,0};
struct queue queue={0,0};
struct list goodvids={0,0}; // pre-approved videos
struct list badvids={0,0}; // not allowed, essentially banned
char* playing=0;
char* requester=0;
char* title=0;
time_t started=0;
int tc_client;

time_t time_with_mods=0;
time_t time_modcount;
char havemods=0;
void timemods(void)
{
  time_t now=time(0);
  if(havemods)
  {
    time_with_mods+=now-time_modcount;
  }
  time_modcount=now;
  havemods=(mods.itemcount>1); // Not counting modbot as a mod
}

struct writebuf
{
  char* buf;
  int len;
};

size_t writehttp(char* ptr, size_t size, size_t nmemb, void* x)
{
  struct writebuf* data=x;
  size*=nmemb;
  data->buf=realloc(data->buf, data->len+size+1);
  memcpy(&data->buf[data->len], ptr, size);
  data->len+=size;
  data->buf[data->len]=0;
  return size;
}

char* http_get(const char* url)
{
  CURL* curl=curl_easy_init();
  if(!curl){return 0;}
  curl_easy_setopt(curl, CURLOPT_URL, url);
  struct writebuf writebuf={0, 0};
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writehttp);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writebuf);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla Firefox");
  char err[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);
  if(curl_easy_perform(curl)){curl_easy_cleanup(curl); printf("%s\n", err); return 0;}
  curl_easy_cleanup(curl);
  return writebuf.buf; // should be free()d when no longer needed
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

void getvidinfo(const char* vid, const char* type, char* buf, char* errbuf, unsigned int len)
{
  int out[2];
  int err[2];
  pipe(out);
  pipe(err);
  if(!fork())
  {
    close(out[0]);
    close(err[0]);
    dup2(out[1], 1);
    dup2(err[1], 2);
    execlp("youtube-dl", "youtube-dl", "--default-search", "auto", type, "--", vid, (char*)0);
    perror("execlp(youtube-dl)");
    _exit(1);
  }
  wait(0);
  close(out[1]);
  close(err[1]);
  size_t r;
  // Read output
  r=read(out[0], buf, len-1);
  if(r<0){r=0;}
  while(r>0 && (buf[r-1]=='\r' || buf[r-1]=='\n')){--r;} // Strip newlines
  buf[r]=0;
  close(out[0]);
  // Read any error messages
  if(errbuf)
  {
    r=read(err[0], errbuf, len-1);
    if(r<0){r=0;}
    while(r>0 && (errbuf[r-1]=='\r' || errbuf[r-1]=='\n')){--r;} // Strip newlines
    errbuf[r]=0;
    char* newline; // No need for newlines in error messages
    while((newline=strchr(errbuf, '\n'))){newline[0]=' ';}
    while((newline=strchr(errbuf, '\r'))){newline[0]=' ';}
  }
  close(err[0]);
}

unsigned int getduration(const char* vid)
{
  char timebuf[128];
  timebuf[0]=':'; // Sacrifice 1 byte to avoid having to deal with a special case later on, where no ':' is found and we go from the start of the string, but only once
  getvidinfo(vid, "--get-duration", &timebuf[1], 0, 127);
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
  if(sep){sep[0]=0; len+=atoi(&sep[1])*60*60;}
  // Days
  sep=strrchr(timebuf, ':');
  if(sep){sep[0]=0; len+=atoi(&sep[1])*60*60*24;}
  return len;
}

unsigned int waitskip=0;
time_t waitskiptime=0;
void playnextvid()
{
  waitskip=0;
  playing=queue.items[0].video;
  requester=queue.items[0].requester;
  title=queue.items[0].title;
  say(0, "/mbs youTube %s %u %s\n", playing, queue.items[0].timeoffset*1000, queue.items[0].title);
  unsigned int duration=getduration(playing)-queue.items[0].timeoffset;
  --queue.itemcount;
  memmove(queue.items, &queue.items[1], sizeof(struct queueitem)*queue.itemcount);
  // Find out the video's length and schedule an alarm for then
  alarm(duration);
  started=time(0);
}

void playnext(int x)
{
  free(playing);
  free(requester);
  free(title);
  playing=0;
  requester=0;
  title=0;
  if(queue.itemcount<1){alarm(0); printf("Nothing more to play\n"); return;} // Nothing more to play
  if(!list_contains(&goodvids, queue.items[0].video))
  {
    if(!waitskip)
    {
      say(0, "Next video (%s, %s) is not yet approved by mods\n", queue.items[0].video, queue.items[0].title);
      unsigned int i;
      for(i=1; i<queue.itemcount; ++i)
      {
        if(list_contains(&goodvids, queue.items[i].video))
        {
          waitskip=i;
          waitskiptime=time(0)+120;
          alarm(120);
          break;
        }
      }
      return;
    }else{
      if(time(0)<waitskiptime){return;} // Not time yet
      say(0, "Skipping http://youtube.com/watch?v=%s because it is still not approved after 2 minutes\n", queue.items[0].video);
      queue_movetofront(&queue, waitskip);
      waitskip=0;
    }
  }
  playnextvid();
}

void playfor(const char* nick)
{
  if(started)
  {
    say(0, "/priv %s /mbs youTube %s %u\n", nick, playing, (time(0)-started)*1000);
  }else{
    say(0, "/priv %s /mbsp youTube %s 0\n", nick, playing); // Start paused
  }
}

int main(int argc, char** argv)
{
  // Handle arguments (-d, -l, -h additions, the rest are handled by tc_client)
  char daemon=0;
  char* logfile=0;
  char verbose=0;
  char disablelists=0;
  unsigned int i;
  for(i=1; i<argc; ++i)
  {
    if(!strcmp(argv[i], "-d") || !strcmp(argv[i], "--daemon"))
    {
      daemon=1;
      // Remove non-tc_client argument
      --argc;
      memmove(&argv[i], &argv[i+1], sizeof(char*)*(argc-i));
      argv[argc]=0;
      --i;
    }
    else if(i+1<argc && (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--log")))
    {
      logfile=argv[i+1];
      // Remove non-tc_client argument
      argc-=2;
      memmove(&argv[i], &argv[i+2], sizeof(char*)*(argc-i));
      argv[argc]=0;
      --i;
    }
    else if(!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
    {
      verbose=1;
      // Remove non-tc_client argument
      --argc;
      memmove(&argv[i], &argv[i+1], sizeof(char*)*(argc-i));
      argv[argc]=0;
      --i;
    }
    else if(!strcmp(argv[i], "--disable-lists"))
    {
      disablelists=1;
      // Remove non-tc_client argument
      --argc;
      memmove(&argv[i], &argv[i+1], sizeof(char*)*(argc-i));
      argv[argc]=0;
      --i;
    }
    else if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
    {
      printf("Additional options for modbot:\n"
             "-d/--daemon     = daemonize after startup\n"
             "-l/--log <file> = log output into <file>\n"
             "-v/--verbose    = print/log all incoming messages\n"
             "--disable-lists = disable playlists\n"
             "\n");
      execvp(strncmp(argv[0], "./", 2)?"tc_client":"./tc_client", argv);
      return 1;
    }
  }
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
    execvp(strncmp(argv[0], "./", 2)?"tc_client":"./tc_client", argv);
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
  time_t sessionstart=time(0);
  time_modcount=sessionstart;
  while(1)
  {
    if(read(out[0], &buf[len], 1)<1){break;}
    if(len<1023&&buf[len]!='\r'&&buf[len]!='\n')
    {
      ++len;
      if(len==18 && !strncmp(buf, "Account password: ", 18))
      {
        len=0;
        struct termios term;
        tcgetattr(0, &term);
        term.c_lflag&=~ECHO;
        tcsetattr(0, TCSANOW, &term);
        fprintf(stdout, "Account password: ");
        fflush(stdout);
        fgets(buf, 1024, stdin);
        write(tc_client, buf, strlen(buf));
        term.c_lflag|=ECHO;
        tcsetattr(0, TCSANOW, &term);
        printf("\n");
      }
      continue;
    }
    if(!len){continue;}
    buf[len]=0;
    if(!strncmp(buf, "Captcha: ", 9))
    {
      fprintf(stdout, "%s\nPress return to continue...", buf);
      fflush(stdout);
      fgetc(stdin);
      write(tc_client, "\n", 1);
    }
    char* esc;
    while((esc=strstr(buf, "\x1b["))) // Strip out ANSI colors
    {
      for(len=2; isdigit(esc[len])||esc[len]==';'; ++len);
      memmove(esc, &esc[len+1], strlen(&esc[len]));
    }
    len=0;
    // Note: daemonizing and setting up logging here to avoid interfering with account password entry
    if(daemon)
    {
      if(fork()){return 0;}
      daemon=0;
      if(!logfile){logfile="/dev/null";} // Prevent writing to stdout as a daemon
    }
    if(logfile)
    {
      int f=open(logfile, O_WRONLY|O_CREAT|O_APPEND, 0600);
      dup2(f, 1);
      dup2(f, 2);
      close(f);
      logfile=0;
    }
    if(verbose){printf("Got line '%s'\n", buf); fflush(stdout);}
    char* space=strchr(buf, ' ');
    if(!space){continue;}
    if(!strcmp(space, " is a moderator."))
    {
      // If there are not-yet-approved videos in the queue when a mod joins, ask them to review them
      space[0]=0;
      list_add(&mods, buf);
      timemods();
      continue;
    }
    if(!strcmp(space, " is no longer a moderator."))
    {
      space[0]=0;
      list_del(&mods, buf);
      timemods();
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
          if(disablelists)
          {
            char* x;
            if((x=strstr(&msg[9], "&list="))){x[0]=0;}
            if((x=strstr(&msg[9], "?list="))){x[1]='_';}
          }
          char title[256];
          char vid[1024];
          char viderr[1024];
          unsigned int timeoffset=0;
          char* timestr=strstr(&msg[9], "&t=");
          if(!timestr){timestr=strstr(&msg[9], "?t=");}
          if(timestr)
          {
            timestr=&timestr[3];
            while(timestr[0])
            {
              unsigned int num=strtoul(timestr, &timestr, 10);
              if(timestr[0])
              {
                if(timestr[0]=='h'){num*=3600;}
                else if(timestr[0]=='m'){num*=60;}
                timestr=&timestr[1];
              }
              timeoffset+=num;
            }
          }
          getvidinfo(&msg[9], "--get-id", vid, viderr, 1024);
          if(!vid[0]) // Nothing found
          {
            say(pm, "No video found, sorry (%s)\n", viderr);
            continue;
          }
          char* plist;
          for(plist=vid; plist[0] && plist[0]!='\r' && plist[0]!='\n'; plist=&plist[1]);
          if(plist[0]) // Link was a playlist, do some trickery to get the title of the first video (instead of getting nothing)
          {
            strcpy(title, "Playlist, starting with ");
            plist[0]=0;
            getvidinfo(vid, "--get-title", &title[24], 0, 256-24);
            plist[0]='\n';
          }else{
            plist=0;
            getvidinfo(vid, "--get-title", title, 0, 256);
          }
          printf("Requested ID '%s' by '%s'\n", vid, nick);
          // Check if it's already queued and mention which spot it's in, or if it's marked as bad and shouldn't be queued
          int pos;
          if((pos=queue_getpos(&queue, vid))>-1)
          {
            say(pm, "Video '%s' is already in queue (number %i, requested by %s)\n", title, pos, queue.items[pos].requester);
            continue;
          }
          if(list_contains(&badvids, vid))
          {
            say(pm, "Video '%s' is marked as bad, won't add to queue\n", title);
            continue;
          }
          if(!list_contains(&goodvids, vid)) // Check that this video can be embedded (for the flash client)
          {
            char url[strlen("https://www.youtube.com/watch?v=0")+strlen(vid)];
            sprintf(url, "https://www.youtube.com/watch?v=%s", vid);
            char* page=http_get(url);
            void* res=strstr(page, "youtube.com/embed/");
            free(page);
            if(!res)
            {
              say(pm, "The video I found for '%s' was not embeddable, sorry\n", &msg[9]);
              continue;
            }
          }
          if(list_contains(&mods, nick)) // Auto-approve for mods
          {
            list_add(&goodvids, vid);
            list_del(&badvids, vid);
            list_save(&goodvids, "goodvids.txt");
            list_save(&badvids, "badvids.txt");
          }

          queue_add(&queue, vid, nick, title, timeoffset);
          if(!list_contains(&goodvids, vid))
          {
            if(plist)
            {
              say(pm, "Playlist '%s' is added to the queue but will need to be approved by a mod\n", title);
            }else{
              say(pm, "Video '%s' (%s) is added to the queue but will need to be approved by a mod\n", vid, title);
            }
          }
          else if(!playing && !waitskip){playnext(0);}
          else{say(pm, "Added '%s' to queue, it has already been approved\n", title);}
        }
        // Undo
        else if(!strcmp(msg, "!wrongrequest"))
        {
          unsigned int i=queue.itemcount;
          while(i>0)
          {
            --i;
            if(!strcmp(queue.items[i].requester, nick))
            {
              say(0, "/priv %s Removing '%s' from queue\n", nick, queue.items[i].title);
              if(list_contains(&mods, nick) && goodvids.itemcount && !strcmp(goodvids.items[goodvids.itemcount-1], queue.items[i].video))
              {
                list_del(&goodvids, queue.items[i].video); // Un-approve the video since it was probably a bad search result and auto-approved because the requester is a mod
                list_save(&goodvids, "goodvids.txt");
              }
              queue_del(&queue, queue.items[i].video);
              if(!playing && i==0){playnext(0);}
              i=1; // distinguish from just having reached the front of the queue
              break;
            }
          }
          if(!i)
          {
            say(pm, "I can't find any request by you, sorry\n");
          }
        }
        else if(!strncmp(msg, "!wrongrequest ", 14))
        {
          char vid[1024];
          getvidinfo(&msg[14], "--get-id", vid, 0, 1024);
          unsigned int i;
          for(i=0; i<queue.itemcount; ++i)
          {
            if(!strcmp(queue.items[i].requester, nick) && !strcmp(queue.items[i].video, vid))
            {
              say(0, "/priv %s Removing '%s' from queue\n", nick, queue.items[i].title);
              if(list_contains(&mods, nick) && goodvids.itemcount && !strcmp(goodvids.items[goodvids.itemcount-1], queue.items[i].video))
              {
                list_del(&goodvids, queue.items[i].video); // Un-approve the video since it was probably a bad search result and auto-approved because the requester is a mod
                list_save(&goodvids, "goodvids.txt");
              }
              queue_del(&queue, queue.items[i].video);
              break;
            }
          }
          if(i==queue.itemcount)
          {
            say(pm, "I can't find that request by you, sorry\n");
          }
        }
        else if(!strcmp(msg, "!queue"))
        {
          unsigned int notapproved=0;
          unsigned int len=0;
          unsigned int i;
          for(i=0; i<queue.itemcount; ++i)
          {
            if(!list_contains(&goodvids, queue.items[i].video)){++notapproved; len+=strlen(queue.items[i].video)+strlen(queue.items[i].title)+strlen(" (), ");}
          }
          if(notapproved)
          {
            char buf[len];
            buf[0]=0;
            unsigned int listed=0;
            for(i=0; i<queue.itemcount; ++i)
            {
              if(listed<5 && !list_contains(&goodvids, queue.items[i].video))
              {
                if(buf[0]){strcat(buf, ", ");}
                strcat(buf, queue.items[i].video);
                strcat(buf, " (");
                strcat(buf, queue.items[i].title);
                strcat(buf, ")");
                ++listed;
              }
            }
            say(pm, "%u video%s in queue, %u of which are not yet approved by mods (%s%s)\n", queue.itemcount, (queue.itemcount==1)?"":"s", notapproved, buf, (listed<notapproved)?", etc.":"");
          }else{
            say(pm, "%u video%s in queue\n", queue.itemcount, (queue.itemcount==1)?"":"s");
          }
        }
        else if(!strncmp(msg, "!queue ", 7))
        {
          unsigned int i=atoi(&msg[7]);
          if(i>=queue.itemcount){say(pm, "%u is beyond the size of the queue\n", i); continue;}
          say(pm, "%s by %s (%s)\n", queue.items[i].video, queue.items[i].requester, queue.items[i].title);
        }
        else if(!strcmp(msg, "!requestedby"))
        {
          if(!playing){say(pm, "Nothing is playing\n");}
          else{say(pm, "%s requested %s (%s)\n", requester, playing, title);}
        }
        else if(!strcmp(msg, "!time")) // Debugging
        {
          unsigned int remaining=alarm(0);
          alarm(remaining);
          say(pm, "'%s' is scheduled to end in %u seconds\n", playing, remaining);
        }
        else if(!strcmp(msg, "!help"))
        {
          say(pm, "http://tc_client.ion.nu/misc/modbotcommands.html\n");
        }
        else if(!strcmp(msg, "!modstats"))
        {
          unsigned int session=time(0)-sessionstart;
          timemods();
          unsigned int hasmods=time_with_mods*100/session;
          const char* timeformat="seconds";
          if(session>=120)
          {
            session/=60;
            timeformat="minutes";
            if(session>=120)
            {
              session/=60;
              timeformat="hours";
              if(session>=48)
              {
                session/=24;
                timeformat="days";
              }
            }
          }
          say(pm, "The channel has had mods %u%% of the time for the past %u %s\n", hasmods, session, timeformat);
        }
        else if(!strcmp(msg, "!syncvid"))
        {
          if(playing)
          {
            space[0]=0;
            playfor(nick);
          }else{
            say(pm, "Nothing is playing\n");
          }
        }
        else if(!strcmp(msg, "!nowplaying"))
        {
          if(!playing){say(pm, "Nothing is playing\n");}
          else{say(pm, "Currently playing: %s (http://youtube.com/watch?v=%s )\n", title, playing);}
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
              if(list_contains(&goodvids, playing) && !list_contains(&badvids, playing)){say(pm, "'%s' is approved, use !approve <ID> to approve another video (or 'next' instead of an ID to approve the next not-yet-approved video in queue)\n", playing); continue;}
              list_add(&goodvids, playing);
              list_del(&badvids, playing);
              list_save(&goodvids, "goodvids.txt");
              list_save(&badvids, "badvids.txt");
            }else if(queue.itemcount>0){
              if(list_contains(&goodvids, queue.items[0].video) && !list_contains(&badvids, queue.items[0].video)){say(pm, "'%s' is approved, use !approve <ID> to approve another video\n", queue.items[0].video); continue;}
              list_add(&goodvids, queue.items[0].video);
              list_del(&badvids, queue.items[0].video);
              list_save(&goodvids, "goodvids.txt");
              list_save(&badvids, "badvids.txt");
              playnext(0);
            }else{say(pm, "Please specify a video to approve\n");}
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
                if(!list_contains(&goodvids, queue.items[i].video))
                {
                  vid=queue.items[i].video;
                  say(pm, "Approved '%s'\n", queue.items[i].title);
                  break;
                }
              }
              if(i==queue.itemcount){say(pm, "Nothing more to approve :)\n"); continue;}
            }
            else if(!strcmp(vid, "entire queue"))
            {
              char approved=0;
              unsigned int i;
              for(i=0; i<queue.itemcount; ++i)
              {
                if(list_contains(&goodvids, queue.items[i].video)){continue;}
                list_add(&goodvids, queue.items[i].video);
                approved=1;
              }
              if(approved)
              {
                list_save(&goodvids, "goodvids.txt");
                if(!playing){playnext(0);} // Next in queue just got approved, so play it
                else{say(pm, "Queue approved. Make sure none of the videos were inappropriate\n");}
              }else{
                say(pm, "The queue is already approved (or empty)\n");
              }
              continue;
            }else{
              getvidinfo(vid, "--get-id", vidbuf, 0, 256);
              vid=vidbuf;
            }
            list_add(&goodvids, vid);
            list_del(&badvids, vid);
            list_save(&goodvids, "goodvids.txt");
            list_save(&badvids, "badvids.txt");
            if(!playing && queue.itemcount>0 && !strcmp(vid, queue.items[0].video)){playnext(0);} // Next in queue just got approved, so play it
          }
          else if(!strcmp(msg, "!badvid") || !strcmp(msg, "!badvideo") || !strncmp(msg, "!badvid ", 8) || !strncmp(msg, "!badvideo ", 10))
          {
            char* space=strchr(msg, ' ');
            if((!space || !space[1]) && !playing){say(pm, "Nothing is playing, please use !badvid <URL/ID> instead\n"); continue;}
            char vid[1024];
            if(space && space[1])
            {
              getvidinfo(&space[1], "--get-id", vid, 0, 256);
            }else{strncpy(vid, playing, 1023); vid[1023]=0;}
            if(!vid[0]){say(pm, "Video not found, sorry\n"); continue;}
            queue_del(&queue, vid);
            list_del(&goodvids, vid);
            list_add(&badvids, vid);
            list_save(&goodvids, "goodvids.txt");
            list_save(&badvids, "badvids.txt");
            say(pm, "Marked '%s' as bad, it will not be allowed into the queue again. You can reverse this by !approve'ing the video by ID/link/name\n", vid);
            if(playing && !strcmp(vid, playing))
            {
              say(0, "/mbc youTube\n");
              playnext(0);
            }
          }
          else if(!strcmp(msg, "!skip") || !strncmp(msg, "!skip ", 6))
          {
            unsigned int num=((msg[5]&&msg[6])?strtoul(&msg[6], 0, 0):1);
            if(num<1){say(pm, "The given value evaluates to 0, please specify the number of videos you would like to skip (or if you do not specify it will default to 1)\n"); continue;}
            if(playing){--num; say(0, "/mbc youTube\n");}
            say(0, "/priv %s Skipping %u videos. Note that these videos are not marked as bad (!badvid). Videos: %s", nick, num+(!!playing), playing?playing:"");
            char first=!playing;
            while(num>0&&queue.itemcount>0)
            {
              say(0, "%s%s", first?"":", ", queue.items[0].video);
              first=0;
              queue_del(&queue, queue.items[0].video);
              --num;
            }
            say(0, "\n");
            free(playing);
            playing=0;
            playnext(0);
          }
          else if(!strncmp(msg, "/mbs youTube ", 13))
          {
            // Someone manually started a video, mark that video as good, remove it from queue, and set an alarm for when it's modbot's turn to play stuff again
            char* vid=&msg[13];
            char* end=strchr(vid, ' ');
            if(end){end[0]=0;}
            queue_del(&queue, vid);
            list_add(&goodvids, vid);
            list_save(&goodvids, "goodvids.txt");
            free(playing);
            playing=strdup(vid);
            free(requester);
            requester=strdup(nick);
            unsigned int pos=(end?(strtol(&end[1], 0, 0)/1000):0);
            alarm(getduration(playing)-pos);
            started=time(0)-pos;
          }
          else if(!strcmp(msg, "/mbc youTube") && playing) // Video cancelled
          {
            list_del(&goodvids, playing); // manual /mbc is often used when !badvid should be used, so at least remove it from the list of approved videos
            list_save(&goodvids, "goodvids.txt");
            playnext(0);
          }
          else if((!strncmp(msg, "/mbsk youTube ", 14) || !strncmp(msg, "/mbpl youTube ", 14)) && playing) // Seeking, or resume playing
          {
            unsigned int pos=strtol(&msg[14], 0, 0)/1000;
            alarm(getduration(playing)-pos);
            started=time(0)-pos;
          }
          else if(!strcmp(msg, "/mbpa youTube")) // Pause
          {
            alarm(0);
            started=0;
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
              if(!list_contains(&goodvids, queue.items[i].video))
              {
                say(nick, "there are 1 or more videos in queue that are not yet approved, please type !queue to review them\n");
                break;
              }
            }
          }
          continue;
        }
        else if(!strcmp(space, " entered the channel")) // Newcomer, inform about the currently playing video
        {
          if(playing)
          {
            space[0]=0;
            playfor(nick);
          }
        }
        else if(!strcmp(space, " left the channel"))
        {
          space[0]=0;
          list_del(&mods, nick); // Absent people can't be mods
          timemods();
        }
      }
    }
  }
  return 0;
}
