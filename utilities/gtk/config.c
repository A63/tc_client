/*
    tc_client-gtk, a graphical user interface for tc_client
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "config.h"

struct configitem
{
  const char* name;
  char* value;
};

struct configitem* configitems=0;
unsigned int configitemcount=0;

void config_load(void)
{
  const char* home=getenv("HOME");
  char filename[strlen(home)+strlen("/.config/tc_client-gtk0")];
  sprintf(filename, "%s/.config/tc_client-gtk", home);
  FILE* f=fopen(filename, "r");
  if(!f){return;}
  char buf[2048];
  while(fgets(buf, 2048, f))
  {
    char* sep=strchr(buf, ':');
    if(!sep){continue;}
    sep[0]=0;
    char* value=&sep[1];
    while(value[0]==' '){value=&value[1];}
    while((sep=strchr(value, '\r'))||(sep=strchr(value, '\n'))){sep[0]=0;}
    ++configitemcount;
    configitems=realloc(configitems, sizeof(struct configitem)*configitemcount);
    configitems[configitemcount-1].name=strdup(buf);
    configitems[configitemcount-1].value=strdup(value);
  }
  fclose(f);
}

void config_save(void)
{
  const char* home=getenv("HOME");
  char filename[strlen(home)+strlen("/.config/tc_client-gtk0")];
  sprintf(filename, "%s/.config", home);
  mkdir(filename, 0700);
  strcat(filename, "/tc_client-gtk");
  FILE* f=fopen(filename, "w");
  if(!f){perror("fopen(~/.config/tc_client-gtk)"); return;}
  unsigned int i;
  for(i=0; i<configitemcount; ++i)
  {
    fprintf(f, "%s: %s\n", configitems[i].name, configitems[i].value);
  }
  fclose(f);
}

char config_get_bool(const char* name)
{
  unsigned int i;
  for(i=0; i<configitemcount; ++i)
  {
    if(!strcmp(configitems[i].name, name)){return !strcasecmp(configitems[i].value, "True");}
  }
  return 0;
}

const char* config_get_str(const char* name)
{
  unsigned int i;
  for(i=0; i<configitemcount; ++i)
  {
    if(!strcmp(configitems[i].name, name)){return configitems[i].value;}
  }
  return "";
}

int config_get_int(const char* name)
{
  unsigned int i;
  for(i=0; i<configitemcount; ++i)
  {
    if(!strcmp(configitems[i].name, name)){return atoi(configitems[i].value);}
  }
  return 0;
}

void config_set(const char* name, const char* value)
{
  unsigned int i;
  for(i=0; i<configitemcount; ++i)
  {
    if(!strcmp(configitems[i].name, name))
    {
      free(configitems[i].value);
      configitems[i].value=strdup(value);
      return;
    }
  }
  ++configitemcount;
  configitems=realloc(configitems, sizeof(struct configitem)*configitemcount);
  configitems[configitemcount-1].name=strdup(name);
  configitems[configitemcount-1].value=strdup(value);
}

void config_set_int(const char* name, int value)
{
  int size=snprintf(0,0, "%i", value);
  char buf[size+1];
  sprintf(buf, "%i", value);
  config_set(name, buf);
}
