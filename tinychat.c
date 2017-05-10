/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2014-2017  alicia@ion.nu
    Copyright (C) 2014-2015  Jade Lea

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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include "client.h"
#include "idlist.h"
#include "rtmp.h"
#include "colors.h"
#include "amfwriter.h"
#include "numlist.h"
#include "media.h"
#define sitearg (greenroom?"greenroom":"tinychat")
static const char* bpassword=0;
static char* unban=0;

static char* gethost(const char *channel, const char *password)
{
  int urllen;
  if(password)
  {
    urllen=strlen("http://tinychat.com/api/find.room/?site=&password=0")+strlen(channel)+strlen(sitearg)+strlen(password);
  }else{
    urllen=strlen("http://tinychat.com/api/find.room/?site=0")+strlen(channel)+strlen(sitearg);
  }
  char url[urllen];
  if(password)
  {
    sprintf(url, "http://tinychat.com/api/find.room/%s?site=%s&password=%s", channel, sitearg, password);
  }else{
    sprintf(url, "http://tinychat.com/api/find.room/%s?site=%s", channel, sitearg);
  }
  char* response=http_get(url, 0);
  if(!response){exit(-1);}
  //response contains result='(OK|RES)|PW' (latter means a password is required)
  char* result=strstr(response, "result='");
  if(!result){printf("No result\n"); exit(-1);}
  char* bpass=strstr(response, " bpassword='");
  result+=strlen("result='");
  // Handle the result value
  if(!strncmp(result, "PW", 2)){printf("Password required\n"); exit(-1);}
  if(strncmp(result, "OK", 2) && strncmp(result, "RES", 3)){printf("Result not OK\n"); exit(-1);}
  // Find and extract the server responsible for this channel
  char* rtmp=strstr(response, "rtmp='rtmp://");
  if(!rtmp){printf("No rtmp found.\n"); exit(-1);}
  rtmp+=strlen("rtmp='rtmp://");
  int len;
  for(len=0; rtmp[len] && rtmp[len]!='/'; ++len);
  char* host=strndup(rtmp, len);
  // Check if this is a greenroom channel
  if(strstr(response, "greenroom=\"1\""))
  {
    printf("Channel has greenroom\n");
  }
  char* end;
  if(bpass && (end=strchr(&bpass[12], '\'')))
  {
    end[0]=0;
    bpassword=strdup(&bpass[12]);
    printf("Authenticated to broadcast\n");
  }
  free(response);
  return host;
}

static char* getkey(int id, const char* channel)
{
  char url[snprintf(0,0, "http://tinychat.com/api/captcha/check.php?guest%%5Fid=%i&room=%s%%5E%s", id, sitearg, channel)+1];
  sprintf(url, "http://tinychat.com/api/captcha/check.php?guest%%5Fid=%i&room=%s%%5E%s", id, sitearg, channel);
  char* response=http_get(url, 0);
  char* key=strstr(response, "\"key\":\"");

  if(!key){return 0;}
  key+=7;
  char* keyend=strchr(key, '"');
  if(!keyend){return 0;}
  char* backslash;
  while((backslash=strchr(key, '\\')))
  {
    memmove(backslash, &backslash[1], strlen(backslash));
    --keyend;
  }
  key=strndup(key, keyend-key);
  free(response);
  return key;
}

static void getcaptcha(const char* channel)
{
  char* url="http://tinychat.com/cauth/captcha";
  char* page=http_get(url, 0);
  char* token=strstr(page, "\"token\":\"");
  if(token)
  {
    token=&token[9];
    char* end=strchr(token, '"');
    if(end)
    {
      end[0]=0;
      printf("Captcha: http://tinychat.com/%s + javascript:void(ShowRecaptcha('%s'));\n", channel, token);
      fflush(stdout);
      fgetc(stdin);
    }
  }
  free(page);
}

static char* getcookie(const char* channel)
{
  unsigned long long now=time(0);
  char url[strlen("http://tinychat.com/cauth?t=&room=0")+snprintf(0,0, "%llu", now)+strlen(channel)];
  sprintf(url, "http://tinychat.com/cauth?t=%llu&room=%s", now, channel);
  char* response=http_get(url, 0);
  if(strstr(response, "\"lurker\":1"))
  {
    printf("Captcha not completed, you will not be able to send any messages or broadcast\n");
  }
  char* cookie=strstr(response, "\"cookie\":\"");

  if(!cookie){return 0;}
  cookie+=10;
  char* end=strchr(cookie, '"');
  if(!end){return 0;}
  cookie=strndup(cookie, end-cookie);
  free(response);
  return cookie;
}

