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
#include <string.h>
#include "colors.h"

// Sorted like rainbows
const char* colors[]={
  "#821615,en",
  "#c53332,en",
  "#a08f23,en",
  "#a78901,en",
  "#919104,en",
  "#7bb224,en",
  "#7db257,en",
  "#487d21,en",
  "#a990,en",
  "#32a5d9,en",
  "#1d82eb,en",
  "#1965b6,en",
  "#5c1a7a,en",
  "#9d5bb5,en",
  "#c356a3,en",
  "#b9807f,en"
};

const char* termcolors[]={
  "31",
  "31;1",
  "33",
  "33",
  "33;1",
  "32;1",
  "32;1",
  "32",
  "36",
  "34;1",
  "34;1",
  "34",
  "35",
  "35;1",
  "35;1",
  "35;1"
};

const char* resolvecolor(const char* tc_color)
{
  int i;
  for(i=0; i<16; ++i)
  {
    if(!strcmp(colors[i], tc_color)){return termcolors[i];}
  }
  return "0";
}
