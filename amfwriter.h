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
#include <stdlib.h>
#include "rtmp.h"

extern void amfinit(struct rtmp* msg, unsigned int streamid);
extern void amfnum(struct rtmp* msg, double v);
extern void amfbool(struct rtmp* msg, char v);
extern void amfstring(struct rtmp* msg, const char* string);
extern void amfobjstart(struct rtmp* msg);
extern void amfobjitem(struct rtmp* msg, char* name);
extern void amfobjend(struct rtmp* msg);
extern void amfnull(struct rtmp* msg);

#define amfsend(rtmp,sock) rtmp_send(sock,rtmp); free((rtmp)->buf); (rtmp)->buf=0
