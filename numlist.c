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
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include "numlist.h"

// Functions for converting to/from the comma-separated decimal character code format that tinychat uses for chat messages, e.g. "97,98,99" = "abc"

wchar_t* fromnumlist(char* in)
{
  int len=1;
  char* x=in;
  while((x=strchr(x, ',')))
  {
    ++len;
    x=&x[1];
  }
  unsigned char string[len+1];
  int i;
  for(i=0; i<len; ++i)
  {
    string[i]=strtol(in, &in, 0);
    if(!in){break;}
    in=&in[1];
  }
  string[len]=0;
  wchar_t* wstring=malloc(sizeof(wchar_t)*(len+1));
  setlocale(LC_ALL, "en_US.ISO-8859-1");
  mbstowcs(wstring, (char*)string, len);
  wstring[len]=0;
  setlocale(LC_ALL, "");
  return wstring;
}

char* tonumlist(const wchar_t* in_x)
{
  int len=wcslen(in_x);
  char in[len+1];
  setlocale(LC_ALL, "en_US.ISO-8859-1");
  wcstombs(in, in_x, len);
  in[len]=0;
  setlocale(LC_ALL, "");
  char* out=malloc(strlen(in)*strlen("255,"));
  out[0]=0;
  char* x=out;
  int i;
  for(i=0; in[i]; ++i)
  {
    sprintf(x, "%s%i", x==out?"":",", (int)in[i]&0xff);
    x=&x[strlen(x)];
  }
  return out;
}
