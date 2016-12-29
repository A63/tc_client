/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2016  alicia@ion.nu

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
#include <string.h>
#ifdef HAVE_GLIB
#include <glib.h>
#endif
#include "media.h"
#include "amfwriter.h"
#include "rtmp.h"
#include "client.h"
#include "idlist.h"
#include "colors.h"

static char* joinkey=0;
static unsigned int joinkeynum;

static void joindata(struct amf* amfin, int sock)
{
  if(amfin->items[6].type!=AMF_STRING || amfin->items[12].type!=AMF_NUMBER){return;}
  joinkey=strdup(amfin->items[6].string.string);
  joinkeynum=amfin->items[12].number;
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "connectionOK");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfnum(&amf, joinkeynum);
  amfstring(&amf, joinkey); // Channel ID or something?
  amfstring(&amf, nickname);
  amfstring(&amf, ""); // Unidentified number string, doesn't seem to matter at all
  amfsend(&amf, sock);
}

static void joinuser(struct amf* amfin, int sock)
{
  if(amfin->items[2].type==AMF_STRING)
  {
    printf("%s %s entered the channel\n", timestamp(), amfin->items[2].string.string);
  }
}

static void receivepublicmsg(struct amf* amfin, int sock)
{
  if(amfin->items[3].type!=AMF_STRING || amfin->items[4].type!=AMF_STRING || amfin->items[5].type!=AMF_STRING){return;}
  if(!strcmp(amfin->items[3].string.string, nickname)){return;} // It's the message we just sent
  const char* color=color_start(amfin->items[5].string.string);
  printf("%s %s%s: %s%s\n", timestamp(), color, amfin->items[3].string.string, amfin->items[4].string.string, color_end());
}

static void removeuser(struct amf* amfin, int sock)
{
  if(amfin->items[2].type!=AMF_STRING){return;}
  idlist_remove(amfin->items[2].string.string);
  printf("%s %s left the channel\n", timestamp(), amfin->items[2].string.string);
}

static void senduserlist(struct amf* amfin, int sock)
{
  if(amfin->items[2].type!=AMF_STRING || amfin->items[2].string.string[1]!='"'){return;}
  printf("Currently online: ");
// "u" seems to be timestamp of status update
// "l" seems to be timestamp of connection
// "m" seems to be a status message (showed up as "BRB" once)
  char* nick=&amfin->items[2].string.string[2];
  char first=1;
  while(nick)
  {
    if(first){first=0;}else{printf(", "); nick=&nick[3];}
    char* end=strchr(nick, '"');
    if(!end){break;}
    end[0]=0;
    printf("%s", nick);
    idlist_add(0, nick, 0, 0);
    nick=strstr(&end[1], "\",\"");
  }
  printf("\n");
}

/*
Unknown command...
amf: 0x1eca070
amf->itemcount: 3
String: 'camList'
Number: 5.000000
String: '{"1":"{\"camno\":1,\"server\":\"<IP>\",\"server_name\":\"<name>\",\"server_location\":\"<location>\",\"camname\":\"<camid>\",\"username\":\"<nickname/username>\"}","2":"{\"camno\":2,\"server\":\"<IP>\",\"server_name\":\"<name>\",\"server_location\":\"<location>\",\"camname\":\"<camid>\",\"username\":\"<nickname/username>\"}","3":etc...}'
*/

static void startcam(struct amf* amfin, int sock)
{
  if(amfin->items[3].type!=AMF_STRING || amfin->items[4].type!=AMF_STRING || amfin->items[5].type!=AMF_STRING){return;}
  printf("%s cammed up on %s/%s/%u as %s\n", amfin->items[4].string.string, amfin->items[5].string.string, channel, joinkeynum, amfin->items[3].string.string);
}

static void stopcam(struct amf* amfin, int sock)
{
  if(amfin->items[3].type!=AMF_STRING || amfin->items[4].type!=AMF_STRING){return;}
  printf("%s cammed down\n", amfin->items[4].string.string);
// TODO: Send a VideoEnd notice?
}