static char* getbroadcastkey(const char* channel, const char* nick, const char* bpassword)
{
  unsigned int id=idlist_get(nick);
  char url[snprintf(0,0, "http://tinychat.com/api/broadcast.pw?name=%s&site=%s&nick=%s&id=%u%s%s", channel, sitearg, nick, id, bpassword?"&password=":"", bpassword?bpassword:"")+1];
  sprintf(url, "http://tinychat.com/api/broadcast.pw?name=%s&site=%s&nick=%s&id=%u%s%s", channel, sitearg, nick, id, bpassword?"&password=":"", bpassword?bpassword:"");
  char* response=http_get(url, 0);
  if(strstr(response, " result='PW'")){free(response); return 0;}
  char* key=strstr(response, " token='");

  if(!key){free(response); return 0;}
  key+=8;
  char* keyend=strchr(key, '\'');
  if(!keyend){free(response); return 0;}
  key=strndup(key, keyend-key);
  free(response);
  return key;
}

static char* getmodkey(const char* user, const char* pass, const char* channel, char* loggedin)
{
  // TODO: if possible, do this in a neater way than digging the key out from an HTML page.
  if(!user||!pass){return 0;}
  // Get token
  char* response=http_get("https://tinychat.com/start/", 0);
  char* token=strstr(response, "\" name=\"_token\"");
  token[0]=0;
  while(token>response && strncmp(token, "value=\"", 7)){--token;}
  token=&token[7];
  // Log in
  char post[strlen("login_username=&login_password=&_token=&next=https://tinychat.com/0")+strlen(user)+strlen(pass)+strlen(token)+strlen(channel)];
  sprintf(post, "login_username=%s&login_password=%s&_token=%s&next=https://tinychat.com/%s", user, pass, token, channel);
  response=http_get("https://tinychat.com/login", post);
  char* key=strstr(response, "autoop: \"");
  if(key)
  {
    key=&key[9];
    char* end=strchr(key, '"');
    if(end)
    {
      end[0]=0;
      key=strdup(key);
    }else{key=0;}
  }
  if(strstr(response, "<div class='name'")){*loggedin=1;}
  free(response);
  return key;
}

static char checknick(const char* nick) // Returns zero if the nick is valid, otherwise returning the character that failed the check
{
  unsigned int i;
  for(i=0; nick[i]; ++i)
  {
    if(!strchr("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-^[]{}`\\|", nick[i])){return nick[i];}
  }
  return 0;
}

static void sendmessage_priv(int sock, const char* buf, const char* priv)
{
  int id=priv?idlist_get(priv):0;
  char privfield[snprintf(0, 0, "n%i-%s", id, priv?priv:"")+1];
  sprintf(privfield, "n%i-%s", id, priv?priv:"");

  struct rtmp amf;
  char* msg=tonumlist((char*)buf, strlen(buf));
  amfinit(&amf, 3);
  amfstring(&amf, "privmsg");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfstring(&amf, msg);
  amfstring(&amf, colors[currentcolor%COLORCOUNT]);
  // For PMs, add a string like "n<numeric ID>-<nick>" to make the server only send it to the intended recipient
  if(priv)
  {
    amfstring(&amf, privfield);
    // And one in case they're broadcasting
    privfield[0]='b'; // 'b' for broadcasting
    struct rtmp bamf;
    amfinit(&bamf, 3);
    amfstring(&bamf, "privmsg");
    amfnum(&bamf, 0);
    amfnull(&bamf);
    amfstring(&bamf, msg);
    amfstring(&bamf, colors[currentcolor%COLORCOUNT]);
    amfstring(&bamf, privfield);
    amfsend(&bamf, sock);
  }
  amfsend(&amf, sock);
  free(msg);
}

static void sendmessage(int sock, const char* buf)
{
  if(!strncmp(buf, "/priv ", 6))
  {
    const char* msg=strchr(&buf[6], ' ');
    if(msg)
    {
      char priv[msg-&buf[6]+1];
      memcpy(priv, &buf[6], msg-&buf[6]);
      priv[msg-&buf[6]]=0;
      sendmessage_priv(sock, &msg[1], priv);
      return;
    }
  }
  sendmessage_priv(sock, buf, 0);
}

