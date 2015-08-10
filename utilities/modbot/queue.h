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
struct queueitem
{
  char* video;
  char* requester;
  char* title;
  unsigned int timeoffset;
};

struct queue
{
  struct queueitem* items;
  unsigned int itemcount;
};

extern void queue_del(struct queue* queue, const char* item);
extern void queue_add(struct queue* queue, const char* item, const char* requester, const char* title, unsigned int timeoffset);
extern int queue_getpos(struct queue* queue, char* item);
extern void queue_movetofront(struct queue* queue, unsigned int pos);
