/*
    Some compatibility code to work on more limited platforms
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
#include "../config.h"
#if defined(__ANDROID__) || defined(_WIN32)
#include <stdint.h>
#include <stddef.h>
#define mbtowc(x,y,z) 1
#endif
#ifdef NO_DPRINTF
  extern size_t dprintf(int fd, const char* fmt, ...);
#endif
#ifdef NO_STRNDUP
  extern char* strndup(const char* in, unsigned int length);
#endif
#ifdef _WIN32
  #define prctl(...)
  #define wait(x)
  #define mkdir(x,y) mkdir(x)
#endif
#if GLIB_MAJOR_VERSION<2 || (GLIB_MAJOR_VERSION==2 && GLIB_MINOR_VERSION<2)
  #define g_io_channel_read_chars(a,b,c,d,e) g_io_channel_read(a,b,c,d)
#endif
