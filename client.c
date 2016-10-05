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

struct writebuf
{
  char* buf;
  int len;
};

void b_read(int sock, void* buf, size_t len)
{
  while(len>0)
  {
    size_t r=read(sock, buf, len);
    if(r<1){return;}
    len-=r;
    buf+=r;
  }
}

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

CURL* curl=0;
const char* cookiefile="";
char* http_get(const char* url, const char* post)
{
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

char* gethost(char *channel, char *password)
{
  int urllen;
  if(password)
  {
    urllen=strlen("http://apl.tinychat.com/api/find.room/?site=tinychat&password=0")+strlen(channel)+strlen(password);
  }else{
    urllen=strlen("http://apl.tinychat.com/api/find.room/?site=tinychat0")+strlen(channel);
  }
  char url[urllen];
  if(password)
  {
    sprintf(url, "http://apl.tinychat.com/api/find.room/%s?site=tinychat&password=%s", channel, password);
  }else{
    sprintf(url, "http://apl.tinychat.com/api/find.room/%s?site=tinychat", channel);
  }
  char* response=http_get(url, 0);
  if(!response){exit(-1);}
  //response contains result='(OK|RES)|PW' (latter means a password is required)
  char* result=strstr(response, "result='");
  if(!result){printf("No result\n"); exit(-1);}
  result+=strlen("result='");
  // Handle the result value
  if(!strncmp(result, "PW", 2)){printf("Password required\n"); exit(-1);}
  if(strncmp(result, "OK", 2) && strncmp(result, "RES", 3)){printf("Result not OK\n"); exit(-1);}
  // Find and extract the server responsible for this channel
  char* rtmp=strstr(response, "rtmp='rtmp://");
  if(!rtmp){printf("No rtmp found.\n"); exit(-1);}
  rtmp+=strlen("rtmp='rtmp://");
  int len;
  for(len=0; rtmp[len] && rtmp[len]!='/'; ++len);
  char* host=strndup(rtmp, len);
  free(response);
  return host;
}

char* getkey(int id, const char* channel)
{
  char url[snprintf(0,0, "http://apl.tinychat.com/api/captcha/check.php?guest%%5Fid=%i&room=tinychat%%5E%s", id, channel)+1];
  sprintf(url, "http://apl.tinychat.com/api/captcha/check.php?guest%%5Fid=%i&room=tinychat%%5E%s", id, channel);
  char* response=http_get(url, 0);
  char* key=strstr(response, "\"key\":\"");

  if(!key){return 0;}
  key+=7;
  char* keyend=strchr(key, '"');
  if(!keyend){return 0;}
  char* backslash;
  while((backslash=strchr(key, '\\')))
  {
    memmove(backslash, &backslash[1], strlen(backslash));
    --keyend;
  }
  key=strndup(key, keyend-key);
  free(response);
  return key;
}

char* getcookie(const char* channel)
{
  unsigned long long now=time(0);
  char url[strlen("http://tinychat.com/cauth?t=&room=0")+snprintf(0,0, "%llu", now)+strlen(channel)];
  sprintf(url, "http://tinychat.com/cauth?t=%llu&room=%s", now, channel);
  char* response=http_get(url, 0);
  char* cookie=strstr(response, "\"cookie\":\"");

  if(!cookie){return 0;}
  cookie+=10;
  char* end=strchr(cookie, '"');
  if(!end){return 0;}
  cookie=strndup(cookie, end-cookie);
  free(response);
  return cookie;
}

char* getbroadcastkey(const char* channel, const char* nick)
{
  unsigned int id=idlist_get(nick);
  char url[strlen("http://apl.tinychat.com/api/broadcast.pw?name=&site=tinychat&nick=&id=0")+128+strlen(channel)+strlen(nick)];
  sprintf(url, "http://apl.tinychat.com/api/broadcast.pw?name=%s&site=tinychat&nick=%s&id=%u", channel, nick, id);
  char* response=http_get(url, 0);
  char* key=strstr(response, " token='");

  if(!key){return 0;}
  key+=8;
  char* keyend=strchr(key, '\'');
  if(!keyend){return 0;}
  key=strndup(key, keyend-key);
  free(response);
  return key;
}

char* getmodkey(const char* user, const char* pass, const char* channel, char* loggedin)
{
  // TODO: if possible, do this in a neater way than digging the key out from an HTML page.
  if(!user||!pass){return 0;}
  char post[strlen("form_sent=1&username=&password=&next=http://tinychat.com/0")+strlen(user)+strlen(pass)+strlen(channel)];
  sprintf(post, "form_sent=1&username=%s&password=%s&next=http://tinychat.com/%s", user, pass, channel);
  char* response=http_get("http://tinychat.com/login", post);
  char* key=strstr(response, "autoop: \"");
  if(key)
  {
    key=&key[9];
    char* end=strchr(key, '"');
    if(end)
    {
      end[0]=0;
      key=strdup(key);
    }else{key=0;}
  }
  if(strstr(response, "<div class='name'")){*loggedin=1;}
  free(response);
  return key;
}

void getcaptcha(void)
{
  char* url="http://tinychat.com/cauth/captcha";
  char* page=http_get(url, 0);
  char* token=strstr(page, "\"token\":\"");
  if(token)
  {
    token=&token[9];
    char* end=strchr(token, '"');
    if(end)
    {
      end[0]=0;
      printf("Captcha: http://tinychat.com/cauth/recaptcha?token=%s\n", token);
      fflush(stdout);
      fgetc(stdin);
    }
  }
  free(page);
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

char checknick(const char* nick) // Returns zero if the nick is valid, otherwise returning the character that failed the check
{
  unsigned int i;
  for(i=0; nick[i]; ++i)
  {
    if(!strchr("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-^[]{}`\\|", nick[i])){return nick[i];}
  }
  return 0;
}

char* getprivfield(char* nick)
{
  unsigned int id;
  unsigned int privlen;
  for(privlen=0; nick[privlen]&&nick[privlen]!=' '; ++privlen);
  id=idlist_get(nick);
  if(id<0)
  {
    nick[privlen]=0;
    printf("No such nick: %s\n", nick);
    fflush(stdout);
    return 0;
  }
  char* priv=malloc(snprintf(0, 0, "n%i-", id)+privlen+1);
  sprintf(priv, "n%i-", id); // 'n' for not-broadcasting
  strncat(priv, nick, privlen);
  return priv;
}

void usage(const char* me)
{
  printf("Usage: %s [options] <channelname> <nickname> [channelpassword]\n"
         "Options include:\n"
         "-h, --help           Show this help text and exit\n"
         "-v, --version        Show the program version and exit.\n"
         "-u, --user <user>    Username of tinychat account to use.\n"
         "-p, --pass <pass>    Password of tinychat account to use.\n"
         "-c, --color <value>  Color to use in chat.\n"
         "    --hexcolors      Print hex colors instead of ANSI color codes.\n"
         "    --cookies <file> File to store cookies in (not having to solve\n"
         "                       the captchas every time)\n"
#ifdef RTMP_DEBUG
         "    --rtmplog <file> Write RTMP input to file.\n"
#endif
         ,me);
}
#ifdef RTMP_DEBUG
extern int rtmplog;
#endif

int main(int argc, char** argv)
{
  char* channel=0;
  char* nickname=0;
  char* password=0;
  char* account_user=0;
  char* account_pass=0;
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
    else if(!strcmp(argv[i], "--cookies"))
    {
      ++i;
      cookiefile=argv[i];
    }
#ifdef RTMP_DEBUG
    else if(!strcmp(argv[i], "--rtmplog")){++i; rtmplog=open(argv[i], O_WRONLY|O_CREAT|O_TRUNC, 0600); if(rtmplog<0){perror("rtmplog: open");}}
#endif
    else if(!strcmp(argv[i], "--hexcolors")){hexcolors=1;}
    else if(!channel){channel=argv[i];}
    else if(!nickname){nickname=strdup(argv[i]);}
    else if(!password){password=argv[i];}
  }
  // Check for required arguments
  if(!channel||!nickname){usage(argv[0]); return 1;}
  char badchar;
  if((badchar=checknick(nickname)))
  {
    printf("'%c' is not allowed in nicknames.\n", badchar);
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
  char* server=gethost(channel, password);
  struct addrinfo* res;
  // Separate IP/domain and port
  char* port=strchr(server, ':');
  if(!port){return 3;}
  port[0]=0;
  ++port;
  // Connect
  getaddrinfo(server, port, 0, &res);
  int sock=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(connect(sock, res->ai_addr, res->ai_addrlen))
  {
    perror("Failed to connect");
    freeaddrinfo(res);
    return 1;
  }
  freeaddrinfo(res);
  i=1;
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &i, sizeof(i));
  int random=open("/dev/urandom", O_RDONLY);
  // RTMP handshake
  unsigned char handshake[1536];
  read(random, handshake, 1536);
  if(currentcolor>=COLORCOUNT)
  {
    read(random, &currentcolor, sizeof(currentcolor));
  }
  close(random);
  write(sock, "\x03", 1); // Send 0x03 and 1536 bytes of random junk
  write(sock, handshake, 1536);
  read(sock, handshake, 1); // Receive server's 0x03+junk
  b_read(sock, handshake, 1536);
  write(sock, handshake, 1536); // Send server's junk back
  b_read(sock, handshake, 1536); // Read our junk back, we don't bother checking that it's the same
  printf("Handshake complete\n");
  struct rtmp rtmp={0,0,0,0,0};
  // Handshake complete, log in (if user account is specified)
  char loggedin=0;
  char* modkey=getmodkey(account_user, account_pass, channel, &loggedin);
  if(!loggedin){free(account_pass); account_user=0; account_pass=0;}
  getcaptcha();
  char* cookie=getcookie(channel);
  // Send connect request
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "connect");
  amfnum(&amf, 0);
  amfobjstart(&amf);
    amfobjitem(&amf, "app");
    amfstring(&amf, "tinyconf");

    amfobjitem(&amf, "flashVer");
    amfstring(&amf, "no flash");

    amfobjitem(&amf, "swfUrl");
    amfstring(&amf, "no flash");

    amfobjitem(&amf, "tcUrl");
    char str[strlen("rtmp://:/tinyconf0")+strlen(server)+strlen(port)];
    sprintf(str, "rtmp://%s:%s/tinyconf", server, port);
    amfstring(&amf, str);

    amfobjitem(&amf, "fpad");
    amfbool(&amf, 0);

    amfobjitem(&amf, "capabilities");
    amfnum(&amf, 0);

    amfobjitem(&amf, "audioCodecs");
    amfnum(&amf, 0);

    amfobjitem(&amf, "videoCodecs");
    amfnum(&amf, 0);

    amfobjitem(&amf, "videoFunction");
    amfnum(&amf, 0);

    amfobjitem(&amf, "pageUrl");
    char pageurl[strlen("http://tinychat.com/0") + strlen(channel)];
    sprintf(pageurl, "http://tinychat.com/%s", channel);
    amfstring(&amf, pageurl);

    amfobjitem(&amf, "objectEncoding");
    amfnum(&amf, 0);
  amfobjend(&amf);
  amfobjstart(&amf);
    amfobjitem(&amf, "version");
    amfstring(&amf, "Desktop");

    amfobjitem(&amf, "type");
    amfstring(&amf, "default"); // This item is called roomtype in the same HTTP response that gives us the server (IP+port) to connect to, but "default" seems to work fine too.

    amfobjitem(&amf, "account");
    amfstring(&amf, account_user?account_user:"");

    amfobjitem(&amf, "prefix");
    amfstring(&amf, "tinychat");

    amfobjitem(&amf, "room");
    amfstring(&amf, channel);

    if(modkey)
    {
      amfobjitem(&amf, "autoop");
      amfstring(&amf, modkey);
    }
    amfobjitem(&amf, "cookie");
    amfstring(&amf, cookie);
  amfobjend(&amf);
  amfsend(&amf, sock);
  free(modkey);
  free(cookie);

  char* unban=0;
  struct pollfd pfd[2];
  pfd[0].fd=0;
  pfd[0].events=POLLIN;
  pfd[0].revents=0;
  pfd[1].fd=sock;
  pfd[1].events=POLLIN;
  pfd[1].revents=0;
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
      char* privfield=0;
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
                 "/topic <topic>  = set the channel topic\n"
                 "/whois <nick/ID> = check a user's username\n");
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
          if((badchar=checknick(&buf[6])))
          {
            printf("'%c' is not allowed in nicknames.\n", badchar);
            continue;
          }
          amfinit(&amf, 3);
          amfstring(&amf, "nick");
          amfnum(&amf, 0);
          amfnull(&amf);
          amfstring(&amf, &buf[6]);
          amfsend(&amf, sock);
          continue;
        }
        else if(!strncmp(buf, "/msg ", 5))
        {
          privfield=getprivfield(&buf[5]);
          if(!privfield){continue;}
        }
        else if(!strncmp(buf, "/priv ", 6))
        {
          char* end=strchr(&buf[6], ' ');
          if(!end){continue;}
          privfield=getprivfield(&buf[6]);
          if(!privfield){continue;}
          len=strlen(&end[1]);
          memmove(buf, &end[1], len+1);
        }
        else if(!strncmp(buf, "/opencam ", 9))
        {
          stream_start(&buf[9], sock);
          continue;
        }
        else if(!strncmp(buf, "/closecam ", 10))
        {
          stream_stopvideo(sock, idlist_get(&buf[10]));
          continue;
        }
        else if(!strncmp(buf, "/close ", 7)) // Stop someone's cam/mic broadcast
        {
          char nick[strlen(&buf[7])+1];
          strcpy(nick, &buf[7]);
          amfinit(&amf, 2);
          amfstring(&amf, "owner_run");
          amfnum(&amf, 0);
          amfnull(&amf);
          sprintf(buf, "_close%s", nick);
          amfstring(&amf, buf);
          amfsend(&amf, sock);
          len=sprintf(buf, "closed: %s", nick);
        }
        else if(!strncmp(buf, "/ban ", 5)) // Ban someone
        {
          char nick[strlen(&buf[5])+1];
          strcpy(nick, &buf[5]);
          amfinit(&amf, 3);
          amfstring(&amf, "owner_run");
          amfnum(&amf, 0);
          amfnull(&amf);
          sprintf(buf, "notice%s%%20was%%20banned%%20by%%20%s%%20(%s)", nick, nickname, account_user);
          amfstring(&amf, buf);
          amfsend(&amf, sock);
          printf("%s %s was banned by %s (%s)\n", timestamp(), nick, nickname, account_user);
          // kick (this does the actual banning)
          amfinit(&amf, 3);
          amfstring(&amf, "kick");
          amfnum(&amf, 0);
          amfnull(&amf);
          amfstring(&amf, nick);
          sprintf(buf, "%i", idlist_get(nick));
          amfstring(&amf, buf);
          amfsend(&amf, sock);
          continue;
        }
        else if(!strcmp(buf, "/banlist") || !strncmp(buf, "/forgive ", 9))
        {
          free(unban);
          if(buf[1]=='f') // forgive
          {
            unban=strdup(&buf[9]);
          }else{
            unban=0;
          }
          amfinit(&amf, 3);
          amfstring(&amf, "banlist");
          amfnum(&amf, 0);
          amfnull(&amf);
          amfsend(&amf, sock);
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
          amfinit(&amf, 3);
          amfstring(&amf, "owner_run");
          amfnum(&amf, 0);
          amfnull(&amf);
          amfstring(&amf, "mute");
          amfsend(&amf, sock);
          continue;
        }
        else if(!strcmp(buf, "/push2talk"))
        {
          amfinit(&amf, 3);
          amfstring(&amf, "owner_run");
          amfnum(&amf, 0);
          amfnull(&amf);
          amfstring(&amf, "push2talk");
          amfsend(&amf, sock);
          continue;
        }
        else if(!strcmp(buf, "/camup"))
        {
          // Retrieve and send the key for broadcasting access
          char* key=getbroadcastkey(channel, nickname);
          amfinit(&amf, 3);
          amfstring(&amf, "bauth");
          amfnum(&amf, 0);
          amfnull(&amf);
          amfstring(&amf, key);
          amfsend(&amf, sock);
          free(key);
          // Initiate stream
          streamout_start(idlist_get(nickname), sock);
          continue;
        }
        else if(!strcmp(buf, "/camdown"))
        {
          stream_stopvideo(sock, idlist_get(nickname));
          continue;
        }
        else if(!strncmp(buf, "/video ", 7)) // Send video data
        {
          size_t len=strtoul(&buf[7],0,10);
          char buf[len];
          fullread(0, buf, len);
          stream_sendvideo(sock, buf, len);
          continue;
        }
        else if(!strncmp(buf, "/topic ", 7))
        {
          amfinit(&amf, 3);
          amfstring(&amf, "topic");
          amfnum(&amf, 0);
          amfnull(&amf);
          amfstring(&amf, &buf[7]);
          amfstring(&amf, "");
          amfsend(&amf, sock);
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
      }
      char* msg=tonumlist(buf, len);
      amfinit(&amf, 3);
      amfstring(&amf, "privmsg");
      amfnum(&amf, 0);
      amfnull(&amf);
      amfstring(&amf, msg);
      amfstring(&amf, colors[currentcolor%COLORCOUNT]);
      // For PMs, add a string like "n<numeric ID>-<nick>" to make the server only send it to the intended recipient
      if(privfield)
      {
        amfstring(&amf, privfield);
        // And one in case they're broadcasting
        privfield[0]='b'; // 'b' for broadcasting
        struct rtmp bamf;
        amfinit(&bamf, 3);
        amfstring(&bamf, "privmsg");
        amfnum(&bamf, 0);
        amfnull(&bamf);
        amfstring(&bamf, msg);
        amfstring(&bamf, colors[currentcolor%COLORCOUNT]);
        amfstring(&bamf, privfield);
        amfsend(&bamf, sock);
        free(privfield);
      }
      amfsend(&amf, sock);
      free(msg);
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
    if(amfin->itemcount>0 && amfin->items[0].type==AMF_STRING)
    {
//      printf("Got command: '%s'\n", amfin->items[0].string.string);
      if(!strcmp(amfin->items[0].string.string, "_error"))
        printamf(amfin);
    }
    // Items for privmsg: 0=cmd, 2=channel, 3=msg, 4=color/lang, 5=nick
    if(amfin->itemcount>5 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "privmsg") && amfin->items[3].type==AMF_STRING && amfin->items[4].type==AMF_STRING && amfin->items[5].type==AMF_STRING)
    {
      size_t len;
      char* msg=fromnumlist(amfin->items[3].string.string, &len);
      const char* color=color_start(amfin->items[4].string.string);
      char* line=msg;
      while(line)
      {
        // Handle multi-line messages
        char* nextline=0;
        unsigned int linelen;
        for(linelen=0; &line[linelen]<&msg[len]; ++linelen)
        {
          if(line[linelen]=='\r' || line[linelen]=='\n'){nextline=&line[linelen+1]; break;}
        }
        printf("%s %s%s: ", timestamp(), color, amfin->items[5].string.string);
        fwrite(line, linelen, 1, stdout);
        printf("%s\n", color_end());
        line=nextline;
      }
      char* response=0;
      if(len==18 && !strncmp(msg, "/userinfo $request", 18))
      {
        if(account_user)
        {
          unsigned int len=strlen("/userinfo ")+strlen(account_user);
          char buf[len+1];
          sprintf(buf, "/userinfo %s", account_user);
          response=tonumlist(buf, len);
        }else{
          response=tonumlist("/userinfo $noinfo", 17);
        }
      }
      else if(len==8 && !strncmp(msg, "/version", 8))
      {
        response=tonumlist("/version tc_client-" VERSION, strlen(VERSION)+19);
      }
      if(response)
      {
        // Send our command reponse with a privacy field
        amfinit(&amf, 3);
        amfstring(&amf, "privmsg");
        amfnum(&amf, 0);
        amfnull(&amf);
        amfstring(&amf, response);
        amfstring(&amf, "#0,en");
        int id=idlist_get(amfin->items[5].string.string);
        char priv[snprintf(0, 0, "n%i-%s", id, amfin->items[5].string.string)+1];
        sprintf(priv, "n%i-%s", id, amfin->items[5].string.string);
        amfstring(&amf, priv);
        amfsend(&amf, sock);
        // And again in case they're broadcasting
        amfinit(&amf, 3);
        amfstring(&amf, "privmsg");
        amfnum(&amf, 0);
        amfnull(&amf);
        amfstring(&amf, response);
        amfstring(&amf, "#0,en");
        priv[0]='b';
        amfstring(&amf, priv);
        amfsend(&amf, sock);
        free(response);
      }
      free(msg);
      fflush(stdout);
    }
    // users on channel entry.  there's also a "joinsdone" command for some reason...
    else if(amfin->itemcount>2 && amfin->items[0].type==AMF_STRING && (amf_comparestrings_c(&amfin->items[0].string, "joins") || amf_comparestrings_c(&amfin->items[0].string, "join") || amf_comparestrings_c(&amfin->items[0].string, "registered")))
    {
      if(amf_comparestrings_c(&amfin->items[0].string, "joins"))
      {
        printf("Currently online: ");
      }else{
        printf("%s ", timestamp());
      }
      int i;
      for(i = 2; i < amfin->itemcount; ++i)
      {
        if(amfin->items[i].type==AMF_OBJECT)
        {
          struct amfitem* item=amf_getobjmember(&amfin->items[i].object, "id");
          if(item->type!=AMF_NUMBER){continue;}
          int id=item->number;
          item=amf_getobjmember(&amfin->items[i].object, "nick");
          if(item->type!=AMF_STRING){continue;}
          const char* nick=item->string.string;
          item=amf_getobjmember(&amfin->items[i].object, "account");
          if(item->type!=AMF_STRING){continue;}
          const char* account=(item->string.string[0]?item->string.string:0);
          item=amf_getobjmember(&amfin->items[i].object, "mod");
          if(item->type!=AMF_BOOL){continue;}
          char mod=item->boolean;
          idlist_add(id, nick, account, mod);
          printf("%s%s", (i==2?"":", "), nick);
          if(amf_comparestrings_c(&amfin->items[0].string, "registered"))
          {
            // Tell the server how often to let us know it got our packets
            struct rtmp setbw={
              .type=RTMP_SERVER_BW,
              .chunkid=2,
              .length=4,
              .msgid=0,
              .buf="\x00\x00\x40\x00" // Every 0x4000 bytes
            };
            rtmp_send(sock, &setbw);
            char* key=getkey(id, channel);
            curl_easy_cleanup(curl); // At this point we should be done with HTTP requests
            curl=0;
            if(!key){printf("Failed to get channel key\n"); return 1;}
            amfinit(&amf, 3);
            amfstring(&amf, "cauth");
            amfnum(&amf, 0);
            amfnull(&amf); // Means nothing but is apparently critically important for cauth at least
            amfstring(&amf, key);
            amfsend(&amf, sock);
            free(key);
            if(nickname[0]) // Empty = don't set one
            {
              amfinit(&amf, 3);
              amfstring(&amf, "nick");
              amfnum(&amf, 0);
              amfnull(&amf);
              amfstring(&amf, nickname);
              amfsend(&amf, sock);
            }
          }
        }
      }
      if(amf_comparestrings_c(&amfin->items[0].string, "joins"))
      {
        printf("\n");
        for(i=0; i<idlistlen; ++i)
        {
          if(idlist[i].op){printf("%s is a moderator.\n", idlist[i].name);}
        }
        printf("Currently on cam: ");
        char first=1;
        for(i = 2; i < amfin->itemcount; ++i)
        {
          if(amfin->items[i].type==AMF_OBJECT)
          {
            struct amfitem* item=amf_getobjmember(&amfin->items[i].object, "bf");
            if(item->type!=AMF_BOOL || !item->boolean){continue;}
            item=amf_getobjmember(&amfin->items[i].object, "nick");
            if(item->type!=AMF_STRING){continue;}
            printf("%s%s", (first?"":", "), item->string.string);
            first=0;
          }
        }
        printf("\n");
      }else{
        printf(" entered the channel\n");
        if(idlist[idlistlen-1].op){printf("%s is a moderator.\n", idlist[idlistlen-1].name);}
        if(amf_comparestrings_c(&amfin->items[0].string, "registered"))
        {
          printf("Connection ID: %i\n", idlist[0].id);
        }
      }
      fflush(stdout);
    }
    // quit/part
    else if(amfin->itemcount>2 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "quit") && amfin->items[2].type==AMF_STRING)
    {
      idlist_remove(amfin->items[2].string.string);
      printf("%s %s left the channel\n", timestamp(), amfin->items[2].string.string);
      fflush(stdout);
    }
    // nick
    else if(amfin->itemcount==5 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "nick") && amfin->items[2].type==AMF_STRING && amfin->items[3].type==AMF_STRING)
    {
      if(!strcmp(amfin->items[2].string.string, nickname)) // Successfully changed our own nickname
      {
        free(nickname);
        nickname=strdup(amfin->items[3].string.string);
      }
      idlist_rename(amfin->items[2].string.string, amfin->items[3].string.string);
      printf("%s %s changed nickname to %s\n", timestamp(), amfin->items[2].string.string, amfin->items[3].string.string);
      fflush(stdout);
    }
    // kick
    else if(amfin->itemcount==4 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "kick") && amfin->items[2].type==AMF_STRING)
    {
      if(atoi(amfin->items[2].string.string) == idlist_get(nickname))
      {
        printf("%s You have been kicked out\n", timestamp());
        fflush(stdout);
      }
    }
    // banned
    else if(amfin->itemcount==2 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "banned"))
    {
      printf("%s You are banned from %s\n", timestamp(), channel);
      fflush(stdout);
      // When banned and reconnecting, tinychat doesn't disconnect us itself, we need to disconnect
      close(sock);
      return 1; // Getting banned is a failure, right?
    }
    // from_owner: notices, mute, push2talk, closing cams
    else if(amfin->itemcount==3 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "from_owner") && amfin->items[2].type==AMF_STRING)
    {
      if(!strncmp("notice", amfin->items[2].string.string, 6))
      {
        char* notice=strdup(&amfin->items[2].string.string[6]);
        // replace "%20" with spaces
        char* space;
        while((space=strstr(notice, "%20")))
        {
          memmove(space, &space[2], strlen(&space[2])+1);
          space[0]=' ';
        }
        printf("%s %s\n", timestamp(), notice);
        fflush(stdout);
      }
      else if(!strcmp("mute", amfin->items[2].string.string))
      {
        printf("%s Non-moderators have been temporarily muted.\n", timestamp());
        fflush(stdout);
      }
      else if(!strcmp("push2talk", amfin->items[2].string.string))
      {
        printf("%s Push to talk request has been sent to non-moderators.\n", timestamp());
        fflush(stdout);
      }
      else if(!strncmp("_close", amfin->items[2].string.string, 6) && !strcmp(&amfin->items[2].string.string[6], nickname))
      {
        printf("Outgoing media stream was closed\n");
        fflush(stdout);
        stream_stopvideo(sock, idlist_get(nickname));
      }
    }
    // nickinuse, the nick we wanted to change to is already taken
    else if(amfin->itemcount>0 && amfin->items[0].type==AMF_STRING &&  amf_comparestrings_c(&amfin->items[0].string, "nickinuse"))
    {
      printf("Nick is already in use.\n");
      fflush(stdout);
    }
    // Room topic
    else if(amfin->itemcount>2 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "topic") && amfin->items[2].type==AMF_STRING && strlen(amfin->items[2].string.string) > 0)
    {
      printf("Room topic: %s\n", amfin->items[2].string.string);
      fflush(stdout);
    }
    // Get list of banned users
    else if(amfin->itemcount>0 && amfin->items[0].type==AMF_STRING &&  amf_comparestrings_c(&amfin->items[0].string, "banlist"))
    {
      unsigned int i;
      if(unban) // This is not a response to /banlist but to /forgive
      {
        for(i=2; i+1<amfin->itemcount; i+=2)
        {
          if(amfin->items[i].type!=AMF_STRING || amfin->items[i+1].type!=AMF_STRING){break;}
          if(!strcmp(amfin->items[i+1].string.string, unban) || !strcmp(amfin->items[i].string.string, unban))
          {
            amfinit(&amf, 3);
            amfstring(&amf, "forgive");
            amfnum(&amf, 0);
            amfnull(&amf);
            amfstring(&amf, amfin->items[i].string.string);
            amfsend(&amf, sock);
          }
          // If the nickname isn't found in the banlist we assume it's an ID
        }
        continue;
      }
      printf("Banned users:\n");
      printf("ID         Nickname\n");
      for(i=2; i+1<amfin->itemcount; i+=2)
      {
        if(amfin->items[i].type!=AMF_STRING || amfin->items[i+1].type!=AMF_STRING){break;}
        unsigned int len=printf("%s", amfin->items[i].string.string);
        for(;len<10; ++len){printf(" ");}
        printf(" %s\n", amfin->items[i+1].string.string);
      }
      printf("Use /forgive <ID/nick> to unban someone\n");
      fflush(stdout);
    }
    else if(amfin->itemcount>4 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "notice") && amfin->items[2].type==AMF_STRING && amf_comparestrings_c(&amfin->items[2].string, "avon"))
    {
      if(amfin->items[4].type==AMF_STRING)
      {
        printf("%s cammed up\n", amfin->items[4].string.string);
        fflush(stdout);
      }
    }
    // Handle results for various requests, haven't seen much of a pattern to it, always successful?
    else if(amfin->itemcount>0 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "_result"))
    {
      // Creating a new stream worked, now play media (cam/mic) on it (if that's what the result was for)
      stream_play(amfin, sock);
    }
    else if(amfin->itemcount>0 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "onStatus"))
    {
      stream_handlestatus(amfin, sock);
    }
    else if(amfin->itemcount>2 && amfin->items[0].type==AMF_STRING && amfin->items[2].type==AMF_OBJECT && amf_comparestrings_c(&amfin->items[0].string, "account"))
    {
      struct amfitem* id=amf_getobjmember(&amfin->items[2].object, "id");
      struct amfitem* account=amf_getobjmember(&amfin->items[2].object, "account");
      if(id && account && id->type==AMF_NUMBER && account->type==AMF_STRING)
      {
        for(i=0; i<idlistlen; ++i)
        {
          if(id->number==idlist[i].id)
          {
            if(strcmp(account->string.string, "$noinfo"))
            {
              printf("%s is logged in as %s\n", idlist[i].name, account->string.string);
            }else{
              printf("%s is not logged in\n", idlist[i].name);
            }
            fflush(stdout);
            break;
          }
        }
      }
    }
    // else{printf("Unknown command...\n"); printamf(amfin);} // (Debugging)
    amf_free(amfin);
  }
  free(rtmp.buf);
  close(sock);
  return 0;
}