static void pmreceive(struct amf* amfin, int sock)
{
  if(amfin->items[2].type!=AMF_STRING || amfin->items[3].type!=AMF_STRING || amfin->items[4].type!=AMF_STRING || amfin->items[5].type!=AMF_STRING){return;}
  if(!strcmp(amfin->items[2].string.string, nickname)){return;} // It's the message we just sent
  const char* color=color_start(amfin->items[5].string.string);
  printf("%s %s%s: /msg %s %s%s\n", timestamp(), color, amfin->items[2].string.string, amfin->items[3].string.string, amfin->items[4].string.string, color_end());
}

static void typingpm(struct amf* amfin, int sock)
{
  if(amfin->items[2].type!=AMF_STRING){return;}
  printf("%s is typing a PM\n", amfin->items[2].string.string);
}

static void kicked(struct amf* amfin, int sock)
{
  if(amfin->items[2].type!=AMF_STRING || amfin->items[3].type!=AMF_STRING || amfin->items[7].type!=AMF_STRING){return;}
  printf("%s %s was kicked by %s (%s)\n", timestamp(), amfin->items[3].string.string, amfin->items[2].string.string, amfin->items[7].string.string);
  idlist_remove(amfin->items[3].string.string);
  printf("%s %s left the channel\n", timestamp(), amfin->items[3].string.string);
}

static void result2(struct amf* amfin, int sock)
{
  if(amfin->items[2].type!=AMF_OBJECT){return;}
  struct amfitem* desc=amf_getobjmember(&amfin->items[2].object, "description");
  if(desc->type!=AMF_STRING){return;}
  if(!strcmp(desc->string.string, "{\"param\":\"\",\"reject\":\"0006\"}"))
  {
    printf("Wrong username/password\n");
  }
  else if(!strcmp(desc->string.string, "{\"param\":\"\",\"reject\":\"0008\"}"))
  {
    printf("Password required\n");
  }
  else if(!strcmp(desc->string.string, "{\"param\":\"\",\"reject\":\"0012\"}"))
  {
    printf("Account required\n");
  }
  else if(!strcmp(desc->string.string, "{\"param\":\"\",\"reject\":\"0003\"}"))
  {
    printf("Nickname unavailable\n");
  }
  else if(!strcmp(desc->string.string, "{\"param\":\"\",\"reject\":\"0005\"}"))
  {
    printf("Nickname too short\n");
  }
}

static void sendmessage(int sock, const char* buf)
{
  char color[8];
  memcpy(color, colors[currentcolor%COLORCOUNT], 7);
  color[7]=0;
  if(color[5]==',') // That one stupid tinychat color
  {
    strcpy(color, "#00a990");
  }
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "send_public");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfnum(&amf, joinkeynum);
  amfstring(&amf, joinkey);
  amfstring(&amf, nickname); // Spoofable?
  amfstring(&amf, buf);
  amfstring(&amf, "0");
  amfstring(&amf, color);
  amfstring(&amf, "1");
  amfnum(&amf, 1);
  amfsend(&amf, sock);
}

/* Not used yet
static void sendpmtyping(int sock, const char* nick)
{
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "typing_pm");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfnum(&amf, joinkeynum);
  amfstring(&amf, joinkey);
  amfstring(&amf, nickname); // Spoofable?
  amfstring(&amf, nick);
  amfstring(&amf, "1"); // 0 for not typing anymore? (or 2? from observations)
  amfsend(&amf, sock);
}
*/

static void sendpm(int sock, const char* buf, const char* nick)
{
  char color[8];
  memcpy(color, colors[currentcolor%COLORCOUNT], 7);
  color[7]=0;
  if(color[5]==',') // That one stupid tinychat color
  {
    strcpy(color, "#00a990");
  }
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "pm");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfnum(&amf, joinkeynum);
  amfstring(&amf, joinkey);
  amfstring(&amf, nickname);
  amfstring(&amf, nick);
  amfstring(&amf, buf);
  amfstring(&amf, color);
  amfstring(&amf, "1");
  amfnum(&amf, 1);
  amfsend(&amf, sock);
}

static void notimplemented()
{
  printf("The requested feature is either not supported by kageshi or not yet implemented in tc_client's kageshi support\n");
}

