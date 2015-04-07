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
#include "amfparser.h"
#include "rtmp.h"
struct stream
{
  unsigned int streamid;
  unsigned int userid;
};

extern struct stream* streams;
extern unsigned int streamcount;

extern void stream_start(const char* nick, int sock); // called upon privmsg "/cam ..."
extern void stream_play(struct amf* amf, int sock); // called upon _result
extern void stream_handledata(struct rtmp* rtmp);
