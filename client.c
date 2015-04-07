/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2014  alicia@ion.nu

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
#include <netdb.h>
#include <poll.h>
#include <curl/curl.h>
#include "rtmp.h"
#include "amfparser.h"
#include "numlist.h"

struct amfmsg
{
  unsigned int len;
  unsigned char* buf;
};

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

void amfinit(struct amfmsg* msg)
{
  msg->len=0;
  msg->buf=malloc(sizeof(struct rtmph));
}

void amfsend(struct amfmsg* msg, int sock)
{
  struct rtmph* head=(struct rtmph*)msg->buf;
  head->streamid=3;
  head->fmt=0;
  head->timestamp=0; // Time is irrelevant
  head->length=flip(msg->len, 3);
  head->type=20;
  head->msgid=0;
  // Send 128 bytes at a time separated by 0xc3 (because apparently that's something RTMP requires)
// printf("Writing %i-byte message\n", msg->len);
  write(sock, msg->buf, sizeof(struct rtmph));
  unsigned char* pos=msg->buf+sizeof(struct rtmph);
  while(msg->len>0)
  {
    int w;
    if(msg->len>128)
    {
      w=write(sock, pos, 128);
      w+=write(sock, "\xc3", 1);
      msg->len-=128;
    }else{
      w=write(sock, pos, msg->len);
      msg->len=0;
    }
// printf("Wrote %i bytes\n", w);
    pos+=128;
  }
  free(msg->buf);
  msg->buf=0;
}

void amfnum(struct amfmsg* msg, double v)
{
  int offset=sizeof(struct rtmph)+msg->len;
  msg->len+=1+sizeof(double);
  msg->buf=realloc(msg->buf, sizeof(struct rtmph)+msg->len);
  unsigned char* type=msg->buf+offset;
  type[0]='\x00';
  double* value=(double*)(msg->buf+offset+1);
  *value=v;
}

void amfbool(struct amfmsg* msg, char v)
{
  int offset=sizeof(struct rtmph)+msg->len;
  msg->len+=2;
  msg->buf=realloc(msg->buf, sizeof(struct rtmph)+msg->len);
  unsigned char* x=msg->buf+offset;
  x[0]='\x01';
  x[1]=!!v;
}

void amfstring(struct amfmsg* msg, char* string)
{
  int len=strlen(string);
  int offset=sizeof(struct rtmph)+msg->len;
  msg->len+=3+len;
  msg->buf=realloc(msg->buf, sizeof(struct rtmph)+msg->len);
  unsigned char* type=msg->buf+offset;
  type[0]='\x02';
  uint16_t* strlength=(uint16_t*)(msg->buf+offset+1);
  *strlength=htons(len);
  memcpy(msg->buf+offset+3, string, len);
}

void amfobjstart(struct amfmsg* msg)
{
  int offset=sizeof(struct rtmph)+msg->len;
  msg->len+=1;
  msg->buf=realloc(msg->buf, sizeof(struct rtmph)+msg->len);
  unsigned char* type=msg->buf+offset;
  type[0]='\x03';
}

void amfobjitem(struct amfmsg* msg, char* name)
{
  int len=strlen(name);
  int offset=sizeof(struct rtmph)+msg->len;
  msg->len+=2+len;
  msg->buf=realloc(msg->buf, sizeof(struct rtmph)+msg->len);
  uint16_t* strlength=(uint16_t*)(msg->buf+offset);
  *strlength=htons(len);
  memcpy(msg->buf+offset+2, name, len);
}

void amfobjend(struct amfmsg* msg)
{
  amfobjitem(msg, "");
  int offset=sizeof(struct rtmph)+msg->len;
  msg->len+=1;
  msg->buf=realloc(msg->buf, sizeof(struct rtmph)+msg->len);
  unsigned char* type=msg->buf+offset;
  type[0]='\x09';
}