static void sendpm(int sock, const char* buf, const char* recipient)
{
  char msg[strlen("/msg  0")+strlen(recipient)+strlen(buf)];
  sprintf(msg, "/msg %s %s", recipient, buf);
  sendmessage_priv(sock, msg, recipient);
}

static void changenick(int sock, const char* newnick)
{
  char badchar;
  if((badchar=checknick(newnick)))
  {
    printf("'%c' is not allowed in nicknames.\n", badchar);
    return;
  }
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "nick");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfstring(&amf, newnick);
  amfsend(&amf, sock);
}

static void joinreg(struct amf* amfin, int sock)
{
  if(amf_comparestrings_c(&amfin->items[0].string, "joins"))
  {
    printf("Currently online: ");
  }else{
    printf("%s ", timestamp());
  }
  int i;
  for(i = 2; i < amfin->itemcount; ++i)
  {
    if(amfin->items[i].type==AMF_OBJECT)
    {
      struct amfitem* item=amf_getobjmember(&amfin->items[i].object, "id");
      if(item->type!=AMF_NUMBER){continue;}
      int id=item->number;
      item=amf_getobjmember(&amfin->items[i].object, "nick");
      if(item->type!=AMF_STRING){continue;}
      const char* nick=item->string.string;
      item=amf_getobjmember(&amfin->items[i].object, "account");
      if(item->type!=AMF_STRING){continue;}
      const char* account=(item->string.string[0]?item->string.string:0);
      item=amf_getobjmember(&amfin->items[i].object, "mod");
      if(item->type!=AMF_BOOL){continue;}
      char mod=item->boolean;
      idlist_add(id, nick, account, mod);
      printf("%s%s", (i==2?"":", "), nick);
      if(amf_comparestrings_c(&amfin->items[0].string, "registered"))
      {
        // Tell the server how often to let us know it got our packets
        struct rtmp setbw={
          .type=RTMP_SERVER_BW,
          .chunkid=2,
          .length=4,
          .msgid=0,
          .buf="\x00\x00\x40\x00" // Every 0x4000 bytes
        };
        rtmp_send(sock, &setbw);
        char* key=getkey(id, channel);
        if(!key){printf("Failed to get channel key\n"); exit(1);}
        struct rtmp amf;
        amfinit(&amf, 3);
        amfstring(&amf, "cauth");
        amfnum(&amf, 0);
        amfnull(&amf); // Means nothing but is apparently critically important for cauth at least
        amfstring(&amf, key);
        amfsend(&amf, sock);
        free(key);
        // Set the given nickname
        changenick(sock, nickname);
        // Keep what the server gave us, just in case
        free(nickname);
        nickname=strdup(nick);
      }
    }
  }
  if(amf_comparestrings_c(&amfin->items[0].string, "joins"))
  {
    printf("\n");
    for(i=0; i<idlistlen; ++i)
    {
      if(idlist[i].op){printf("%s is a moderator.\n", idlist[i].name);}
    }
    printf("Currently on cam: ");
    char first=1;
    for(i = 2; i < amfin->itemcount; ++i)
    {
      if(amfin->items[i].type==AMF_OBJECT)
      {
        struct amfitem* item=amf_getobjmember(&amfin->items[i].object, "bf");
        if(item->type!=AMF_BOOL || !item->boolean){continue;}
        item=amf_getobjmember(&amfin->items[i].object, "nick");
        if(item->type!=AMF_STRING){continue;}
        printf("%s%s", (first?"":", "), item->string.string);
        first=0;
      }
    }
    printf("\n");
  }else{
    printf(" entered the channel\n");
    if(idlist[idlistlen-1].op){printf("%s is a moderator.\n", idlist[idlistlen-1].name);}
    if(amf_comparestrings_c(&amfin->items[0].string, "registered"))
    {
      printf("Connection ID: %i\n", idlist[0].id);
    }
  }
}

