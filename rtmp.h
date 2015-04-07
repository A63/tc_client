/*
    tc_client, a simple non-flash client for tinychat(.com)
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
#define RTMP_SET_PACKET_SIZE 0x01
#define RTMP_PING            0x04
#define RTMP_SERVER_BW       0x05
#define RTMP_CLIENT_BW       0x06
#define RTMP_AUDIO           0x08
#define RTMP_VIDEO           0x09
#define RTMP_AMF3            0x11
#define RTMP_INVOKE          0x12
#define RTMP_AMF0            0x14

struct rtmp
{
  unsigned char type;
  unsigned int chunkid;
  unsigned int length;
  unsigned int msgid;
  void* buf;
};

extern char rtmp_get(int sock, struct rtmp* rtmp);
extern void rtmp_send(int sock, struct rtmp* rtmp);