void amfnull(struct amfmsg* msg)
{
  int offset=sizeof(struct rtmph)+msg->len;
  msg->len+=1;
  msg->buf=realloc(msg->buf, sizeof(struct rtmph)+msg->len);
  unsigned char* type=msg->buf+offset;
  type[0]='\x05';
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

char* http_get(char* url)
{
  CURL* curl=curl_easy_init();
  if(!curl){return 0;}
  curl_easy_setopt(curl, CURLOPT_URL, url);
  char* buf=0;
  int len=0;
  size_t writehttp(char* ptr, size_t size, size_t nmemb, void* x)
  {
    size*=nmemb;
    buf=realloc(buf, len+size+1);
    memcpy(&buf[len], ptr, size);
    len+=size;
    buf[len]=0;
    return size;
  }
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writehttp);
  char err[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);
  if(curl_easy_perform(curl)){curl_easy_cleanup(curl); printf(err); return 0;}
  curl_easy_cleanup(curl);
  return buf; // should be free()d when no longer needed
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
  char *response=http_get(url);
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

int main(int argc, char** argv)
{
  if(argc<3)
   {
    printf("Usage: %s <roomname> <nickname> [password]\n", argv[0]);
    return 1;
  }
  char* channel=argv[1];
  char* nickname=argv[2];
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
    amfstring(&amf, "LNX 11,2,202,424");

    amfobjitem(&amf, "swfUrl");
    amfstring(&amf, "http://tinychat.com/embed/Tinychat-11.1-1.0.0.0602.swf?version=1.0.0.0602/[[DYNAMIC]]/8");

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

/* As they appear in the flash client:
    char* colors[]={
      "#1965b6,en",
      "#32a5d9,en",
      "#7db257,en",
      "#a78901,en",
      "#9d5bb5,en",
      "#5c1a7a,en",
      "#c53332,en",
      "#821615,en",
      "#a08f23,en",
      "#487d21,en",
      "#c356a3,en",
      "#1d82eb,en",
      "#919104,en",
      "#00a990,en",
      "#b9807f,en",
      "#7bb224,en"
    };
*/
    char* colors[]={ // Sorted like a rainbow
      "#821615,en",
      "#c53332,en",
      "#a08f23,en",
      "#a78901,en",
      "#919104,en",
      "#7bb224,en",
      "#7db257,en",
      "#487d21,en",
      "#00a990,en",
      "#32a5d9,en",
      "#1d82eb,en",
      "#1965b6,en",
      "#5c1a7a,en",
      "#9d5bb5,en",
      "#c356a3,en",
      "#b9807f,en"
    };
    unsigned int currentcolor=0;

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
      while(len>0 && (buf[len-1]=='\n'||buf[len-1]=='\r')){--len;}
      buf[len]=0;
      char* msg=tonumlist((char*)buf);
      amfinit(&amf);
      amfstring(&amf, "privmsg");
      amfnum(&amf, 0);
      amfnull(&amf);
      amfstring(&amf, msg);
      // The alternating color thing is an unnecessary but fun feature
      amfstring(&amf, colors[currentcolor%16]);
      amfsend(&amf, sock);
      ++currentcolor;
      free(msg);
      continue;
    }
    // Got data from server
    pfd[1].revents=0;
// TODO: This should be done differently, first reading one byte to get the size of the header, then read the rest of the header and thus get the length of the content
    len=read(sock, buf, 2048);
//  printf("Received %i byte response\n", len);
    if(!len){printf("Server disconnected\n"); break;}
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
      if(amfin->itemcount>0 && amfin->items[0].type==AMF_STRING && amf_comparestrings_c(&amfin->items[0].string, "privmsg") && amfin->items[3].type==AMF_STRING && amfin->items[5].type==AMF_STRING)
      {
        char* msg=fromnumlist(amfin->items[3].string.string);
        // Timestamps, e.g. "[hh:mm] name: message"
        time_t timestamp=time(0);
        struct tm* t=localtime(&timestamp);
        printf("[%02i:%02i] %s: %s\n", t->tm_hour, t->tm_min, amfin->items[5].string.string, msg);
        free(msg);
      }
      amf_free(amfin);
    }
//    ++outnum; (Debugging)
  }
  return 0;
}
