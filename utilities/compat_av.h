/*
    Some compatibility code for older libav* versions
    Copyright (C) 2016  alicia@ion.nu

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
#include "config.h"

#ifdef AVCODEC_NO_SEND_RECEIVE_API
  #include <libavcodec/avcodec.h>
  #define avcodec_send_packet compat_avcodec_send_packet
  #define avcodec_receive_frame compat_avcodec_receive_frame
  #define avcodec_send_frame compat_avcodec_send_frame
  #define avcodec_receive_packet compat_avcodec_receive_packet
  extern int compat_avcodec_send_packet(AVCodecContext* ctx, AVPacket* packet);
  extern int compat_avcodec_receive_frame(AVCodecContext* ctx, AVFrame* frame);
  extern int compat_avcodec_send_frame(AVCodecContext* ctx, AVFrame* frame);
  extern int compat_avcodec_receive_packet(AVCodecContext* ctx, AVPacket* packet);
#endif
