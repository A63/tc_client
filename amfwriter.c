/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2014-2015  alicia@ion.nu

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
#include <endian.h>
#include "rtmp.h"
#include "amfwriter.h"

void amfinit(struct rtmp* msg, unsigned int streamid)
{
  msg->type=RTMP_AMF0;
  msg->streamid=streamid;
  msg->length=0;
  msg->buf=0;
}

void amfnum(struct rtmp* msg, double v)
{
  int offset=msg->length;
  msg->length+=1+sizeof(double);
  msg->buf=realloc(msg->buf, msg->length);
  unsigned char* type=msg->buf+offset;
  type[0]='\x00';
  memcpy(msg->buf+offset+1, &v, sizeof(v));
}

void amfbool(struct rtmp* msg, char v)
{
  int offset=msg->length;
  msg->length+=2;
  msg->buf=realloc(msg->buf, msg->length);
  unsigned char* x=msg->buf+offset;
  x[0]='\x01';
  x[1]=!!v;
}

void amfstring(struct rtmp* msg, const char* string)
{
  int len=strlen(string);
  int offset=msg->length;
  msg->length+=3+len;
  msg->buf=realloc(msg->buf, msg->length);
  unsigned char* type=msg->buf+offset;
  type[0]='\x02';
  uint16_t* strlength=(uint16_t*)(msg->buf+offset+1);
  *strlength=htobe16(len);
  memcpy(msg->buf+offset+3, string, len);
}

void amfobjstart(struct rtmp* msg)
{
  int offset=msg->length;
  msg->length+=1;
  msg->buf=realloc(msg->buf, msg->length);
  unsigned char* type=msg->buf+offset;
  type[0]='\x03';
}

void amfobjitem(struct rtmp* msg, char* name)
{
  int len=strlen(name);
  int offset=msg->length;
  msg->length+=2+len;
  msg->buf=realloc(msg->buf, msg->length);
  uint16_t* strlength=(uint16_t*)(msg->buf+offset);
  *strlength=htobe16(len);
  memcpy(msg->buf+offset+2, name, len);
}

void amfobjend(struct rtmp* msg)
{
  amfobjitem(msg, "");
  int offset=msg->length;
  msg->length+=1;
  msg->buf=realloc(msg->buf, msg->length);
  unsigned char* type=msg->buf+offset;
  type[0]='\x09';
}

void amfnull(struct rtmp* msg)
{
  int offset=msg->length;
  msg->length+=1;
  msg->buf=realloc(msg->buf, msg->length);
  unsigned char* type=msg->buf+offset;
  type[0]='\x05';
}