int init_kageshi(const char* chanpass, const char* username, const char* userpass, struct site* site)
{
#ifndef HAVE_GLIB
  if(username){printf("Account support requires glib (for md5sums)\n");}
  username=0;
#endif
  registercmd("joinData", joindata, 13);
  registercmd("joinuser", joinuser, 3);
  registercmd("receivePublicMsg", receivepublicmsg, 6);
  registercmd("removeuser", removeuser, 3);
  registercmd("sendUserList", senduserlist, 3);
  registercmd("startCam", startcam, 6);
  registercmd("stopCam", stopcam, 5);
  registercmd("pmReceive", pmreceive, 6);
  registercmd("typingPM", typingpm, 3);
  registercmd("kicked", kicked, 8);
  registercmd("_result", result2, 3);
  site->sendmessage=sendmessage;
  site->sendpm=sendpm;
  site->nick=notimplemented;
  site->mod_close=notimplemented;
  site->mod_ban=notimplemented;
  site->mod_banlist=notimplemented;
  site->mod_unban=notimplemented;
  site->mod_mute=notimplemented;
  site->mod_push2talk=notimplemented;
  site->mod_topic=notimplemented;
  site->mod_allowbroadcast=notimplemented;
  site->opencam=notimplemented;
  site->closecam=notimplemented;
  site->camup=notimplemented;
  site->camdown=notimplemented;
  int sock=connectto("107.191.96.85", "1935");
  rtmp_handshake(sock);
  // Send connect request
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "connect");
  amfnum(&amf, username?1:0);
  amfobjstart(&amf);
    amfobjitem(&amf, "app");
    {char str[strlen("chat/0")+strlen(channel)];
    sprintf(str, "chat/%s", channel);
    amfstring(&amf, str);}

    amfobjitem(&amf, "flashVer");
    amfstring(&amf, "no flash");

    amfobjitem(&amf, "swfUrl");
    amfstring(&amf, "no flash");

    amfobjitem(&amf, "tcUrl");
    {char str[strlen("rtmp://107.191.96.85:1935/chat/0")+strlen(channel)];
    sprintf(str, "rtmp://107.191.96.85:1935/chat/%s", channel);
    amfstring(&amf, str);}

    amfobjitem(&amf, "fpad");
    amfbool(&amf, 0);

    amfobjitem(&amf, "capabilities");
    amfnum(&amf, 0);

    amfobjitem(&amf, "audioCodecs");
    amfnum(&amf, 0);

    amfobjitem(&amf, "videoCodecs");
    amfnum(&amf, 0);

    amfobjitem(&amf, "videoFunction");
    amfnum(&amf, 0);

    amfobjitem(&amf, "pageUrl");
    char pageurl[strlen("https://kageshi.com/rooms/0") + strlen(channel)];
    sprintf(pageurl, "https://kageshi.com/rooms/%s", channel);
    amfstring(&amf, pageurl);

    amfobjitem(&amf, "objectEncoding");
    amfnum(&amf, 0);
  amfobjend(&amf);
  amfstring(&amf, "connect");
  amfstring(&amf, "");
  amfstring(&amf, username?"":nickname);
  if(username)
  {
  #ifdef HAVE_GLIB
    GChecksum* md5=g_checksum_new(G_CHECKSUM_MD5);
    g_checksum_update(md5, (void*)userpass, strlen(userpass));
    amfstring(&amf, username);
    amfstring(&amf, g_checksum_get_string(md5));
    g_checksum_free(md5);
  #endif
  }else{
    amfstring(&amf, "");
    amfstring(&amf, "");
  }
  amfstring(&amf, channel);
  // Not sure what these are, they don't seem to do anything, but need to be in the right format. kinda looks like HMAC
  amfstring(&amf, "00000000-0000-0000-0000-000000000000-00000000-0000-0000-0000-000000000000");
  amfstring(&amf, "00000000-0000-0000-0000-000000000000-00000000-0000-0000-0000-000000000000");
  amfstring(&amf, "0.31");
  amfstring(&amf, chanpass?chanpass:"");
  amfstring(&amf, "");
  amfbool(&amf, !!chanpass);
  amfsend(&amf, sock);
  return sock;
}

