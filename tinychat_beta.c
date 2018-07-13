/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2017-2018  alicia@ion.nu

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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <json.h>
#include "client.h"
#include "idlist.h"

static const char* user_join(struct json_object* user)
{
  struct json_object* value;
  if(!json_object_object_get_ex(user, "nick", &value)){return 0;}
  const char* nick=json_object_get_string(value);
  if(!json_object_object_get_ex(user, "username", &value)){return 0;}
  const char* account=json_object_get_string(value);
  if(!json_object_object_get_ex(user, "handle", &value)){return 0;}
  int64_t id=json_object_get_int64(value);
  if(!json_object_object_get_ex(user, "mod", &value)){return 0;}
  char mod=json_object_get_boolean(value);
  idlist_add(id, nick, account, mod);
  return nick;
}

static const char* chanpass=0;
static void handlepacket(websock_conn* conn, void* data, struct websock_head* head)
{
  // Parse JSON and handle data
  struct json_tokener* tok=json_tokener_new();
  struct json_object* obj=json_tokener_parse_ex(tok, data, head->length);
  json_tokener_free(tok);
  if(!obj){return;}
  struct json_object* cmdobj;
  if(!json_object_object_get_ex(obj, "tc", &cmdobj)){json_object_put(obj); return;}
  const char* cmd=json_object_get_string(cmdobj);
  if(!strcmp(cmd, "ping")){websock_write(conn, "{\"tc\":\"pong\"}", 13, WEBSOCK_TEXT);}
  else if(!strcmp(cmd, "password"))
  {
    if(!chanpass){printf("Password required\n"); exit(-1);}
    char buf[strlen("{\"tc\":\"password\",\"password\":\"\"}0")+strlen(chanpass)];
    sprintf(buf, "{\"tc\":\"password\",\"password\":\"%s\"}", chanpass);
    websock_write(conn, buf, strlen(buf), WEBSOCK_TEXT);
  }
  else if(!strcmp(cmd, "userlist"))
  {
    struct json_object* users;
    if(!json_object_object_get_ex(obj, "users", &users)){json_object_put(obj); return;}
    if(!json_object_is_type(users, json_type_array)){json_object_put(obj); return;}
    printf("Currently online: ");
    unsigned int i;
    for(i=0; i<json_object_array_length(users); ++i)
    {
      struct json_object* user=json_object_array_get_idx(users, i);
      if(!user){continue;}
      const char* nick=user_join(user);
      if(!nick){continue;}
      printf(i?", %s":"%s", nick);
    }
    printf("\n");
    // List everyone who is a mod
    for(i=0; i<idlistlen; ++i)
    {
      if(idlist[i].op){printf("%s is a moderator.\n", idlist[i].name);}
    }
  }
  else if(!strcmp(cmd, "msg"))
  {
    if(!json_object_object_get_ex(obj, "text", &cmdobj)){json_object_put(obj); return;}
    const char* msg=json_object_get_string(cmdobj);
    if(!json_object_object_get_ex(obj, "handle", &cmdobj)){json_object_put(obj); return;}
    int64_t handle=json_object_get_int64(cmdobj);
    const char* nick=idlist_getnick(handle);
    if(!strcmp(nick, nickname)){json_object_put(obj); return;} // Ignore messages from ourselves (server sends it back along with sending it to everyone else)
    printf("%s %s: %s\n", timestamp(), nick, msg);
  }
  else if(!strcmp(cmd, "joined"))
  {
    struct json_object* channel;
    if(!json_object_object_get_ex(obj, "room", &channel)){json_object_put(obj); return;}
    if(!json_object_object_get_ex(channel, "topic", &cmdobj)){json_object_put(obj); return;}
    const char* topic=json_object_get_string(cmdobj);
    printf("Room topic: %s\n", topic);
    if(!json_object_object_get_ex(obj, "self", &cmdobj)){json_object_put(obj); return;}
    const char* nick=user_join(cmdobj);
    printf("Connection ID: %i\n", idlist[0].id);
    if(nick)
    {
      free(nickname);
      nickname=strdup(nick);
    }
  }
  else if(!strcmp(cmd, "nick"))
  {
    if(!json_object_object_get_ex(obj, "success", &cmdobj)){json_object_put(obj); return;}
    if(!json_object_get_boolean(cmdobj)){json_object_put(obj); return;} // Ignore failed nick changes
    if(!json_object_object_get_ex(obj, "nick", &cmdobj)){json_object_put(obj); return;}
    const char* nick=json_object_get_string(cmdobj);
    if(!json_object_object_get_ex(obj, "handle", &cmdobj)){json_object_put(obj); return;}
    int64_t handle=json_object_get_int64(cmdobj);
    const char* oldnick=idlist_getnick(handle);
    printf("%s %s changed nickname to %s\n", timestamp(), oldnick, nick);
    if(!strcmp(oldnick, nickname)){free(nickname); nickname=strdup(nick);}
    idlist_renameid(handle, nick);
  }
  else if(!strcmp(cmd, "join"))
  {
    const char* nick=user_join(obj);
    if(!nick){json_object_put(obj); return;}
    printf("%s %s entered the channel\n", timestamp(), nick);
    if(!json_object_object_get_ex(obj, "mod", &cmdobj)){json_object_put(obj); return;}
    if(json_object_get_boolean(cmdobj))
    {
      printf("%s is a moderator.\n", nick);
    }
  }
  else if(!strcmp(cmd, "quit"))
  {
    if(!json_object_object_get_ex(obj, "handle", &cmdobj)){json_object_put(obj); return;}
    int64_t handle=json_object_get_int64(cmdobj);
    const char* nick=idlist_getnick(handle);
    printf("%s %s left the channel\n", timestamp(), nick);
    idlist_removeid(handle);
  }
  else if(!strcmp(cmd, "sysmsg"))
  {
    if(!json_object_object_get_ex(obj, "text", &cmdobj)){json_object_put(obj); return;}
    const char* text=json_object_get_string(cmdobj);
    printf("%s %s\n", timestamp(), text);
  }
  else if(!strcmp(cmd, "yut_play"))
  {
    if(!json_object_object_get_ex(obj, "success", &cmdobj)){json_object_put(obj); return;}
    if(!json_object_get_boolean(cmdobj)){json_object_put(obj); return;} // Ignore failed media actions
    struct json_object* item;
    if(!json_object_object_get_ex(obj, "item", &item)){json_object_put(obj); return;}
    if(!json_object_object_get_ex(item, "id", &cmdobj)){json_object_put(obj); return;}
    const char* id=json_object_get_string(cmdobj);
    if(!json_object_object_get_ex(item, "offset", &cmdobj)){json_object_put(obj); return;}
    int64_t offset=json_object_get_int64(cmdobj)*1000; // Milliseconds
    printf("Media start: youtube(%s) %llu\n", id, offset);
  }else{
    write(1, "Received: ", 10);
    write(1, data, head->length);
    write(1, "\n", 1);
    printf("Command: %s\n", cmd);
  }
  json_object_put(obj);
}

