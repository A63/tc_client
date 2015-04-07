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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iconv.h>
#include "numlist.h"

// Functions for converting to/from the comma-separated decimal character code format that tinychat uses for chat messages, e.g. "97,98,99" = "abc"

char* fromnumlist(char* in, size_t* outlen)
{
  size_t len=1;
  char* x=in;
  while((x=strchr(x, ',')))
  {
    ++len;
    x=&x[1];
  }
  unsigned short string[len+1];
  int i;
  for(i=0; i<len; ++i)
  {
    string[i]=strtol(in, &in, 0);
    if(!in){break;}
    in=&in[1];
  }
  string[len]=0;

  iconv_t cd=iconv_open("", "utf-16");
  char* outbuf=malloc(len*4);
  char* i_out=outbuf;
  char* i_in=(char*)string;
  size_t remaining=len*4;
  *outlen=remaining;
  len*=2; // 2 bytes per number/character in utf-16
  while(outlen>0 && len>0 && iconv(cd, &i_in, &len, &i_out, &remaining)>0);
  i_out[0]=0; // null-terminates 'outbuf'
  iconv_close(cd);
  *outlen-=remaining;
  return outbuf;
}

char* tonumlist(char* i_in, size_t len)
{
  iconv_t cd=iconv_open("utf-16le", "");
  unsigned short in[len+1];
  char* i_out=(char*)in;
  size_t outlen=len*2; // 2 bytes per character in utf-16
  while(outlen>0 && len>0 && iconv(cd, &i_in, &len, &i_out, &outlen)>0);
  iconv_close(cd);
  len=((void*)i_out-(void*)in)/2;

  char* out=malloc(len*strlen("65535,"));
  out[0]=0;
  char* x=out;
  int i;
  for(i=0; i<len; ++i)
  {
    sprintf(x, "%s%u", x==out?"":",", (unsigned int)in[i]&0xffff);
    x=&x[strlen(x)];
  }
  return out;
}
