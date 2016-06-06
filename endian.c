/*
    tc_client, a simple non-flash client for tinychat(.com)
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
#include "endian.h"

uint64_t be64(uint64_t in)
{
#if(__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
  return ((in&0xff)<<56) |
         ((in&0xff00)<<40) |
         ((in&0xff0000)<<24) |
         ((in&0xff000000)<<8) |
         ((in&0xff00000000)>>8) |
         ((in&0xff0000000000)>>24) |
         ((in&0xff000000000000)>>40) |
         ((in&0xff00000000000000)>>56);
#else
  return in;
#endif
}

uint32_t be32(uint32_t in)
{
#if(__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
  return ((in&0xff)<<24) |
         ((in&0xff00)<<8) |
         ((in&0xff0000)>>8) |
         ((in&0xff000000)>>24);
#else
  return in;
#endif
}

uint32_t le32(uint32_t in)
{
#if(__BYTE_ORDER__==__ORDER_BIG_ENDIAN__)
  return ((in&0xff)<<24) |
         ((in&0xff00)<<8) |
         ((in&0xff0000)>>8) |
         ((in&0xff000000)>>24);
#else
  return in;
#endif
}

uint16_t be16(uint16_t in)
{
#if(__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
  return ((in&0xff)<<8) |
         ((in&0xff00)>>8);
#else
  return in;
#endif
}

uint16_t le16(uint16_t in)
{
#if(__BYTE_ORDER__==__ORDER_BIG_ENDIAN__)
  return ((in&0xff00)<<8) |
         ((in&0xff)>>8);
#else
  return in;
#endif
}
