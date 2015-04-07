/*
    irchack, a simple application to reuse IRC clients as user interfaces for tc_client
    Copyright (C) 2014  alicia@ion.nu

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
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>

int main(int argc, char** argv)
{
  int port=(argc>1?atoi(argv[1]):6667);
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family=AF_INET;
  addr.sin_addr.s_addr=0;
  addr.sin_port=htons(port);
  int lsock=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(bind(lsock, (struct sockaddr*)&addr, sizeof(addr))){perror("bind"); return 1;}
  listen(lsock, 1);
  printf("Done! Open an IRC client and connect to localhost on port %i\n", port);
  int sock=accept(lsock, 0, 0);
  close(lsock);
  char buf[2048];
  char* nick=0;
  char* channel=0;
  char* pass=0;
  int len;
  while(!nick || !channel) // Init loop, wait for USER, NICK and JOIN, well not USER
  {
    len=0;
    while(1)
    {
      if(read(sock, &buf[len], 1)!=1 || buf[len]=='\r' || buf[len]=='\n'){break;}
      ++len;
    }
    buf[len]=0;
    if(!strncmp(buf, "NICK ", 5))
    {
      char* newnick=&buf[5];
      if(newnick[0]==':'){newnick=&nick[1];}
      if(!nick)
      {
        dprintf(sock, ":irchack 001 %s :Welcome\n", newnick);
      }else{
        dprintf(sock, ":%s!user@host NICK :%s\n", nick, newnick);
      }
      free(nick);
      nick=strdup(newnick);
    }
    else if(!strncmp(buf, "JOIN ", 5))
    {
      free(channel);
      channel=&buf[5];
      if(channel[0]==':'){channel=&channel[1];}
      free(pass);
      pass=strchr(channel, ' ');
      if(pass)
      {
        pass[0]=0;
        pass=&pass[1];
        if(pass[0]==':'){pass=strdup(&pass[1]);}
      }
      if(channel[0]=='#'){channel=&channel[1];}
      channel=strdup(channel);
    }
    else if(!strncmp(buf, "PING ", 5))
    {
      dprintf(sock, ":irchack PONG %s\n", &buf[5]);
    }
  }
  // We now have what we need to launch tc_client
  printf("Nick: %s\n", nick);
  printf("Channel: %s\n", channel);
  printf("Password: %s\n", pass);
  int tc_in[2];
  int tc_out[2];
  pipe(tc_in);
  pipe(tc_out);
  if(!fork())
  {
    close(tc_in[1]);
    close(tc_out[0]);
    dup2(tc_in[0], 0);
    dup2(tc_out[1], 1);
    execl("./tc_client", "./tc_client", channel, nick, pass, (char*)0);
    printf("Failed to exec tc_client\n");
    _exit(1);
  }
  close(tc_in[0]);
  close(tc_out[1]);
  dprintf(sock, ":%s!user@host JOIN #%s\n", nick, channel);
  struct pollfd pfd[2];
  pfd[0].fd=tc_out[0];
  pfd[0].events=POLLIN;
  pfd[0].revents=0;
  pfd[1].fd=sock;
  pfd[1].events=POLLIN;
  pfd[1].revents=0;
  while(1)
  {
    poll(pfd, 2, -1);
    if(pfd[0].revents)
    {
      pfd[0].revents=0;
      len=0;
      while(1)
      {
        if(read(tc_out[0], &buf[len], 1)!=1 || buf[len]=='\r' || buf[len]=='\n'){break;}
        ++len;
      }
      buf[len]=0;
printf("Got from tc_client: '%s'\n", buf);
      if(!strncmp(buf, "Currently online: ", 18))
      {
        dprintf(sock, ":irchack 353 %s = #%s :", nick, channel);
        char* user=&buf[18];
        while(user)
        {
          char* next=strchr(user, ',');
          if(next){next[0]=0; next=&next[2];}
          dprintf(sock, "%s%s", user, next?" ":"");
          user=next;
        }
        write(sock, "\n", 1);
        dprintf(sock, ":irchack 366 %s #%s :End of /NAMES list.\n", nick, channel);
        continue;
      }
      if(buf[0]!='['){continue;} // Beyond this we only care about timestamped lines
      // Strip out ANSI escape codes (TODO: translate them to IRC color codes instead)
      char* ansi;
      while((ansi=strstr(buf, "\x1b[")))
      {
        int len;
        for(len=0; ansi[len]&&ansi[len]!='m'; ++len);
        if(ansi[len]=='m'){++len;}
        memmove(ansi, &ansi[len], strlen(&ansi[len])+1);
      }

      char* name=strchr(buf, ' ');
      if(!name){continue;}
      name[0]=0;
      name=&name[1];
      char* msg=name;
      while(msg[0]&&msg[0]!=':'&&msg[0]!=' '){msg=&msg[1];}
      if(msg[0]==':') // message
      {
        msg[0]=0;
        msg=&msg[2];
        if(!strncmp(msg, "/msg ", 5)) // PM
        {
          msg=strchr(&msg[5], ' ');
          if(!msg){continue;}
          msg=&msg[1];
          dprintf(sock, ":%s!user@host PRIVMSG %s :%s\n", name, nick, msg);
        }else{ // Regular channel message
          dprintf(sock, ":%s!user@host PRIVMSG #%s :%s\n", name, channel, msg);
        }
      }else{ // action, parse the actions and send them as JOINs, NICKs and QUITs etc. instead
        msg[0]=0;
        msg=&msg[1];
        if(!strcmp(msg, "entered the channel"))
        {
          dprintf(sock, ":%s!user@host JOIN #%s\n", name, channel);
        }
        else if(!strncmp(msg, "changed nickname to ", 20))
        {
          dprintf(sock, ":%s!user@host NICK :%s\n", name, &msg[20]);
        }
        else if(!strcmp(msg, "left the channel"))
        {
          dprintf(sock, ":%s!user@host QUIT :left the channel\n", name);
        }
        else // Unhandled action
        {
          dprintf(sock, ":%s!user@host PRIVMSG #%s :\x01""ACTION %s\x01\n", name, channel, msg);
        }
      }
    }
    if(pfd[1].revents)
    {
      pfd[1].revents=0;
      len=0;
      while(1)
      {
        if(read(sock, &buf[len], 1)!=1 || buf[len]=='\r' || buf[len]=='\n'){break;}
        ++len;
      }
      if(len<=0){continue;}
      while(len>0 && (buf[len-1]=='\n'||buf[len-1]=='\r')){--len;}
      buf[len]=0;
printf("Got from IRC client: '%s'\n", buf);
      if(!strncmp(buf, "PRIVMSG ", 8))
      {
        char* target=&buf[8];
        char* msg=strchr(&buf[8], ' ');
        if(!msg){continue;}
        msg[0]=0;
        msg=&msg[1];
        if(msg[0]==':'){msg=&msg[1];}
        if(target[0]=='#' && !strcmp(&target[1], channel))
        {
          dprintf(tc_in[1], "%s\n", msg);
        }else{
          dprintf(tc_in[1], "/msg %s %s\n", target, msg);
        }
      }
      else if(!strncmp(buf, "PING ", 5))
      {
        dprintf(sock, ":irchack PONG %s\n", &buf[5]);
      }
    }
  }

  shutdown(sock, SHUT_RDWR);
  return 0;
}
