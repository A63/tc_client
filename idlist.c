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
#include <stdlib.h>
#include <string.h>
#include "idlist.h"

struct idmap* idlist=0;
int idlistlen=0;

void idlist_add(int id, const char* name)
{
  idlist_remove(name);
  ++idlistlen;
  idlist=realloc(idlist, sizeof(struct idmap)*idlistlen);
  idlist[idlistlen-1].id=id;
  idlist[idlistlen-1].name=strdup(name);
}

void idlist_remove(const char* name)
{
  int i;
  for(i=0; i<idlistlen; ++i)
  {
    if(!strcmp(name, idlist[i].name))
    {
      free((void*)idlist[i].name);
      --idlistlen;
      memmove(&idlist[i], &idlist[i+1], sizeof(struct idmap)*(idlistlen-i));
      return;
    }
  }
}

void idlist_rename(const char* oldname, const char* newname)
{
  int i;
  for(i=0; i<idlistlen; ++i)
  {
    if(!strcmp(oldname, idlist[i].name))
    {
      free((void*)idlist[i].name);
      idlist[i].name=strdup(newname);
      return;
    }
  }
}

int idlist_get(const char* name)
{
  int len;
  for(len=0; name[len]&&name[len]!=' '; ++len);
  int i;
  for(i=0; i<idlistlen; ++i)
  {
    if(!strncmp(name, idlist[i].name, len) && !idlist[i].name[len])
    {
      return idlist[i].id;
    }
  }
  return -1;
}
