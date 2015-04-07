/*
    cursedchat, a simple curses interface for tc_client
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
#include <string.h>
#include <stdlib.h>
#include <curses.h>
#include "buffer.h"

struct buffer* buffers=0;
unsigned int buffercount=0;
unsigned int currentbuf=0;

unsigned int createbuffer(const char* name)
{
  ++buffercount;
  buffers=realloc(buffers, buffercount*sizeof(struct buffer));
  buffers[buffercount-1].pad=newpad(2048, COLS);
  scrollok(buffers[buffercount-1].pad, 1);
  buffers[buffercount-1].name=(name?strdup(name):0);
  buffers[buffercount-1].scroll=-1;
  buffers[buffercount-1].seen=1;
  return buffercount-1;
}

unsigned int findbuffer(const char* name)
{
  unsigned int i;
  for(i=1; i<buffercount; ++i)
  {
    if(!strcmp(buffers[i].name, name)){return i;}
  }
  return 0;
}

void renamebufferunique(unsigned int id)
{
  unsigned int i=0;
  while(i<buffercount)
  {
    int len=strlen(buffers[id].name);
    buffers[id].name=realloc(buffers[id].name, len+2);
    buffers[id].name[len]='_';
    buffers[id].name[len+1]=0;
    for(i=1; i<buffercount; ++i)
    {
      if(i!=id && !strcmp(buffers[i].name, buffers[id].name)){break;}
    }
  }
}