static void sendmessage(conn c, const char* msg)
{
  struct json_object* obj=json_object_new_object();
  json_object_object_add(obj, "tc", json_object_new_string("msg"));
  json_object_object_add(obj, "text", json_object_new_string(msg));
  const char* json=json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
  websock_write(c.ws, json, strlen(json), WEBSOCK_TEXT);
  json_object_put(obj);
}

static void setnick(conn c, const char* nick)
{
  struct json_object* obj=json_object_new_object();
  json_object_object_add(obj, "tc", json_object_new_string("nick"));
  json_object_object_add(obj, "nick", json_object_new_string(nick));
  const char* json=json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
  websock_write(c.ws, json, strlen(json), WEBSOCK_TEXT);
  json_object_put(obj);
}

static void notimplemented()
{
  printf("The requested feature is either not supported by tinychat beta or not yet implemented in tc_client's tinychat beta support\n");
}

int init_tinychat_beta(const char* chanpass_, const char* username, const char* userpass, struct site* site)
{
  site->handlewspacket=handlepacket;
  site->sendmessage=sendmessage;
  site->sendpm=notimplemented;
  site->nick=setnick;
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
  chanpass=chanpass_;
  // Request token and endpoint
  char url[strlen("https://tinychat.com/api/v1.0/room/token/0")+strlen(channel)];
  sprintf(url, "https://tinychat.com/api/v1.0/room/token/%s", channel);
  char* tokenfile=http_get(url, 0);
  struct json_tokener* tok=json_tokener_new();
  struct json_object* obj=json_tokener_parse_ex(tok, tokenfile, strlen(tokenfile));
  json_tokener_free(tok);
  if(!obj){return -1;}
  struct json_object* value;
  if(!json_object_object_get_ex(obj, "result", &value)){return -1;}
  const char* token=json_object_get_string(value);
  if(!json_object_object_get_ex(obj, "endpoint", &value)){return -1;}
  const char* endpointstr=json_object_get_string(value);
  // Decompose endpoint into host and port
  endpointstr=strstr(endpointstr, "://");
  if(!endpointstr){return -1;}
  endpointstr=&endpointstr[3];
  char endpoint[strlen(endpointstr)+1];
  strcpy(endpoint, endpointstr);
  char* port=strchr(endpoint, ':');
  if(!port){return -1;}
  port[0]=0;
  port=&port[1];
  // Connect
  int sock=connectto(endpoint, port);
  site->websocket=websock_new(sock, 1, 0, 0);
  if(!websock_handshake_client(site->websocket, "/", endpoint, "tc", "https://tinychat.com", 0)){printf("Websocket handshake failed\n"); return -1;}
  const char* fmt="{\"tc\":\"join\",\"useragent\":\"tc_client\",\"token\":\"%s\",\"room\":\"%s\",\"nick\":\"%s\"}";
  char buf[snprintf(0,0, fmt, token, channel, nickname)+1];
  sprintf(buf, fmt, token, channel, nickname);
  free(tokenfile);
  websock_write(site->websocket, buf, strlen(buf), WEBSOCK_TEXT);
  return sock;
}
