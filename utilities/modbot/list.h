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
struct list
{
  char** items;
  unsigned int itemcount;
};

extern void list_del(struct list* list, const char* item);
extern void list_add(struct list* list, const char* item);
extern void list_switch(struct list* list, char* olditem, char* newitem);
extern int list_getpos(struct list* list, char* item);
extern char list_contains(struct list* list, char* item);
extern void list_load(struct list* list, const char* file);
extern void list_save(struct list* list, const char* file);
