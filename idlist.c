/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2014-2017  alicia@ion.nu
    Copyright (C) 2014-2015  Jade Lea

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

void idlist_add(int id, const char* name, const char* account, char op)
{
  idlist_remove(name);
  ++idlistlen;
  idlist=realloc(idlist, sizeof(struct idmap)*idlistlen);
  idlist[idlistlen-1].id=id;
  idlist[idlistlen-1].op=op;
  idlist[idlistlen-1].name=strdup(name);
  idlist[idlistlen-1].account=(account?strdup(account):0);
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

void idlist_removeid(int id)
{
  int i;
  for(i=0; i<idlistlen; ++i)
  {
    if(idlist[i].id==id)
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

void idlist_renameid(int id, const char* newname)
{
  int i;
  for(i=0; i<idlistlen; ++i)
  {
    if(idlist[i].id==id)
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

const char* idlist_getaccount(const char* name)
{
  int len;
  for(len=0; name[len]&&name[len]!=' '; ++len);
  int i;
  for(i=0; i<idlistlen; ++i)
  {
    if(!strncmp(name, idlist[i].name, len) && !idlist[i].name[len])
    {
      return idlist[i].account;
    }
  }
  return 0;
}

char idlist_is_op(const char* name)
{
  int i;
  for(i=0; i<idlistlen; ++i)
  {
    if(!strcmp(name, idlist[i].name))
    {
      return idlist[i].op;
    }
  }
  return 0;
}

const char* idlist_getnick(int id)
{
  int i;
  for(i=0; i<idlistlen; ++i)
  {
    if(idlist[i].id==id)
    {
      return idlist[i].name;
    }
  }
  return 0;
}
