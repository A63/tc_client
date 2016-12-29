#include "amfparser.h"
struct site
{
  void(*sendmessage)(int sock, const char* buf);
  void(*sendpm)(int sock, const char* buf, const char* recipient);
  void(*nick)(int sock, const char* newnick);
  void(*mod_close)(int sock, const char* nick);
  void(*mod_ban)(int sock, const char* nick);
  void(*mod_banlist)(int sock);
  void(*mod_unban)(int sock, const char* nick);
  void(*mod_mute)(int sock);
  void(*mod_push2talk)(int sock);
  void(*mod_topic)(int sock, const char* newtopic);
  void(*mod_allowbroadcast)(int sock, const char* nick);
  void(*opencam)(int sock, const char* arg);
  void(*closecam)(int sock, const char* arg);
  void(*camup)(int sock);
  void(*camdown)(int sock);
};

extern char greenroom;
extern char* channel;
extern char* nickname;
extern char* http_get(const char* url, const char* post);
extern char* timestamp();
extern int connectto(const char* host, const char* port);
extern void registercmd(const char* command, void(*callback)(struct amf*,int), unsigned int minargs);
