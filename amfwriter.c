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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "rtmp.h"
#include "amfwriter.h"

extern unsigned int flip(unsigned int bits, int bytecount);

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
  memcpy(msg->buf+offset+1, &v, sizeof(v));
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
