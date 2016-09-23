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
#include "compat_av.h"

#ifdef AVCODEC_NO_SEND_RECEIVE_API
#include <libavcodec/avcodec.h>
void* compat_sendreceive_object=0;

int compat_avcodec_send_packet(AVCodecContext* ctx, AVPacket* packet)
{
  compat_sendreceive_object=packet;
  return 0;
}

int compat_avcodec_receive_frame(AVCodecContext* ctx, AVFrame* frame)
{
  int got;
  if(ctx->codec_type==AVMEDIA_TYPE_VIDEO)
  {
    avcodec_decode_video2(ctx, frame, &got, compat_sendreceive_object);
  }else{
    avcodec_decode_audio4(ctx, frame, &got, compat_sendreceive_object);
  }
  return !got;
}

int compat_avcodec_send_frame(AVCodecContext* ctx, AVFrame* frame)
{
  compat_sendreceive_object=frame;
  return 0;
}

int compat_avcodec_receive_packet(AVCodecContext* ctx, AVPacket* packet)
{
  int got;
  if(ctx->codec_type==AVMEDIA_TYPE_VIDEO)
  {
    avcodec_encode_video2(ctx, packet, compat_sendreceive_object, &got);
  }
  return !got;
}
#endif
