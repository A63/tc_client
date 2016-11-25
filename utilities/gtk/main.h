/*
    tc_client-gtk, a graphical user interface for tc_client
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
extern char* nickname;
extern void togglecam(GtkCheckMenuItem* item, void* x);
#ifdef HAVE_PULSEAUDIO
void togglemic(GtkCheckMenuItem* item, void* x);
#endif
extern gboolean inputkeys(GtkWidget* widget, GdkEventKey* event, void* data);
extern void sendmessage(GtkEntry* entry, void* x);
extern void startsession(GtkButton* button, void* x);
extern void captcha_done(GtkWidget* button, void* x);
