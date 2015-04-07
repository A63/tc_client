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

struct amfmsg
{
  unsigned int len;
  unsigned char* buf;
};

extern void amfinit(struct amfmsg* msg);
extern void amfsend(struct amfmsg* msg, int sock);
extern void amfnum(struct amfmsg* msg, double v);
extern void amfbool(struct amfmsg* msg, char v);
extern void amfstring(struct amfmsg* msg, char* string);
extern void amfobjstart(struct amfmsg* msg);
extern void amfobjitem(struct amfmsg* msg, char* name);
extern void amfobjend(struct amfmsg* msg);
extern void amfnull(struct amfmsg* msg);
