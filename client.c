/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2014-2016  alicia@ion.nu
    Copyright (C) 2014-2015  Jade Lea
    Copyright (C) 2015  Pamela Hiatt

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
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <locale.h>
#include <ctype.h>
#include <termios.h>
#include <curl/curl.h>
#include "numlist.h"
#include "idlist.h"
#include "colors.h"
#include "media.h"
#include "amfwriter.h"
#include "utilities/compat.h"
#include "client.h"

struct command
{
  const char* command;
  void(*callback)(struct amf*,int);
  unsigned int minargs;
};
static struct command* commands=0;
static unsigned int commandcount=0;

char greenroom=0;
char* channel=0;
char* nickname=0;

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

const char* cookiefile="";
char* http_get(const char* url, const char* post)
{
  static CURL* curl=0;
  if(!curl){curl=curl_easy_init();}
  if(!curl){return 0;}
  curl_easy_setopt(curl, CURLOPT_URL, url);
  struct writebuf writebuf={0, 0};
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writehttp);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writebuf);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookiefile);
  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookiefile);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla Firefox");
  if(post){curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);}
  char err[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);
  if(curl_easy_perform(curl)){curl_easy_cleanup(curl); printf("%s\n", err); return 0;}
  return writebuf.buf; // should be free()d when no longer needed
}

char timestampbuf[8];
char* timestamp()
{
  // Timestamps, e.g. "[hh:mm] name: message"
  time_t timestamp=time(0);
  struct tm* t=localtime(&timestamp);
  sprintf(timestampbuf, "[%02i:%02i]", t->tm_hour, t->tm_min);
  return timestampbuf;
}

int connectto(const char* host, const char* port)
{
  struct addrinfo* res;
  getaddrinfo(host, port, 0, &res);
  int sock=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(connect(sock, res->ai_addr, res->ai_addrlen))
  {
    perror("Failed to connect");
    freeaddrinfo(res);
    return 1;
  }
  freeaddrinfo(res);
  int i=1;
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &i, sizeof(i));
  return sock;
}

void registercmd(const char* command, void(*callback)(struct amf*,int), unsigned int minargs)
{
  ++commandcount;
  commands=realloc(commands, sizeof(struct command)*commandcount);
  commands[commandcount-1].command=strdup(command);
  commands[commandcount-1].callback=callback;
  commands[commandcount-1].minargs=minargs;
}

static void usage(const char* me)
{
  printf("Usage: %s [options] <channelname> <nickname> [channelpassword]\n"
         "Options include:\n"
         "-h, --help           Show this help text and exit.\n"
         "-v, --version        Show the program version and exit.\n"
         "-u, --user <user>    Username of tinychat account to use.\n"
         "-p, --pass <pass>    Password of tinychat account to use.\n"
         "-c, --color <value>  Color to use in chat.\n"
         "-s, --site <site>    Site to connect to.\n"
         "    --hexcolors      Print hex colors instead of ANSI color codes.\n"
         "    --cookies <file> File to store cookies in (not having to solve\n"
         "                       the captchas every time)\n"
         "    --greenroom      Join a channel's greenroom.\n"
#ifdef RTMP_DEBUG
         "    --rtmplog <file> Write RTMP input to file.\n"
#endif
         ,me);
}
#ifdef RTMP_DEBUG
extern int rtmplog;
#endif

