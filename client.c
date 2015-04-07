/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2014  alicia@ion.nu
    Copyright (C) 2014  Jade Lea

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
#include <locale.h>
#include <curl/curl.h>
#include "rtmp.h"
#include "amfparser.h"
#include "numlist.h"
#include "amfwriter.h"
#include "idlist.h"
#include "colors.h"

struct writebuf
{
  char* buf;
  int len;
};

char showcolor=1;

unsigned int flip(unsigned int bits, int bytecount)
{
  unsigned int ret=0;
  unsigned char* bytes=(unsigned char*)&bits;
  unsigned char* retb=(unsigned char*)&ret;
  int i;
  for(i=0; i<bytecount; ++i)
  {
    retb[i]=bytes[bytecount-1-i];
  }
  return ret;
}

void b_read(int sock, void* buf, size_t len)
{
  while(len>0)
  {
    size_t r=read(sock, buf, len);
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

char* http_get(char* url)
{
  CURL* curl=curl_easy_init();
  if(!curl){return 0;}
  curl_easy_setopt(curl, CURLOPT_URL, url);
  struct writebuf writebuf={0, 0};
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writehttp);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writebuf);
  char err[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);
  if(curl_easy_perform(curl)){curl_easy_cleanup(curl); printf(err); return 0;}
  curl_easy_cleanup(curl);
  return writebuf.buf; // should be free()d when no longer needed
}

char* gethost(char *channel, char *password)
{
  int urllen;
  if(password)
  {
    urllen=strlen("http://tinychat.com/api/find.room/?site=tinychat&password=0")+strlen(channel)+strlen(password);
  }else{
    urllen=strlen("http://tinychat.com/api/find.room/?site=tinychat0")+strlen(channel);
  }
  char url[urllen];
  if(password)
  {
    sprintf(url, "http://tinychat.com/api/find.room/%s?site=tinychat&password=%s", channel, password);
  }else{
    sprintf(url, "http://tinychat.com/api/find.room/%s?site=tinychat", channel);
  }
  char *response=http_get(url);
  //response contains result='(OK|RES)|PW' (latter means a password is required)
  char* result=strstr(response, "result='");
  if(!result){printf("No result\n"); exit(-1); return 0;}
  result+=strlen("result='");
  // Handle the result value
  if(!strncmp(result, "PW", 2)){printf("Password required\n"); exit(-1); return 0;}
  if(strncmp(result, "OK", 2) && strncmp(result, "RES", 3)){printf("Result not OK\n"); exit(-1); return 0;}
  // Find and extract the server responsible for this channel
  char* rtmp=strstr(response, "rtmp='rtmp://");
  if(!rtmp){printf("No rtmp found.\n"); exit(-1); return 0;}
  rtmp+=strlen("rtmp='rtmp://");
  int len;
  for(len=0; rtmp[len] && rtmp[len]!='/'; ++len);
  char* host=strndup(rtmp, len);
  free(response);
  return host;
}

