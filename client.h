#ifdef HAVE_WEBSOCKET
#include <libwebsocket/websock.h>
#endif
#include "amfparser.h"
typedef union
{
  int fd;
#ifdef HAVE_WEBSOCKET
  websock_conn* ws;
#endif
} conn;
struct site
{
  void(*sendmessage)(conn c, const char* buf);
  void(*sendpm)(conn c, const char* buf, const char* recipient);
  void(*nick)(conn c, const char* newnick);
  void(*mod_close)(conn c, const char* nick);
  void(*mod_ban)(conn c, const char* nick);
  void(*mod_banlist)(conn c);
  void(*mod_unban)(conn c, const char* nick);
  void(*mod_mute)(conn c);
  void(*mod_push2talk)(conn c);
  void(*mod_topic)(conn c, const char* newtopic);
  void(*mod_allowbroadcast)(conn c, const char* nick);
  void(*opencam)(conn c, const char* arg);
  void(*closecam)(conn c, const char* arg);
  void(*camup)(conn c);
  void(*camdown)(conn c);
#ifdef HAVE_WEBSOCKET
  websock_conn* websocket;
  void(*handlewspacket)(websock_conn*, void*, struct websock_head*);
#endif
};

extern char greenroom;
extern char* channel;
extern char* nickname;
extern char* http_get(const char* url, const char* post);
extern char* timestamp();
extern int connectto(const char* host, const char* port);
extern void registercmd(const char* command, void(*callback)(struct amf*,int), unsigned int minargs);
