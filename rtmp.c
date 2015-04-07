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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include "rtmp.h"

char rtmp_get(int sock, struct rtmp* rtmp)
{
  static unsigned int length;
  static unsigned char type;
  static unsigned int timestamp;
  // Header format and stream ID
  unsigned int x=0;
  if(read(sock, &x, 1)<1){return 0;}
  unsigned int streamid=x&0x3f;
  unsigned int fmt=(x&0xc0)>>6;
  // Handle extended stream IDs
  if(streamid<2) // (0=1 extra byte, 1=2 extra bytes)
  {
    read(sock, &x, streamid+1);
    streamid=64+x;
  }
  if(fmt<3)
  {
    // Timestamp
    read(sock, &x, 3);
    timestamp=x;
    if(fmt<2)
    {
      // Length
      x=0;
      read(sock, ((void*)&x)+1, 3);
      length=be32toh(x);
      // Type
      read(sock, &type, sizeof(type));
      if(fmt<1)
      {
        // Message ID (is this ever used?)
        unsigned int msgid;
        read(sock, &msgid, sizeof(msgid));
      }
    }
  }
  // Extended timestamp
  if(timestamp==0xffffff)
  {
    read(sock, &x, sizeof(x));
    timestamp=be32toh(x);
  }

  rtmp->type=type;
  rtmp->streamid=streamid;
  rtmp->length=length+length/128;
  free(rtmp->buf);
  rtmp->buf=malloc(rtmp->length);
  read(sock, rtmp->buf, rtmp->length);
  int i;
  for(i=128; i<rtmp->length; i+=128)
  {
    --rtmp->length;
    // printf("Dropping garbage byte %x\n", (unsigned int)((unsigned char*)rtmp->buf)[i]&0xff);
    memmove(rtmp->buf+i, rtmp->buf+i+1, rtmp->length-i);
  }
  return 1;
}
