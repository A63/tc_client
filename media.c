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
#include <string.h>
#include "endian.h"
#include "media.h"
#include "amfwriter.h"
#include "idlist.h"

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

void stream_start(const char* nick, int sock) // called upon privmsg "/opencam ..."
{
  unsigned int userid=idlist_get(nick);
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
  unsigned int i;
  for(i=0; i<streamcount; ++i)
  {
    if(streams[i].streamid!=rtmp->msgid){continue;}
// fprintf(stderr, "Chunk: chunkid: %u, streamid: %u, userid: %u\n", rtmp->chunkid, rtmp->msgid, streams[i].userid);
    if(rtmp->type==RTMP_VIDEO)
    {
      printf("Video: %u %u\n", streams[i].userid, rtmp->length);
    }else if(rtmp->type==RTMP_AUDIO){
      printf("Audio: %u %u\n", streams[i].userid, rtmp->length);
    }
    fwrite(rtmp->buf, rtmp->length, 1, stdout);
    fflush(stdout);
    return;
  }
  printf("Received media data to unknown stream ID %u\n", rtmp->msgid);
}

void stream_handlestatus(struct amf* amf)
{
  if(amf->itemcount<3 || amf->items[2].type!=AMF_OBJECT){return;}
  struct amfobject* obj=&amf->items[2].object;
  struct amfitem* code=amf_getobjmember(obj, "code");
  struct amfitem* details=amf_getobjmember(obj, "details");
  if(!code || !details){return;}
  if(code->type!=AMF_STRING || details->type!=AMF_STRING){return;}
  if(!strcmp(code->string.string, "NetStream.Play.Stop"))
  {
    unsigned int id=strtoul(details->string.string, 0, 0);
    unsigned int i;
    for(i=0; i<streamcount; ++i)
    {
      if(streams[i].userid==id)
      {
        printf("VideoEnd: %u\n", streams[i].userid);
        // Note: not removing it from the list because tinychat doesn't seem to handle reusing stream IDs
        return;
      }
    }
  }
}
