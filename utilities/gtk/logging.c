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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include "config.h"
#include "../stringutils.h"
#include "../compat.h"

struct logfile
{
  FILE* f;
  const char* path;
};
struct logfile* logfiles=0;
unsigned int logfilecount=0;

void logger_write(const char* line, const char* channel, const char* nick)
{
  const char* home=getenv("HOME");
  if(!home){home=".";}
  const char* path=config_get_str(nick?"logpath_pm":"logpath_channel");
  int namelen=strlen(path)+1;
  namelen+=strcount(path, "%h")*(strlen(home)-2);
  namelen+=strcount(path, "%c")*(strlen(channel)-2);
  if(nick){namelen+=strcount(path, "%n")*(strlen(nick)-2);}
  char filename[namelen];
  filename[0]=0;
  int len;
  while(path[0])
  {
    if(!strncmp(path, "%h", 2)){strcat(filename, home); path=&path[2]; continue;}
    if(!strncmp(path, "%c", 2)){strcat(filename, channel); path=&path[2]; continue;}
    if(nick && !strncmp(path, "%n", 2)){strcat(filename, nick); path=&path[2]; continue;}
    for(len=0; path[len] && strncmp(&path[len], "%h", 2) && strncmp(&path[len], "%c", 2) && (!nick||strncmp(&path[len], "%n", 2)); ++len);
    strncat(filename, path, len);
    path=&path[len];
  }
  unsigned int i;
  for(i=0; i<logfilecount; ++i)
  {
    if(!strcmp(filename, logfiles[i].path)){break;}
  }
  if(i==logfilecount)
  {
    ++logfilecount;
    logfiles=realloc(logfiles, logfilecount*sizeof(struct logfile));
    logfiles[i].path=strdup(filename);
    // Make sure the whole path exists
    char* sep=filename;
    while((sep=strchr(sep, '/')))
    {
      sep[0]=0;
// printf("Creating '%s' if it doesn't exist yet\n", filename);
      mkdir(filename, 0700);
      sep[0]='/';
      sep=&sep[1];
    }
    logfiles[i].f=fopen(logfiles[i].path, "a");
    if(!logfiles[i].f)
    {
      perror("fopen(logfile)");
      free((void*)logfiles[i].path);
      --logfilecount;
      return;
    }
    time_t timebuf=time(0);
    fprintf(logfiles[i].f, "Opening logfile on %s\n", ctime(&timebuf));
  }
  fprintf(logfiles[i].f, "%s\n", line);
  fflush(logfiles[i].f);
}

void logger_close_all(void)
{
  unsigned int i;
  for(i=0; i<logfilecount; ++i)
  {
    fclose(logfiles[i].f);
    free((void*)logfiles[i].path);
  }
  free(logfiles);
  logfiles=0;
  logfilecount=0;
}