static void privmsg(struct amf* amfin, int sock)
{
  if(amfin->items[3].type!=AMF_STRING || amfin->items[4].type!=AMF_STRING || amfin->items[5].type!=AMF_STRING){return;}
  size_t len;
  char* msg=fromnumlist(amfin->items[3].string.string, &len);
  const char* color=color_start(amfin->items[4].string.string);
  char* line=msg;
  while(line)
  {
    // Handle multi-line messages
    char* nextline=0;
    unsigned int linelen;
    for(linelen=0; &line[linelen]<&msg[len]; ++linelen)
    {
      if(line[linelen]=='\r' || line[linelen]=='\n'){nextline=&line[linelen+1]; break;}
    }
    printf("%s %s%s: ", timestamp(), color, amfin->items[5].string.string);
    fwrite(line, linelen, 1, stdout);
    printf("%s\n", color_end());
    line=nextline;
  }
  if(!strncmp(msg, "/allowbroadcast ", 16))
  {
    if(getbroadcastkey(channel, nickname, &msg[16])) // Validate password
    {
      free((void*)bpassword);
      bpassword=strdup(&msg[16]);
      printf("Authenticated to broadcast\n");
    }
  }
  else if(!strcmp(msg, "/version"))
  {
    sendmessage_priv(sock, "/version tc_client-" VERSION, amfin->items[5].string.string);
  }
  free(msg);
}

static void userquit(struct amf* amfin, int sock)
{
  // quit/part
  if(amfin->items[2].type!=AMF_STRING){return;}
  idlist_remove(amfin->items[2].string.string);
  printf("%s %s left the channel\n", timestamp(), amfin->items[2].string.string);
}

static void usernick(struct amf* amfin, int sock)
{
  // nick
  if(amfin->items[2].type!=AMF_STRING || amfin->items[3].type!=AMF_STRING){return;}
  if(!strcmp(amfin->items[2].string.string, nickname)) // Successfully changed our own nickname
  {
    free(nickname);
    nickname=strdup(amfin->items[3].string.string);
  }
  idlist_rename(amfin->items[2].string.string, amfin->items[3].string.string);
  printf("%s %s changed nickname to %s\n", timestamp(), amfin->items[2].string.string, amfin->items[3].string.string);
}

static void userkick(struct amf* amfin, int sock)
{
  // kick
  if(amfin->items[2].type!=AMF_STRING){return;}
  if(atoi(amfin->items[2].string.string) == idlist_get(nickname))
  {
    printf("%s You have been kicked out\n", timestamp());
  }
}

static void banned(struct amf* amfin, int sock)
{
  // banned
  printf("%s You are banned from %s\n", timestamp(), channel);
  exit(1); // Getting banned is a failure, right?
}

static void closecam(int sock, const char* nick)
{
  unsigned int userid=idlist_get(nick);
  char camid[snprintf(0,0,"%u", userid)+1];
  sprintf(camid, "%u", userid);
  stream_stopvideo(sock, camid);
}

static void fromowner(struct amf* amfin, int sock)
{
  // from_owner: notices, mute, push2talk, closing cams
  if(amfin->items[2].type!=AMF_STRING){return;}
  if(!strncmp("notice", amfin->items[2].string.string, 6))
  {
    char* notice=strdup(&amfin->items[2].string.string[6]);
    // replace "%20" with spaces
    char* space;
    while((space=strstr(notice, "%20")))
    {
      memmove(space, &space[2], strlen(&space[2])+1);
      space[0]=' ';
    }
    printf("%s %s\n", timestamp(), notice);
  }
  else if(!strcmp("mute", amfin->items[2].string.string))
  {
    printf("%s Non-moderators have been temporarily muted.\n", timestamp());
  }
  else if(!strcmp("push2talk", amfin->items[2].string.string))
  {
    printf("%s Push to talk request has been sent to non-moderators.\n", timestamp());
  }
  else if(!strncmp("_close", amfin->items[2].string.string, 6) && !strcmp(&amfin->items[2].string.string[6], nickname))
  {
    printf("Outgoing media stream was closed\n");
    closecam(sock, nickname);
  }
}

static void nickinuse(struct amf* amfin, int sock)
{
  // nickinuse, the nick we wanted to change to is already taken
  printf("Nick is already in use.\n");
}

static void channeltopic(struct amf* amfin, int sock)
{
  // Room topic
  if(amfin->items[2].type!=AMF_STRING || !amfin->items[2].string.length){return;}
  printf("Room topic: %s\n", amfin->items[2].string.string);
}