char* getkey(char* id, char* channel)
{
  char url[strlen("http://tinychat.com/api/captcha/check.php?guest%5Fid=&room=tinychat%5E0")+strlen(id)+strlen(channel)];
  sprintf(url, "http://tinychat.com/api/captcha/check.php?guest%%5Fid=%s&room=tinychat%%5E%s", id, channel);
  char* response=http_get(url);
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

char timestampbuf[8];
char* timestamp()
{
  // Timestamps, e.g. "[hh:mm] name: message"
  time_t timestamp=time(0);
  struct tm* t=localtime(&timestamp);
  sprintf(timestampbuf, "[%02i:%02i]", t->tm_hour, t->tm_min);
  return timestampbuf;
}

int main(int argc, char** argv)
{
  setlocale(LC_ALL, "");
  if(argc<3)
   {
    printf("Usage: %s <channelname> <nickname> [password]\n", argv[0]);
    return 1;
  }
  char* channel=argv[1];
  char* nickname=strdup(argv[2]);
  char* password=(argc>3?argv[3]:0);
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
  int random=open("/dev/urandom", O_RDONLY);
  // RTMP handshake
  unsigned char handshake[1536];
  read(random, handshake, 1536);
  read(random, &currentcolor, sizeof(currentcolor));
  close(random);
  write(sock, "\x03", 1); // Send 0x03 and 1536 bytes of random junk
  write(sock, handshake, 1536);
  read(sock, handshake, 1); // Receive server's 0x03+junk
  b_read(sock, handshake, 1536);
  write(sock, handshake, 1536); // Send server's junk back
  b_read(sock, handshake, 1536); // Read our junk back, we don't bother checking that it's the same
  printf("Handshake complete\n");
  // Handshake complete, send connect request
  struct amfmsg amf;
  amfinit(&amf);
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
  amfstring(&amf, channel);
  amfstring(&amf, "none");
  amfstring(&amf, "show"); // This item is called roomtype in the same HTTP response that gives us the server (IP+port) to connect to
  amfstring(&amf, "tinychat");
  amfstring(&amf, "");
  amfsend(&amf, sock);

  unsigned char buf[2048];
  int len=read(sock, buf, 2048);
//  printf("Received %i byte response\n", len);
/* Debugging
  int f=open("output", O_WRONLY|O_CREAT|O_TRUNC, 0644);
  write(f, buf, len);
  close(f);
*/

//  int outnum=2; (Debugging, number for output filenames)
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
      len=read(0, buf, 2047);
      if(len<1){break;}
      while(len>0 && (buf[len-1]=='\n'||buf[len-1]=='\r')){--len;}
      if(!len){continue;} // Don't send empty lines
      buf[len]=0;
      int privfield=-1;
      int privlen;
      if(buf[0]=='/') // Got a command
      {
        if(!strncmp((char*)buf, "/color", 6) && (!buf[6]||buf[6]==' '))
        {
          if(buf[6]) // Color specified
          {
            if(!strcmp((char*)&buf[7], "off")){showcolor=0; continue;}
            if(!strcmp((char*)&buf[7], "on")){showcolor=1; continue;}
            currentcolor=atoi((char*)&buf[7]);
            printf("\x1b[%smChanged color\x1b[0m\n", termcolors[currentcolor%16]);
          }else{ // No color specified, state our current color
            printf("\x1b[%smCurrent color: %i\x1b[0m\n", termcolors[currentcolor%16], currentcolor%16);
          }
          fflush(stdout);
          continue;
        }
        else if(!strcmp((char*)buf, "/colors"))
        {
          int i;
          for(i=0; i<16; ++i)
          {
            printf("\x1b[%smColor %i\x1b[0m\n", termcolors[i], i);
          }
          fflush(stdout);
          continue;
        }
        else if(!strncmp((char*)buf, "/nick ", 6))
        {
          free(nickname);
          nickname=strdup((char*)&buf[6]);
          amfinit(&amf);
          amfstring(&amf, "nick");
          amfnum(&amf, 0);
          amfnull(&amf);
          amfstring(&amf, nickname);
          amfsend(&amf, sock);
          continue;
        }
        else if(!strncmp((char*)buf, "/msg ", 5))
        {
          for(privlen=0; buf[5+privlen]&&buf[5+privlen]!=' '; ++privlen);
          privfield=idlist_get((char*)&buf[5]);
          if(privfield<0)
          {
            printf("No such nick\n");
            fflush(stdout);
            continue;
          }
        }
      }
      len=mbstowcs(0, (char*)buf, 0);
      wchar_t wcsbuf[len+1];
      mbstowcs(wcsbuf, (char*)buf, len+1);
      char* msg=tonumlist(wcsbuf);
      amfinit(&amf);
      amfstring(&amf, "privmsg");
      amfnum(&amf, 0);
      amfnull(&amf);
      amfstring(&amf, msg);
      amfstring(&amf, colors[currentcolor%16]);
      // For PMs, add a string like "n<numeric ID>-<nick>" to make the server only send it to the intended recipient
      if(privfield>-1)
      {
        char priv[snprintf(0, 0, "n%i-", privfield)+privlen+1];
        sprintf(priv, "n%i-", privfield);
        strncat(priv, (char*)&buf[5], privlen);
        amfstring(&amf, priv);
      }
      amfsend(&amf, sock);
      free(msg);
      continue;
    }
    // Got data from server
    pfd[1].revents=0;
// TODO: This should be done differently, first reading one byte to get the size of the header, then read the rest of the header and thus get the length of the content
    len=read(sock, buf, 2048);
//  printf("Received %i byte response\n", len);
    if(len<=0){printf("Server disconnected\n"); break;}
/* Debugging
  char name[128];
  sprintf(name, "output%i", outnum);
  f=open(name, O_WRONLY|O_CREAT|O_TRUNC, 0644);
  write(f, buf, len);
  close(f);
*/
    // Separate and handle all AMF0 buffers in the RTMP stream
    unsigned char* rtmp_pos=buf;
    int rtmp_len;
    unsigned char* amfbuf;
    while((amfbuf=rtmp_getamf(&rtmp_pos, &len, &rtmp_len)))
    {
      struct amf* amfin=amf_parse(amfbuf, rtmp_len);
      if(amfin->itemcount>0 && amfin->items[0].type==AMF_STRING)
      {
//        printf("Got command: '%s'\n", amfin->items[0].string.string);
        if(!strcmp(amfin->items[0].string.string, "_error"))
          printamf(amfin);
      }
      if(amfin->itemcount>0 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "registered") && amfin->items[amfin->itemcount-1].type==AMF_STRING)
      {
        char* id=amfin->items[amfin->itemcount-1].string.string;
        printf("Guest ID: %s\n", id);
        char* key=getkey(id, channel);

        amfinit(&amf);
        amfstring(&amf, "cauth");
        amfnum(&amf, 0);
        amfnull(&amf); // Means nothing but is apparently critically important for cauth at least
        amfstring(&amf, key);
        amfsend(&amf, sock);
        free(key);

        amfinit(&amf);
        amfstring(&amf, "nick");
        amfnum(&amf, 0);
        amfnull(&amf);
        amfstring(&amf, nickname);
        amfsend(&amf, sock);
      }
      // Items for privmsg: 0=cmd, 2=channel, 3=msg, 4=color/lang, 5=nick
      else if(amfin->itemcount>5 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "privmsg") && amfin->items[3].type==AMF_STRING && amfin->items[4].type==AMF_STRING && amfin->items[5].type==AMF_STRING)
      {
        wchar_t* msg=fromnumlist(amfin->items[3].string.string);
        const char* color=(showcolor?resolvecolor(amfin->items[4].string.string):"0");
        printf("%s \x1b[%sm%s: %ls\x1b[0m\n", timestamp(), color, amfin->items[5].string.string, msg);
        free(msg);
        fflush(stdout);
      }
      // users on channel entry.  there's also a "joinsdone" command for some reason...
      else if(amfin->itemcount>3 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "joins"))
      {
        printf("Currently online: ");
        int i;
        for(i = 3; i < amfin->itemcount-1; i+=2)
        {
          // a "numeric" id precedes each nick, i.e. i is the id, i+1 is the nick
          if(amfin->items[i].type==AMF_STRING && amfin->items[i+1].type==AMF_STRING)
          {
            idlist_add(atoi(amfin->items[i].string.string), amfin->items[i+1].string.string);
            printf("%s%s", (i==3?"":", "), amfin->items[i+1].string.string);
          }
        }
        printf("\n");
        fflush(stdout);
      }
      // join ("join", 0, "<ID>", "guest-<ID>")
      else if(amfin->itemcount==4 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "join") && amfin->items[2].type==AMF_STRING && amfin->items[3].type==AMF_STRING)
      {
        idlist_add(atoi(amfin->items[2].string.string), amfin->items[3].string.string);
        printf("%s %s entered the channel\n", timestamp(), amfin->items[3].string.string);
        fflush(stdout);
      }
      // part
      else if(amfin->itemcount==4 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "quit") && amfin->items[3].type==AMF_STRING)
      {
        idlist_remove(amfin->items[2].string.string);
        printf("%s %s left the channel\n", timestamp(), amfin->items[2].string.string);
        fflush(stdout);
      }
      // nick
      else if(amfin->itemcount==5 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "nick") && amfin->items[2].type==AMF_STRING && amfin->items[3].type==AMF_STRING)
      {
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
      // from_owner: notices
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
      }
      // else{printf("Unknown command...\n"); printamf(amfin);} // (Debugging)
      amf_free(amfin);
    }
//    ++outnum; (Debugging)
  }
  close(sock);
  return 0;
}
