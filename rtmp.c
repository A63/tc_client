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

struct chunk
{
  unsigned int id;
  unsigned int length;
  unsigned char type;
  unsigned int timestamp;
  unsigned int streamid;
  unsigned int pos;
  void* buf;
};
struct chunk* chunks=0;
unsigned int chunkcount=0;
unsigned int chunksize_in=128;
#ifdef RTMP_DEBUG
int rtmplog=-1;
#endif

size_t fullread(int fd, void* buf, size_t len)
{
  size_t remaining=len;
  while(remaining>0)
  {
    size_t r=read(fd, buf, remaining);
    if(r<1){return 0;}
#ifdef RTMP_DEBUG
    if(rtmplog>-1)
    {
      write(rtmplog, buf, r);
    }
#endif
    remaining-=r;
    buf+=r;
  }
  return len;
}

struct chunk* chunk_get(unsigned int id)
{
  unsigned int i;
  for(i=0; i<chunkcount; ++i)
  {
    if(chunks[i].id==id){return &chunks[i];}
  }
// printf("No chunk found for %u, creating new one\n", id);
  ++chunkcount;
  chunks=realloc(chunks, sizeof(struct chunk)*chunkcount);
  chunks[i].id=id;
  chunks[i].streamid=0;
  chunks[i].buf=0;
  chunks[i].timestamp=0;
  chunks[i].length=0;
  chunks[i].type=0;
  return &chunks[i];
}

char rtmp_get(int sock, struct rtmp* rtmp)
{
  // Header format and chunk ID
  unsigned int x=0;
  if(fullread(sock, &x, 1)<1){return 0;}
  unsigned int chunkid=x&0x3f;
  unsigned int fmt=(x&0xc0)>>6;
  struct chunk* chunk=chunk_get(chunkid);
  // Handle extended stream IDs
  if(chunkid<2) // (0=1 extra byte, 1=2 extra bytes)
  {
    fullread(sock, &x, chunkid+1);
    chunkid=64+x;
  }
  if(fmt<3)
  {
    // Timestamp
    x=0;
    fullread(sock, ((void*)&x)+1, 3);
    chunk->timestamp=be32(x);
    if(fmt<2)
    {
      // Length
      x=0;
      fullread(sock, ((void*)&x)+1, 3);
      chunk->length=be32(x);
      // Type
      fullread(sock, &chunk->type, sizeof(chunk->type));
      if(fmt<1)
      {
        // Message ID
        fullread(sock, &chunk->streamid, sizeof(chunk->streamid));
      }
    }
  }
  // Extended timestamp
  if(chunk->timestamp==0xffffff)
  {
    fullread(sock, &x, sizeof(x));
    chunk->timestamp=be32(x);
  }

  if(!chunk->buf)
  {
    chunk->buf=malloc(chunk->length);
    chunk->pos=0;
  }
  unsigned int rsize=((chunk->length-chunk->pos>=chunksize_in)?chunksize_in:(chunk->length-chunk->pos));
  chunk->pos+=fullread(sock, chunk->buf+chunk->pos, rsize);
  if(chunk->pos==chunk->length)
  {
    if(chunk->type==RTMP_SET_PACKET_SIZE)
    {
      memcpy(&chunksize_in, chunk->buf, sizeof(unsigned int));
      chunksize_in=be32(chunksize_in);
//      printf("Server set chunk size to %u (packet size: %u)\n", chunksize_in, chunk->length);
    }
// printf("Got chunk: chunkid=%u, type=%u, length=%u, streamid=%u\n", chunk->id, chunk->type, chunk->length, chunk->streamid);
    rtmp->type=chunk->type;
    rtmp->chunkid=chunk->id;
    rtmp->length=chunk->length;
    rtmp->msgid=le32(chunk->streamid);
    free(rtmp->buf);
    rtmp->buf=chunk->buf;
    chunk->buf=0;
    return 1;
  }
// printf("Waiting for next part of chunk\n");
  return 2;
}

void rtmp_send(int sock, struct rtmp* rtmp)
{
  // Header format and stream ID
  unsigned int fmt=(rtmp->msgid?0:1);
  unsigned char basicheader=(rtmp->chunkid<64?rtmp->chunkid:(rtmp->chunkid<256?0:1)) | (fmt<<6);
  write(sock, &basicheader, sizeof(basicheader));
  if(rtmp->chunkid>=64) // Handle large stream IDs
  {
    if(rtmp->chunkid<256)
    {
      unsigned char chunkid=rtmp->chunkid-64;
      write(sock, &chunkid, sizeof(chunkid));
    }else{
      unsigned short chunkid=le16(rtmp->chunkid-64);
      write(sock, &chunkid, sizeof(chunkid));
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
  // Send 128 bytes at a time separated by a "continuation header", the 0xc3 byte for chunk 3
  void* pos=rtmp->buf;
  unsigned int len=rtmp->length;
  basicheader|=0xc0; // Turn it into a "continuation header" by setting format to 3
  while(len>0)
  {
    int w;
    if(len>128)
    {
      w=write(sock, pos, 128);
      w+=write(sock, &basicheader, 1);
      len-=128;
    }else{
      w=write(sock, pos, len);
      len=0;
    }
// printf("Wrote %i bytes\n", w);
    pos+=128;
  }
}