static void banlist(struct amf* amfin, int sock)
{
  // Get list of banned users
  unsigned int i;
  if(unban) // This is not a response to /banlist but to /forgive
  {
    // If the nickname isn't found in the banlist we assume it's an ID
    const char* id=unban;
    for(i=2; i+1<amfin->itemcount; i+=2)
    {
      if(amfin->items[i].type!=AMF_STRING || amfin->items[i+1].type!=AMF_STRING){break;}
      if(!strcmp(amfin->items[i+1].string.string, unban) || !strcmp(amfin->items[i].string.string, unban))
      {
        id=amfin->items[i].string.string;
      }
    }
    struct rtmp amf;
    amfinit(&amf, 3);
    amfstring(&amf, "forgive");
    amfnum(&amf, 0);
    amfnull(&amf);
    amfstring(&amf, id);
    amfsend(&amf, sock);
    free(unban);
    unban=0;
    return;
  }
  printf("Banned users:\n");
  printf("ID         Nickname\n");
  for(i=2; i+1<amfin->itemcount; i+=2)
  {
    if(amfin->items[i].type!=AMF_STRING || amfin->items[i+1].type!=AMF_STRING){break;}
    unsigned int len=printf("%s", amfin->items[i].string.string);
    for(;len<10; ++len){printf(" ");}
    printf(" %s\n", amfin->items[i+1].string.string);
  }
  printf("Use /forgive <ID/nick> to unban someone\n");
}

static void notice(struct amf* amfin, int sock)
{
  if(amfin->items[2].type!=AMF_STRING || !amf_comparestrings_c(&amfin->items[2].string, "avon") || amfin->items[4].type!=AMF_STRING){return;}
  printf("%s cammed up\n", amfin->items[4].string.string);
}

static void result(struct amf* amfin, int sock)
{
  // Handle results for various requests, haven't seen much of a pattern to it, always successful?
  // Creating a new stream worked, now play media (cam/mic) on it (if that's what the result was for)
  stream_play(amfin, sock);
}

static void onstatus(struct amf* amfin, int sock)
{
  stream_handlestatus(amfin, sock);
}

static void mod_close(int sock, const char* nick)
{
  struct rtmp amf;
  amfinit(&amf, 2);
  amfstring(&amf, "owner_run");
  amfnum(&amf, 0);
  amfnull(&amf);
  char buf[strlen("closed: 0")+strlen(nick)];
  sprintf(buf, "_close%s", nick);
  amfstring(&amf, buf);
  amfsend(&amf, sock);
  sprintf(buf, "closed: %s", nick);
  sendmessage(sock, buf);
}

static void mod_ban(int sock, const char* nick)
{
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "owner_run");
  amfnum(&amf, 0);
  amfnull(&amf);
//  char buf[strlen("notice%20was%20banned%20by%20%20()0")+strlen(nick)+strlen(nickname)+strlen(account_user)];
//  sprintf(buf, "notice%s%%20was%%20banned%%20by%%20%s%%20(%s)", nick, nickname, account_user);
  char buf[strlen("notice%20was%20banned%20by%200")+strlen(nick)+strlen(nickname)];
  sprintf(buf, "notice%s%%20was%%20banned%%20by%%20%s", nick, nickname);
  amfstring(&amf, buf);
  amfsend(&amf, sock);
//  printf("%s %s was banned by %s (%s)\n", timestamp(), nick, nickname, account_user);
  printf("%s %s was banned by %s\n", timestamp(), nick, nickname);
  // kick (this does the actual banning)
  amfinit(&amf, 3);
  amfstring(&amf, "kick");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfstring(&amf, nick);
  sprintf(buf, "%i", idlist_get(nick));
  amfstring(&amf, buf);
  amfsend(&amf, sock);
}

static void mod_banlist(int sock)
{
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "banlist");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfsend(&amf, sock);
}

static void mod_unban(int sock, const char* nick)
{
  free(unban);
  unban=strdup(nick);
  mod_banlist(sock);
}

static void mod_mute(int sock)
{
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "owner_run");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfstring(&amf, "mute");
  amfsend(&amf, sock);
}

static void mod_push2talk(int sock)
{
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "owner_run");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfstring(&amf, "push2talk");
  amfsend(&amf, sock);
}

static void mod_topic(int sock, const char* newtopic)
{
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "topic");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfstring(&amf, newtopic);
  amfstring(&amf, "");
  amfsend(&amf, sock);
}

static void mod_allowbroadcast(int sock, const char* nick)
{
  if(!bpassword){return;}
  char buf[strlen("/allowbroadcast 0")+strlen(bpassword)];
  sprintf(buf, "/allowbroadcast %s", bpassword);
  sendmessage_priv(sock, buf, nick);
}

