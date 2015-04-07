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
#pragma pack(push)
#pragma pack(1)
struct rtmph
{
  unsigned int streamid:6;
  unsigned int fmt:2;
  unsigned int timestamp:24;
  unsigned int length:24;
  unsigned char type;
  unsigned int msgid;
};
#pragma pack(pop)

extern unsigned char* rtmp_getamf(unsigned char** msg, int* length, int* amflen);
