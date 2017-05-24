/*
    tc_client-gtk, a graphical user interface for tc_client
    Copyright (C) 2015-2017  alicia@ion.nu

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
#define TC_CLIENT (frombuild?"./tc_client":"tc_client")
extern int tc_client_in[2];
extern const char* channel;
extern char* nickname;
extern char hasgreenroom;
extern char frombuild;
extern gboolean handledata(GIOChannel* iochannel, GIOCondition condition, gpointer x);
extern void togglecam(GtkCheckMenuItem* item, void* x);
#ifdef HAVE_PULSEAUDIO
void togglemic(GtkCheckMenuItem* item, void* x);
#endif
extern gboolean inputkeys(GtkWidget* widget, GdkEventKey* event, void* data);
extern void sendmessage(GtkEntry* entry, void* x);
extern void startsession(GtkButton* button, void* x);
extern void captcha_done(void* x, void* y);
