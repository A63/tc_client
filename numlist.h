/*
    tc_client, a simple non-flash client for tinychat(.com)
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
#ifdef __ANDROID__ // Compatibility hacks for android
  #define wchar_t char
  #define mbstowcs(dst,src,len) ((dst)?(int)strncpy(dst,src,len):strlen(src))
  #define wcstombs strncpy
  #define wcslen strlen
#endif

extern wchar_t* fromnumlist(char* in);
extern char* tonumlist(const wchar_t* in);
