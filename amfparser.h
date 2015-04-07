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
enum
{
  AMF_NUMBER,
  AMF_BOOL,
  AMF_STRING,
  AMF_OBJECT
};

struct amfstring
{
  int length;
  char* string;
};

struct amfobject
{
  int membercount;
  struct amfobjectmember* members;
  char ended; // Used in the parsing process
};

struct amfitem
{
  char type;
  union
  {
    double number;
    char boolean;
    struct amfstring string;
    struct amfobject object;
  };
};

struct amfobjectmember
{
  const char* name;
  struct amfitem value;
};

struct amf
{
  struct amfitem* items;
  int itemcount;
};

extern char amf_comparestrings_c(struct amfstring* a, const char* b);
extern char amf_comparestrings(struct amfstring* a, struct amfstring* b);
extern char amf_compareitems(struct amfitem* a, struct amfitem* b);

extern struct amf* amf_parse(const unsigned char* buf, int len);
extern void amf_free(struct amf* amf);
extern struct amfitem* amf_getobjmember(struct amfobject* obj, const char* name);

extern void printamf(struct amf* amf);