static void camup(int sock)
{
  // Retrieve and send the key for broadcasting access
  char* key=getbroadcastkey(channel, nickname, bpassword);
  if(!key){printf("Failed to get the broadcast key\n"); return;}
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "bauth");
  amfnum(&amf, 0);
  amfnull(&amf);
  amfstring(&amf, key);
  amfsend(&amf, sock);
  free(key);
  // Initiate stream
  unsigned int userid=idlist_get(nickname);
  char camid[snprintf(0,0,"%u", userid)+1];
  sprintf(camid, "%u", userid);
  streamout_start(camid, sock);
}

static void camdown(int sock)
{
  unsigned int userid=idlist_get(nickname);
  char camid[snprintf(0,0,"%u", userid)+1];
  sprintf(camid, "%u", userid);
  stream_stopvideo(sock, camid);
}

static void opencam(int sock, const char* nick)
{
  unsigned int userid=idlist_get(nick);
  char camid[snprintf(0,0,"%u", userid)+1];
  sprintf(camid, "%u", userid);
  stream_start(nick, camid, sock);
}

int init_tinychat(const char* chanpass, const char* username, const char* userpass, struct site* site)
{
  char badchar;
  if((badchar=checknick(nickname)))
  {
    printf("'%c' is not allowed in nicknames.\n", badchar);
    return -1;
  }
  registercmd("privmsg", privmsg, 6);
  registercmd("join", joinreg, 3);
  registercmd("nick", usernick, 5);
  registercmd("quit", userquit, 3);
  registercmd("notice", notice, 5);
  registercmd("from_owner", fromowner, 3);
  registercmd("nickinuse", nickinuse, 1);
  registercmd("banlist", banlist, 1);
  registercmd("topic", channeltopic, 3);
  registercmd("joins", joinreg, 3);
  registercmd("registered", joinreg, 3);
  registercmd("onStatus", onstatus, 1);
  registercmd("_result", result, 3);
  registercmd("kick", userkick, 4);
  registercmd("banned", banned, 2);
  site->sendmessage=sendmessage;
  site->sendpm=sendpm;
  site->nick=changenick;
  site->mod_close=mod_close;
  site->mod_ban=mod_ban;
  site->mod_banlist=mod_banlist;
  site->mod_unban=mod_unban;
  site->mod_mute=mod_mute;
  site->mod_push2talk=mod_push2talk;
  site->mod_topic=mod_topic;
  site->mod_allowbroadcast=mod_allowbroadcast;
  site->opencam=opencam;
  site->closecam=closecam;
  site->camup=camup;
  site->camdown=camdown;
  char loggedin=0;
  // Log in if user account is specified
  char* modkey=getmodkey(username, userpass, channel, &loggedin);
  char* server=gethost(channel, chanpass);
  // Separate IP/domain and port
  char* port=strchr(server, ':');
  if(!port){return 3;}
  port[0]=0;
  ++port;
  int sock=connectto(server, port);
  if(sock==-1){return -1;}

  rtmp_handshake(sock);
  if(!loggedin){username=0; userpass=0;}
  getcaptcha(channel);
  char* cookie=getcookie(channel);
  // Send connect request
  struct rtmp amf;
  amfinit(&amf, 3);
  amfstring(&amf, "connect");
  amfnum(&amf, 0);
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
    {char str[strlen("rtmp://:/tinyconf0")+strlen(server)+strlen(port)+strlen(channel)];
    sprintf(str, "rtmp://%s:%s/chat/%s", server, port, channel);
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
    char pageurl[strlen("http://tinychat.com/rooms/0") + strlen(channel)];
    sprintf(pageurl, "http://tinychat.com/rooms/%s", channel);
    amfstring(&amf, pageurl);

    amfobjitem(&amf, "objectEncoding");
    amfnum(&amf, 0);
  amfobjend(&amf);
  amfobjstart(&amf);
    amfobjitem(&amf, "version");
    amfstring(&amf, "Desktop");

    amfobjitem(&amf, "type");
    amfstring(&amf, "default"); // This item is called roomtype in the same HTTP response that gives us the server (IP+port) to connect to, but "default" seems to work fine too.

    amfobjitem(&amf, "account");
    amfstring(&amf, username?username:"");

    amfobjitem(&amf, "prefix");
    amfstring(&amf, sitearg);

    amfobjitem(&amf, "room");
    amfstring(&amf, channel);

    if(modkey)
    {
      amfobjitem(&amf, "autoop");
      amfstring(&amf, modkey);
    }
    amfobjitem(&amf, "cookie");
    amfstring(&amf, cookie);
  amfobjend(&amf);
  amfsend(&amf, sock);
  free(modkey);
  free(cookie);
  return sock;
}