static void result(struct amf* amfin, int sock)
{
  // Handle results for various requests, haven't seen much of a pattern to it, always successful?
  // Creating a new stream worked, now play media (cam/mic) on it (if that's what the result was for)
  stream_play(amfin, sock);
  // Tell them to actually send us media
  unsigned int i;
  for(i=0; i<streamcount; ++i)
  {
    if(streams[i].streamid==amfin->items[2].number)
    {
      struct rtmp amf;
      amfinit(&amf, streams[i].streamid*6+2);
      amfstring(&amf, "receiveVideo");
      amfnum(&amf, 0);
      amfnull(&amf);
      amfbool(&amf, 1);
      amfsend(&amf, sock);
      amfinit(&amf, streams[i].streamid*6+2);
      amfstring(&amf, "receiveAudio");
      amfnum(&amf, 0);
      amfnull(&amf);
      amfbool(&amf, 1);
      amfsend(&amf, sock);
    }
  }
}

static void opencam(int sock, const char* camname)
{
  stream_start(camname, camname, sock);
}

static void closecam(int sock, const char* camname)
{
  stream_stopvideo(sock, camname);
}

static void notimplemented2()
{
  printf("The requested feature is either not supported by kageshi cam sessions or not yet implemented in tc_client's kageshi cam support\n");
}

int init_kageshicam(struct site* site)
{
  // Separate server from channel name
  char server[strlen(channel)+1];
  strcpy(server, channel);
  char* channelname=strchr(server, '/');
  if(!channelname){return -1;} // TODO: Useful error
  channelname[0]=0;
  channelname=&channelname[1];
  // Separate the channel name from the numeric key
  char* key=strchr(channelname, '/');
  if(!key){return -1;} // TODO: Useful error
  key[0]=0;
  joinkeynum=strtoul(&key[1], 0, 0);

  registercmd("_result", result, 3);
  site->opencam=opencam;
  site->closecam=closecam;
  site->sendmessage=notimplemented2;
  site->sendpm=notimplemented2;
  site->nick=notimplemented2;
  site->mod_close=notimplemented2;
  site->mod_ban=notimplemented2;
  site->mod_banlist=notimplemented2;
  site->mod_unban=notimplemented2;
  site->mod_mute=notimplemented2;
  site->mod_push2talk=notimplemented2;
  site->mod_topic=notimplemented2;
  site->mod_allowbroadcast=notimplemented2;
  site->camup=notimplemented2;
  site->camdown=notimplemented2;
  int sock=connectto(server, "1935");
  rtmp_handshake(sock);
  // Send connect request
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "connect");
  amfnum(&amf, 0);
  amfobjstart(&amf);
    amfobjitem(&amf, "app");
    {char str[snprintf(0,0,"CamServer/%u", joinkeynum)+1];
    sprintf(str, "CamServer/%u", joinkeynum);
    amfstring(&amf, str);}

    amfobjitem(&amf, "flashVer");
    amfstring(&amf, "no flash");

    amfobjitem(&amf, "swfUrl");
    amfstring(&amf, "no flash");

    amfobjitem(&amf, "tcUrl");
    {char str[snprintf(0,0,"rtmp://%s:1935/CamServer/%u", server, joinkeynum)+1];
    sprintf(str, "rtmp://%s:1935/CamServer/%u", server, joinkeynum);
    amfstring(&amf, str);}

    amfobjitem(&amf, "fpad");
    amfbool(&amf, 0);

    amfobjitem(&amf, "capabilities");
    amfnum(&amf, 0);

    amfobjitem(&amf, "audioCodecs");
    amfnum(&amf, 0);

    amfobjitem(&amf, "videoCodecs");
    amfnum(&amf, 0);

    amfobjitem(&amf, "videoFunction");
    amfnum(&amf, 0);

    amfobjitem(&amf, "pageUrl");
    char pageurl[strlen("http://kageshi.com/rooms/0") + strlen(channelname)];
    sprintf(pageurl, "http://kageshi.com/rooms/%s", channelname);
    amfstring(&amf, pageurl);

    amfobjitem(&amf, "objectEncoding");
    amfnum(&amf, 0);
  amfobjend(&amf);
  amfstring(&amf, "connect");
  amfstring(&amf, "0.1");
  amfsend(&amf, sock);
  return sock;
}
