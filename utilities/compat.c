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
#include "compat.h"
#if defined(__ANDROID__) || defined(_WIN32)
// Android and windows have no dprintf, so we make our own
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
size_t dprintf(int fd, const char* fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  int len=vsnprintf(0, 0, fmt, va);
  va_end(va);
  char buf[len+1];
  va_start(va, fmt);
  vsnprintf(buf, len+1, fmt, va);
  va_end(va);
  buf[len]=0;
  write(fd, buf, len);
  return len;
}
#endif
