/*
    cursedchat, a simple curses interface for tc_client
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
struct buffer
{
  WINDOW* pad;
  char* name;
  int scroll;
  char seen;
};

extern struct buffer* buffers;
extern unsigned int buffercount;
extern unsigned int currentbuf;

extern unsigned int createbuffer(const char* name);
extern unsigned int findbuffer(const char* name);
extern void renamebufferunique(unsigned int id);
