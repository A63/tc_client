/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2015-2016  alicia@ion.nu

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
#include "endianutils.h"
#include "media.h"
#include "amfwriter.h"
#include "idlist.h"

struct stream* streams=0;
unsigned int streamcount=0;
char allowsnapshots=1;

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
  streams[streamcount-1].outgoing=0;
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "createStream");
  amfnum(&amf, streamid+1);
  amfnull(&amf);
  amfsend(&amf, sock);
  printf("Starting media stream for %s (%u)\n", nick, userid);
  fflush(stdout);
}

void streamout_start(unsigned int id, int sock) // called upon privmsg "/camup"
{
  unsigned int streamid=1;
  while(stream_idtaken(streamid)){++streamid;}
  ++streamcount;
  streams=realloc(streams, sizeof(struct stream)*streamcount);
  streams[streamcount-1].userid=id;
  streams[streamcount-1].streamid=streamid;
  streams[streamcount-1].outgoing=1;
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "createStream");
  amfnum(&amf, streamid+1);
  amfnull(&amf);
  amfsend(&amf, sock);
  printf("Starting outgoing media stream\n");
  fflush(stdout);
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
      amfstring(&amf, streams[i].outgoing?"publish":"play");
      amfnum(&amf, 0);
      amfnull(&amf);
      char camid[snprintf(0,0,"%u0", streams[i].userid)];
      sprintf(camid, "%u", streams[i].userid);
      amfstring(&amf, camid);
      if(streams[i].outgoing){amfstring(&amf, "live");}
      amf.msgid=le32(streams[i].streamid);
      amfsend(&amf, sock);
      if(!allowsnapshots && streams[i].outgoing) // Prevent snapshots
      {
        amfinit(&amf, 3);
        amf.type=RTMP_INVOKE;
        amf.msgid=le32(streams[i].streamid);
        amfstring(&amf, "|RtmpSampleAccess");
        amfbool(&amf, 0);
        amfbool(&amf, 0);
        amfnull(&amf);
        amfsend(&amf, sock);
      }
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
  fflush(stdout);
}

void stream_handlestatus(struct amf* amf, int sock)
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
    printf("VideoEnd: %u\n", id);
    stream_stopvideo(sock, id);
  }
}

void stream_sendframe(int sock, void* buf, size_t len, unsigned char type)
{
  unsigned int i;
  for(i=0; i<streamcount; ++i)
  {
    if(streams[i].outgoing)
    {
      struct rtmp msg;
      msg.type=type;
      msg.chunkid=6;
      msg.length=len;
      msg.msgid=streams[i].streamid;
      msg.buf=buf;
      rtmp_send(sock, &msg);
      return;
    }
  }
}

void stream_stopvideo(int sock, unsigned int id)
{
  unsigned int i;
  for(i=0; i<streamcount; ++i)
  {
    if(streams[i].userid==id)
    {
      struct rtmp amf;
      // Close the stream
      amfinit(&amf, 8);
      amfstring(&amf, "closeStream");
      amfnum(&amf, 0);
      amfnull(&amf);
      amf.msgid=le32(streams[i].streamid);
      amfsend(&amf, sock);
      // Delete the stream
      amfinit(&amf, 3);
      amfstring(&amf, "deleteStream");
      amfnum(&amf, 0);
      amfnull(&amf);
      amfnum(&amf, streams[i].streamid);
      amfsend(&amf, sock);
      // Remove from list of streams
      --streamcount;
      memmove(&streams[i], &streams[i+1], sizeof(struct stream)*(streamcount-i));
      return;
    }
  }
}

void setallowsnapshots(int sock, char v)
{
  allowsnapshots=v;
  // Update any active stream as well
  unsigned int i;
  for(i=0; i<streamcount; ++i)
  {
    if(streams[i].outgoing)
    {
      struct rtmp amf;
      amfinit(&amf, 3);
      amf.type=RTMP_INVOKE;
      amf.msgid=le32(streams[i].streamid);
      amfstring(&amf, "|RtmpSampleAccess");
      amfbool(&amf, v);
      amfbool(&amf, v);
      amfnull(&amf);
      amfsend(&amf, sock);
    }
  }
}
