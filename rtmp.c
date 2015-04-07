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
#include "rtmp.h"

extern unsigned int flip(unsigned int bits, int bytecount);

int rtmp_lastlen=0;
int rtmp_lasttype=0;
unsigned char* rtmp_getamf(unsigned char** msg, int* length, int* amflen)
{
  int headsize;
  struct rtmph* head=(struct rtmph*)*msg;
  while((void*)head<((void*)*msg)+*length)
  {
    int len=flip(head->length, 3);
    int type;
    switch(head->fmt)
    {
      case 0: headsize=12; break;
      case 1: headsize=8; break;
      case 2: headsize=4; break;
      case 3: headsize=1; break;
    }
//  printf("fmt: %u\n", (unsigned int)head->fmt);
//  printf("streamid: %u\n", (unsigned int)head->streamid);
//  printf("timestamp: %u\n", (unsigned int)head->timestamp);
    if(headsize>=8){
//    printf("length: %u\n", flip(head->length, 3));
//    printf("type: %u\n", (unsigned int)head->type);
      type=head->type;
//    if(head->fmt==0){printf("msgid: %u\n", (unsigned int)head->msgid);}
      rtmp_lasttype=head->type;
      rtmp_lastlen=len;
    }else{
      type=rtmp_lasttype;
      len=rtmp_lastlen;
    }

    int skip=headsize+len+(len/128);
//      printf("Skipping %i bytes to next message (%i + %i + %i)\n\n", skip, headsize, len, len/128);
    *msg+=skip;
    *length-=skip;
    if(type==0x14)
    {
      unsigned char* data=((void*)head)+headsize;
      // Cut out every 128th byte (0xc3 garbage required by RTMP)
      int i;
      skip=0;
      for(i=128; i<len; i+=128)
      {
        ++skip;
// printf("Skipping garbage byte '%x' at offset %i\n", (int)data[i]&0xff, i);
        if(i+128<len)
        {
          memmove(&data[i], &data[i+skip], 128);
        }else{
          memmove(&data[i], &data[i+skip], len-i);
        }
      }
      *amflen=len;
      return data;
    }
    head=((void*)head)+skip;
  }
  return 0;
}
