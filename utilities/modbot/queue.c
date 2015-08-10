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
#include <string.h>
#include <stdlib.h>
#include "queue.h"

void queue_del(struct queue* queue, const char* item)
{
  unsigned int i;
  unsigned int len;
  while(item[0])
  {
    if(item[0]=='\r' || item[0]=='\n'){item=&item[1]; continue;} // Skip empty lines
    for(len=0; item[len] && item[len]!='\r' && item[len]!='\n'; ++len);
    for(i=0; i<queue->itemcount; ++i)
    {
      if(!strncmp(queue->items[i].video, item, len) && !queue->items[i].video[len])
      {
        free(queue->items[i].video);
        free(queue->items[i].requester);
        free(queue->items[i].title);
        --queue->itemcount;
        memmove(&queue->items[i], &queue->items[i+1], sizeof(struct queueitem)*(queue->itemcount-i));
      }
    }
    item=&item[len];
  }
}

void queue_add(struct queue* queue, const char* item, const char* requester, const char* title, unsigned int timeoffset)
{
  queue_del(queue, item);
  unsigned int len;
  while(item[0])
  {
    if(item[0]=='\r' || item[0]=='\n'){item=&item[1]; continue;} // Skip empty lines
    for(len=0; item[len] && item[len]!='\r' && item[len]!='\n'; ++len);
    ++queue->itemcount;
    queue->items=realloc(queue->items, sizeof(struct queueitem)*queue->itemcount);
    queue->items[queue->itemcount-1].video=strndup(item, len);
    queue->items[queue->itemcount-1].requester=strdup(requester);
    queue->items[queue->itemcount-1].title=strdup(title);
    queue->items[queue->itemcount-1].timeoffset=timeoffset*1000;
    item=&item[len];
  }
}

int queue_getpos(struct queue* queue, char* item)
{
  int i;
  unsigned int len;
  while(item[0])
  {
    if(item[0]=='\r' || item[0]=='\n'){item=&item[1]; continue;} // Skip empty lines
    for(len=0; item[len] && item[len]!='\r' && item[len]!='\n'; ++len);
    for(i=0; i<queue->itemcount; ++i)
    {
      if(!strncmp(queue->items[i].video, item, len) && !queue->items[i].video[len]){return i;}
    }
    item=&item[len];
  }
  return -1;
}

void queue_movetofront(struct queue* queue, unsigned int pos)
{
  if(pos>=queue->itemcount){return;}
  struct queueitem move=queue->items[pos];
  memmove(&queue->items[1], queue->items, sizeof(struct queueitem)*pos);
  queue->items[0]=move;
}
