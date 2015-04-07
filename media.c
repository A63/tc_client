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
#include <stdio.h>
#include <stdlib.h>
//#include "rtmp.h"
//#include "amfparser.h"
#include "endian.h"
#include "media.h"
#include "amfwriter.h"
#include "idlist.h"
/*
struct stream
{
  unsigned int streamid;
  unsigned int userid;
};
*/

struct stream* streams=0;
unsigned int streamcount=0;

char stream_idtaken(unsigned int id)
{
  unsigned int i;
  for(i=0; i<streamcount; ++i)
  {
    if(streams[i].streamid==id){return 1;}
  }
  return 0;
}

void stream_start(const char* nick, int sock) // called upon privmsg "/cam ..."
{
  unsigned int userid=idlist_get(nick);
/*
  unsigned int id=idlist_get((char*)&buf[5]);
  camid=malloc(128);
  sprintf(camid, "%u", id);
*/
  unsigned int streamid=1;
  while(stream_idtaken(streamid)){++streamid;}
  ++streamcount;
  streams=realloc(streams, sizeof(struct stream)*streamcount);
  streams[streamcount-1].userid=userid;
  streams[streamcount-1].streamid=streamid;
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "createStream");
  amfnum(&amf, streamid+1);
  amfnull(&amf);
  amfsend(&amf, sock);
  printf("Starting media stream for %s (%u)\n", nick, userid);
}

void stream_play(struct amf* amf, int sock) // called upon _result
{
  unsigned int i;
  for(i=0; i<streamcount; ++i)
  {
    if(streams[i].streamid==amf->items[2].number)
    {
      struct rtmp amf;
      amfinit(&amf, 8);
      amfstring(&amf, "play");
      amfnum(&amf, 0);
      amfnull(&amf);
      char camid[snprintf(0,0,"%u0", streams[i].userid)];
      sprintf(camid, "%u", streams[i].userid);
      amfstring(&amf, camid);
      amf.msgid=le32(streams[i].streamid);
      amfsend(&amf, sock);
      return;
    }
  }
}

void stream_handledata(struct rtmp* rtmp)
{
rtmp->msgid=1;
  unsigned int i;
  for(i=0; i<streamcount; ++i)
  {
    if(streams[i].streamid!=rtmp->msgid){continue;}
    printf("Video: %u %u\n", streams[i].userid, rtmp->length); // TODO: if this becomes permanent we will have to specify a nick or ID or something
//  write(1, rtmp.buf, rtmp.length);
    fwrite(rtmp->buf, rtmp->length, 1, stdout);
    fflush(stdout);
    return;
  }
  printf("Received media data to unknown stream ID %u\n", rtmp->msgid);
}
