/*
    tc_client, a simple non-flash client for tinychat(.com)
    Copyright (C) 2014-2015  alicia@ion.nu

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
#define COLORCOUNT 16
extern const char* colors[];
extern const char* termcolors[];
extern unsigned int currentcolor;
extern char hexcolors;
extern char showcolor;

extern const char* resolvecolor(const char* tc_color);
extern const char* color_start(const char* hex);
extern const char* color_end(void);
