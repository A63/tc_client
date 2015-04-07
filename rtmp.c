/*
    tc_client, a simple non-flash client for tinychat(.com)
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "endian.h"
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
  unsigned int msgid=0;
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
      length=be32(x);
      // Type
      read(sock, &type, sizeof(type));
      if(fmt<1)
      {
        // Message ID
        read(sock, &msgid, sizeof(msgid));
      }
    }
  }
  // Extended timestamp
  if(timestamp==0xffffff)
  {
    read(sock, &x, sizeof(x));
    timestamp=be32(x);
  }

  rtmp->type=type;
  rtmp->streamid=streamid;
  rtmp->length=length;
  rtmp->msgid=le32(msgid);
  free(rtmp->buf);
  rtmp->buf=malloc(rtmp->length);
  size_t pos=0;
  size_t w;
  // Only read up to 128 bytes at a time and discard the (garbage/RTMP continuation header) bytes in between
  while(pos<length)
  {
    w=read(sock, rtmp->buf+pos, ((length-pos>127)?128:(length-pos)));
    if(w<1){break;}
    pos+=w;
    if(length-pos>0){read(sock, &w, 1);} // Skip junk once every 128 bytes
  }
  return 1;
}

void rtmp_send(int sock, struct rtmp* rtmp)
{
  // Header format and stream ID
  unsigned int fmt=(rtmp->msgid?0:1);
  unsigned char basicheader=(rtmp->streamid<64?rtmp->streamid:(rtmp->streamid<256?0:1)) | (fmt<<6);
  write(sock, &basicheader, sizeof(basicheader));
  if(rtmp->streamid>=64) // Handle large stream IDs
  {
    if(rtmp->streamid<256)
    {
      unsigned char streamid=rtmp->streamid-64;
      write(sock, &streamid, sizeof(streamid));
    }else{
      unsigned short streamid=le16(rtmp->streamid-64);
      write(sock, &streamid, sizeof(streamid));
    }
  }
  unsigned int x=0;
  // Timestamp
  write(sock, &x, 3); // Time is irrelevant
  // Length
  x=be32(rtmp->length);
  write(sock, ((void*)&x)+1, 3);
  // Type
  write(sock, &rtmp->type, sizeof(rtmp->type));
  if(fmt<1) // Send message ID if there is one (that isn't 0)
  {
    write(sock, &rtmp->msgid, sizeof(rtmp->msgid));
  }
  // Send 128 bytes at a time separated by 0xc3 (because apparently that's something RTMP requires)
  void* pos=rtmp->buf;
  unsigned int len=rtmp->length;
  while(len>0)
  {
    int w;
    if(len>128)
    {
      w=write(sock, pos, 128);
      w+=write(sock, "\xc3", 1);
      len-=128;
    }else{
      w=write(sock, pos, len);
      len=0;
    }
// printf("Wrote %i bytes\n", w);
    pos+=128;
  }
}
