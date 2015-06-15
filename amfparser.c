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
#include <stdio.h> // For debugging
#include "endian.h"
#include "amfparser.h"

char amf_comparestrings_c(struct amfstring* a, const char* b)
{
  int blen=strlen(b);
  if(a->length!=blen){return 0;}
  return !strncmp(a->string, b, blen);
}

char amf_comparestrings(struct amfstring* a, struct amfstring* b)
{
  if(a->length!=b->length){return 0;}
  return !strncmp(a->string, b->string, a->length);
}

char amf_compareitems(struct amfitem* a, struct amfitem* b)
{
  if(a->type!=b->type){return 0;}
  char ret=0;
  switch(a->type)
  {
  case AMF_STRING:
    ret=amf_comparestrings(&a->string, &b->string);
    break;
  }
  return ret;
}

struct amfitem* amf_newitem(struct amf* amf)
{
  ++amf->itemcount;
  amf->items=realloc(amf->items, sizeof(struct amfitem)*amf->itemcount);
  return &amf->items[amf->itemcount-1];
}

struct amf* amf_parse(const unsigned char* buf, int len)
{
  struct amf* amf=malloc(sizeof(struct amf));
  amf->items=0;
  amf->itemcount=0;
  struct amfitem* item;
  struct amfobject* obj;
  int i;
  const unsigned char* end=&buf[len];
  while(buf<end)
  {
    obj=0;
    // Figure out if the last item is an unfinished object
    if(amf->itemcount && amf->items[amf->itemcount-1].type==AMF_OBJECT && !amf->items[amf->itemcount-1].object.ended)
    {
      obj=&amf->items[amf->itemcount-1].object;
      // TODO: recurse into unfinished member-objects (unimportant, I haven't seen any objects within objects so far)
      // Add member and set name
      unsigned short x;
      memcpy(&x, buf, sizeof(x));
      i=be16(x);
      buf=&buf[sizeof(short)];
      if(&buf[i]>=end){printf("Warning: skipping object item with name exceeding RTMP size (0x%x)\n", i);}
      if(&buf[i]<end && buf[i]!=9) // 9=end-of-object
      {
        ++obj->membercount;
        obj->members=realloc(obj->members, sizeof(struct amfobjectmember)*obj->membercount);
        obj->members[obj->membercount-1].name=strndup((char*)buf, i);
        buf=&buf[i];
      }else{ // String-length 0, 
        obj->ended=1;
        obj=0;
        buf=&buf[1]; // Skip object-end
        continue;
      }
    }
    switch(buf[0])
    {
    case 0:
      buf=&buf[1];
      if(obj)
        item=&obj->members[obj->membercount-1].value;
      else
        item=amf_newitem(amf);
      item->type=AMF_NUMBER;
      memcpy(&item->number, buf, sizeof(double));
      unsigned long long* endian=(void*)&item->number;
      *endian=be64(*endian);
      buf=&buf[sizeof(double)];
      break;
    case 1:
      buf=&buf[1];
      if(obj)
        item=&obj->members[obj->membercount-1].value;
      else
        item=amf_newitem(amf);
      item->type=AMF_BOOL;
      item->boolean=buf[0];
      buf=&buf[1];
      break;
    case 2:
      buf=&buf[1];
      if(obj)
        item=&obj->members[obj->membercount-1].value;
      else
        item=amf_newitem(amf);
      unsigned short x=0;
      memcpy(&x, buf, sizeof(x));
      i=be16(x);
      buf=&buf[sizeof(short)];
      item->type=AMF_STRING;
      item->string.length=i;
      item->string.string=strndup((char*)buf, i);
// printf("Allocated AMF string '%s' (%p)\n", item->string.string, item->string.string);
      buf=&buf[i];
      break;
    case 3:
      buf=&buf[1];
      if(obj)
        item=&obj->members[obj->membercount-1].value;
      else
        item=amf_newitem(amf);
      item->type=AMF_OBJECT;
      item->object.members=0;
      item->object.membercount=0;
      item->object.ended=0;
      break;
    case 5: // Null element
      buf=&buf[1];
      break;
    default:
      printf("Unknown datatype %i\n", (int)buf[0]);
      buf=end;
      break;
    }
  }
  return amf;
}

void amf_freeitem(struct amfitem* item)
{
  if(item->type==AMF_STRING)
  {
    free(item->string.string);
  }
  else if(item->type==AMF_OBJECT)
  {
    int i;
    for(i=0; i<item->object.membercount; ++i)
    {
      free((void*)item->object.members[i].name);
      amf_freeitem(&item->object.members[i].value);
    }
    free(item->object.members);
  }
}

void amf_free(struct amf* amf)
{
  int i;
  for(i=0; i<amf->itemcount; ++i)
  {
    amf_freeitem(&amf->items[i]);
  }
  free(amf->items);
  free(amf);
}

struct amfitem* amf_getobjmember(struct amfobject* obj, const char* name)
{
  unsigned int i;
  for(i=0; i<obj->membercount; ++i)
  {
    if(!strcmp(obj->members[i].name, name)){return &obj->members[i].value;}
  }
  return 0;
}

void printamfobject(struct amfobject* obj, int indent)
{
  int i, j;
  for(i=0; i<obj->membercount; ++i)
  {
    for(j=0; j<indent; ++j){printf("  ");}
    printf("%s: ", obj->members[i].name);
    if(obj->members[i].value.type==AMF_NUMBER)
    {
      printf("Number: %f\n", obj->members[i].value.number);
    }
    else if(obj->members[i].value.type==AMF_BOOL)
    {
      printf("Bool: %s\n", obj->members[i].value.boolean?"True":"False");
    }
    else if(obj->members[i].value.type==AMF_STRING)
    {
      printf("String: %s\n", obj->members[i].value.string.string);
    }
    else if(obj->members[i].value.type==AMF_OBJECT)
    {
      printf("Object:\n");
      printamfobject(&obj->members[i].value.object, indent+1);
    }
    else{printf("(Unhandled type)\n");}
  }
}

void printamf(struct amf* amf)
{
  printf("amf: %p\n", amf);
  printf("amf->itemcount: %i\n", amf->itemcount);
  int i;
  for(i=0; i<amf->itemcount; ++i)
  {
    if(amf->items[i].type==AMF_NUMBER)
    {
      printf("Number: %f\n", amf->items[i].number);
    }
    else if(amf->items[i].type==AMF_BOOL)
    {
      printf("Bool: %s\n", amf->items[i].boolean?"True":"False");
    }
    else if(amf->items[i].type==AMF_STRING)
    {
      printf("String: '%s'\n", amf->items[i].string.string);
    }
    else if(amf->items[i].type==AMF_OBJECT)
    {
      printf("Object:\n");
      printamfobject(&amf->items[i].object, 1);
    }
    else{printf("(Unhandled type)\n");}
  }
}