extern int init_tinychat(const char* chanpass, const char* username, const char* userpass, struct site* site);
extern int init_kageshi(const char* chanpass, const char* username, const char* userpass, struct site* site);
extern int init_kageshicam(struct site* site);
int main(int argc, char** argv)
{
  char* password=0;
  char* account_user=0;
  char* account_pass=0;
  const char* sitestr=0;
  int i;
  for(i=1; i<argc; ++i)
  {
    if(!strcmp(argv[i], "-h")||!strcmp(argv[i], "--help")){usage(argv[0]); return 0;}
    if(!strcmp(argv[i], "-v")||!strcmp(argv[i], "--version")){printf("tc_client-"VERSION"\n"); return 0;}
    else if(!strcmp(argv[i], "-u")||!strcmp(argv[i], "--user"))
    {
      if(i+1==argc){continue;}
      ++i;
      account_user=argv[i];
      unsigned int j;
      for(j=0; account_user[j]; ++j){account_user[j]=tolower(account_user[j]);}
    }
    else if(!strcmp(argv[i], "-p")||!strcmp(argv[i], "--pass"))
    {
      if(i+1==argc){continue;}
      ++i;
      account_pass=strdup(argv[i]);
    }
    else if(!strcmp(argv[i], "-c")||!strcmp(argv[i], "--color"))
    {
      ++i;
      currentcolor=atoi(argv[i]);
    }
    else if(!strcmp(argv[i], "-s")||!strcmp(argv[i], "--site"))
    {
      ++i;
      sitestr=argv[i];
    }
    else if(!strcmp(argv[i], "--cookies"))
    {
      ++i;
      cookiefile=argv[i];
    }
#ifdef RTMP_DEBUG
    else if(!strcmp(argv[i], "--rtmplog")){++i; rtmplog=open(argv[i], O_WRONLY|O_CREAT|O_TRUNC, 0600); if(rtmplog<0){perror("rtmplog: open");}}
#endif
    else if(!strcmp(argv[i], "--hexcolors")){hexcolors=1;}
    else if(!strcmp(argv[i], "--greenroom")){greenroom=1;}
    else if(!channel){channel=argv[i];}
    else if(!nickname){nickname=strdup(argv[i]);}
    else if(!password){password=argv[i];}
  }
  // Check for required arguments
  if(!channel||!nickname){usage(argv[0]); return 1;}
  if(sitestr &&
     strcmp(sitestr, "tinychat") &&
     strcmp(sitestr, "kageshi") &&
     strcmp(sitestr, "kageshicam"))
  {
    printf("Unknown site '%s'. Currently supported sites:\n"
           "tinychat\n"
           "kageshi\n"
           "kageshicam (server/camname/numkey as channel)\n", sitestr);
    return 1;
  }
  setlocale(LC_ALL, "");
  if(account_user && !account_pass) // Only username given, prompt for password
  {
    struct termios term;
    tcgetattr(0, &term);
    term.c_lflag&=~ECHO;
    tcsetattr(0, TCSANOW, &term);
    fprintf(stdout, "Account password: ");
    fflush(stdout);
    account_pass=malloc(128);
    fgets(account_pass, 128, stdin);
    term.c_lflag|=ECHO;
    tcsetattr(0, TCSANOW, &term);
    printf("\n");
    unsigned int i;
    for(i=0; account_pass[i]; ++i)
    {
      if(account_pass[i]=='\n'||account_pass[i]=='\r'){account_pass[i]=0; break;}
    }
  }

  // Accept URL as "channel" and pick site accordingly (in addition to --site)
  char* domain;
  if((domain=strstr(channel, "://")))
  {
    domain=&domain[3];
    if(!strncmp(domain, "www.", 4)){domain=&domain[4];}
    if(!strncmp(domain, "tinychat.com/", 13))
    {
      sitestr="tinychat";
      channel=&domain[13];
    }
    else if(!strncmp(domain, "kageshi.com/rooms/", 18))
    {
      sitestr="kageshi";
      channel=&domain[18];
    }
  }
  struct site site;
  int sock;
  if(!sitestr || !strcmp(sitestr, "tinychat"))
  {
    sock=init_tinychat(password, account_user, account_pass, &site);
  }
  else if(!strcmp(sitestr, "kageshi"))
  {
    sock=init_kageshi(password, account_user, account_pass, &site);
  }
  else if(!strcmp(sitestr, "kageshicam"))
  {
    sock=init_kageshicam(&site);
  }
  free(account_pass);
  int random=open("/dev/urandom", O_RDONLY);
  if(currentcolor>=COLORCOUNT)
  {
    read(random, &currentcolor, sizeof(currentcolor));
  }
  close(random);
  struct pollfd pfd[2];
  pfd[0].fd=0;
  pfd[0].events=POLLIN;
  pfd[0].revents=0;
  pfd[1].fd=sock;
  pfd[1].events=POLLIN;
  pfd[1].revents=0;
  struct rtmp rtmp={0,0,0,0,0};
  while(1)
  {
    // Poll for input, very crude chat UI
    poll(pfd, 2, -1);
    if(pfd[0].revents) // Got input, send a privmsg command
    {
      pfd[0].revents=0;
      char buf[2048];
      unsigned int len=0;
      int r;
      while(len<2047)
      {
        if((r=read(0, &buf[len], 1))!=1 || buf[len]=='\r' || buf[len]=='\n'){break;}
        ++len;
      }
      if(r<1){break;}
      if(len<1){continue;}
      while(len>0 && (buf[len-1]=='\n'||buf[len-1]=='\r')){--len;}
      if(!len){continue;} // Don't send empty lines
      buf[len]=0;
      if(buf[0]=='/') // Got a command
      {
        if(!strcmp(buf, "/help"))
        {
          printf("/help           = print this help text\n"
                 "/color <0-15>   = pick color of your messages\n"
                 "/color <on/off> = turn on/off showing others' colors with ANSI codes\n"
                 "/color          = see your current color\n"
                 "/colors         = list the available colors and their numbers\n"
                 "/nick <newnick> = changes your nickname\n"
                 "/msg <to> <msg> = send a private message\n"
                 "/opencam <nick> = see someone's cam/mic (Warning: writes binary data to stdout)\n"
                 "/closecam <nick> = stop receiving someone's cam stream\n"
                 "/close <nick>   = close someone's cam/mic stream (as a mod)\n"
                 "/ban <nick>     = ban someone\n"
                 "/banlist        = list who is banned\n"
                 "/forgive <nick/ID> = unban someone\n"
                 "/names          = list everyone that is online\n"
                 "/mute           = temporarily mutes all non-moderator broadcasts\n"
                 "/push2talk      = puts all non-operators in push2talk mode\n"
                 "/camup          = open an audio/video stream for broadcasting your video\n"
                 "/camdown        = close the broadcasting stream\n"
                 "/video <length> = send a <length> bytes long encoded frame, send the frame data after this line\n"
                 "/audio <length> = send a <length> bytes long encoded frame, send the frame data after this line\n"
                 "/topic <topic>  = set the channel topic\n"
                 "/whois <nick/ID> = check a user's username\n"
                 "/disablesnapshots = disable flash client's snapshots of our stream\n"
                 "/enablesnapshots = re-enable flash client's snapshots of our stream\n"
                 "/allow <nick>   = allow user to broadcast\n"
                 "/getnick <ID>   = get nickname from connection ID\n");
          fflush(stdout);
          continue;
        }
        else if(!strncmp(buf, "/color", 6) && (!buf[6]||buf[6]==' '))
        {
          if(buf[6]) // Color specified
          {
            if(!strcmp(&buf[7], "off")){showcolor=0; continue;}
            if(!strcmp(&buf[7], "on")){showcolor=1; continue;}
            currentcolor=atoi(&buf[7]);
            printf("%sChanged color%s\n", color_start(colors[currentcolor%COLORCOUNT]), color_end());
          }else{ // No color specified, state our current color
            printf("%sCurrent color: %i%s\n", color_start(colors[currentcolor%COLORCOUNT]), currentcolor%COLORCOUNT, color_end());
          }
          fflush(stdout);
          continue;
        }
        else if(!strcmp(buf, "/colors"))
        {
          int i;
          for(i=0; i<COLORCOUNT; ++i)
          {
            printf("%sColor %i%s\n", color_start(colors[i]), i, color_end());
          }
          fflush(stdout);
          continue;
        }
        else if(!strncmp(buf, "/nick ", 6))
        {
          site.nick(sock, &buf[6]);
          continue;
        }
        else if(!strncmp(buf, "/msg ", 5))
        {
          char* msg=strchr(&buf[5], ' ');
          if(msg)
          {
            msg[0]=0;
            site.sendpm(sock, &msg[1], &buf[5]);
            continue;
          }
        }
        else if(!strncmp(buf, "/opencam ", 9))
        {
          site.opencam(sock, &buf[9]);
          continue;
        }
        else if(!strncmp(buf, "/closecam ", 10))
        {
          site.closecam(sock, &buf[10]);
          continue;
        }
        else if(!strncmp(buf, "/close ", 7)) // Stop someone's cam/mic broadcast
        {
          site.mod_close(sock, &buf[7]);
          continue;
        }
        else if(!strncmp(buf, "/ban ", 5)) // Ban someone
        {
          site.mod_ban(sock, &buf[5]);
          continue;
        }
        else if(!strcmp(buf, "/banlist"))
        {
          site.mod_banlist(sock);
          continue;
        }
        else if(!strncmp(buf, "/forgive ", 9))
        {
          site.mod_unban(sock, &buf[9]);
          continue;
        }
        else if(!strcmp(buf, "/names"))
        {
          printf("Currently online: ");
          for(i=0; i<idlistlen; ++i)
          {
            printf("%s%s", (i?", ":""), idlist[i].name);
          }
          printf("\n");
          fflush(stdout);
          continue;
        }
        else if(!strcmp(buf, "/mute"))
        {
          site.mod_mute(sock);
          continue;
        }
        else if(!strcmp(buf, "/push2talk"))
        {
          site.mod_push2talk(sock);
          continue;
        }
        else if(!strcmp(buf, "/camup"))
        {
          site.camup(sock);
          continue;
        }
        else if(!strcmp(buf, "/camdown"))
        {
          site.camdown(sock);
          continue;
        }
        else if(!strncmp(buf, "/video ", 7)) // Send video data
        {
          size_t len=strtoul(&buf[7],0,0);
          char buf[len];
          fullread(0, buf, len);
          stream_sendframe(sock, buf, len, RTMP_VIDEO);
          continue;
        }
        else if(!strncmp(buf, "/audio ", 7)) // Send audio data
        {
          size_t len=strtoul(&buf[7],0,0);
          char buf[len];
          fullread(0, buf, len);
          stream_sendframe(sock, buf, len, RTMP_AUDIO);
          continue;
        }
        else if(!strncmp(buf, "/topic ", 7))
        {
          site.mod_topic(sock, &buf[7]);
          continue;
        }
        else if(!strncmp(buf, "/whois ", 7)) // Get account username
        {
          const char* account=idlist_getaccount(&buf[7]);
          if(account)
          {
            printf("%s is logged in as %s\n", &buf[7], account);
          }else{
            printf("%s is not logged in\n", &buf[7]);
          }
          fflush(stdout);
          continue;
        }
        else if(!strcmp(buf, "/disablesnapshots") || !strcmp(buf, "/enablesnapshots"))
        {
          setallowsnapshots(sock, buf[1]=='e'); // True for "/enablesnapshots", false for "/disablesnapshots"
          continue;
        }
        else if(!strncmp(buf, "/allow ", 7))
        {
          site.mod_allowbroadcast(sock, &buf[7]);
          continue;
        }
        else if(!strncmp(buf, "/getnick ", 9))
        {
          int id=atoi(&buf[9]);
          const char* nick=idlist_getnick(id);
          if(nick){printf("Nickname of connection %i: %s\n", id, nick); fflush(stdout);}
          continue;
        }
      }
      site.sendmessage(sock, buf);
      continue;
    }
    // Got data from server
    pfd[1].revents=0;
    // Read the RTMP stream and handle AMF0 packets
    char rtmpres=rtmp_get(sock, &rtmp);
    if(!rtmpres){printf("Server disconnected\n"); break;}
    if(rtmpres==2){continue;} // Not disconnected, but we didn't get a complete chunk yet either
    if(rtmp.type==RTMP_VIDEO || rtmp.type==RTMP_AUDIO){stream_handledata(&rtmp); continue;}
    if(rtmp.type!=RTMP_AMF0){printf("Got RTMP type 0x%x\n", rtmp.type); fflush(stdout); continue;}
    struct amf* amfin=amf_parse(rtmp.buf, rtmp.length);
    for(i=0; i<commandcount; ++i)
    {
      if(amfin->itemcount>=commands[i].minargs && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, commands[i].command))
      {
        commands[i].callback(amfin, sock);
        fflush(stdout);
        break;
      }
    }
    // if(i==commandcount){printf("Unknown command...\n"); printamf(amfin);} // (Debugging)
    amf_free(amfin);
  }
  free(rtmp.buf);
  close(sock);
  return 0;
}
