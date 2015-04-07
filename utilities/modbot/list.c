/*
    modbot, a bot for tc_client that queues and plays videos
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
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include "list.h"

void list_del(struct list* list, const char* item)
{
  unsigned int i;
  unsigned int len;
  while(item[0])
  {
    if(item[0]=='\r' || item[0]=='\n'){item=&item[1]; continue;} // Skip empty lines
    for(len=0; item[len] && item[len]!='\r' && item[len]!='\n'; ++len);
    for(i=0; i<list->itemcount; ++i)
    {
      if(!strncmp(list->items[i], item, len) && !list->items[i][len])
      {
        free(list->items[i]);
        --list->itemcount;
        memmove(&list->items[i], &list->items[i+1], sizeof(char*)*(list->itemcount-i));
      }
    }
    item=&item[len];
  }
}

void list_add(struct list* list, const char* item)
{
  list_del(list, item);
  unsigned int len;
  while(item[0])
  {
    if(item[0]=='\r' || item[0]=='\n'){item=&item[1]; continue;} // Skip empty lines
    for(len=0; item[len] && item[len]!='\r' && item[len]!='\n'; ++len);
    ++list->itemcount;
    list->items=realloc(list->items, sizeof(char*)*list->itemcount);
    list->items[list->itemcount-1]=strndup(item, len);
    item=&item[len];
  }
}

void list_switch(struct list* list, char* olditem, char* newitem)
{
  unsigned int i;
  for(i=0; i<list->itemcount; ++i)
  {
    if(!strcmp(list->items[i], olditem))
    {
      free(list->items[i]);
      list->items[i]=strdup(newitem);
    }
  }
}

int list_getpos(struct list* list, char* item)
{
  int i;
  unsigned int len;
  while(item[0])
  {
    if(item[0]=='\r' || item[0]=='\n'){item=&item[1]; continue;} // Skip empty lines
    for(len=0; item[len] && item[len]!='\r' && item[len]!='\n'; ++len);
    for(i=0; i<list->itemcount; ++i)
    {
      if(!strncmp(list->items[i], item, len) && !list->items[i][len]){return i;}
    }
    item=&item[len];
  }
  return -1;
}

char list_contains(struct list* list, char* item)
{
  return (list_getpos(list, item)!=-1);
}

void list_load(struct list* list, const char* file)
{
  struct stat st;
  if(stat(file, &st)){return;}
  int f=open(file, O_RDONLY);
  char buf[st.st_size+1];
  read(f, buf, st.st_size);
  buf[st.st_size]=0;
  close(f);
  char* start=buf;
  char* end;
  while((end=strchr(start, '\n'))) // TODO: support \r?
  {
    end[0]=0;
    list_add(list, start);
    start=&end[1];
  }
  printf("Loaded %u lines from %s\n", list->itemcount, file);
}

void list_save(struct list* list, const char* file)
{
  int f=open(file, O_WRONLY|O_TRUNC|O_CREAT, 0600);
  unsigned int i;
  for(i=0; i<list->itemcount; ++i)
  {
    write(f, list->items[i], strlen(list->items[i]));
    write(f, "\n", 1);
  }
  close(f);
}
