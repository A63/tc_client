/*
    camviewer, a sample application to view tinychat cam streams
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
struct user
{
  char* nick;
  GtkWidget* label;
//  unsigned int id; // hm, tc_client doesn't share IDs other than in guestnicks, this might be useful for a ban-after-they-left situation
  char ismod;
  GtkWidget* pm_tab;
  GtkWidget* pm_tablabel;
  GtkTextBuffer* pm_buffer;
  GtkAdjustment* pm_scroll;
  char pm_highlight;
};

extern struct user* userlist;
extern unsigned int usercount;
extern GtkWidget* userlistwidget;

extern struct user* finduser(const char* nick);
extern struct user* user_find_by_tab(GtkWidget* tab);
extern struct user* adduser(const char* nick);
extern void renameuser(const char* old, const char* newnick);
extern void removeuser(const char* nick);
