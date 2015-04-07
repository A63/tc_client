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
struct idmap
{
  const char* name;
  int id;
  char op;
};

extern struct idmap* idlist;
extern int idlistlen;

extern void idlist_add(int id, const char* name);
extern void idlist_remove(const char* name);
extern void idlist_rename(const char* oldname, const char* newname);
extern int idlist_get(const char* name);
extern void idlist_set_op(const char* name);
extern char idlist_is_op(const char* name);
