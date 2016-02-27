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
#if LIBAVCODEC_VERSION_MAJOR<54 || (LIBAVCODEC_VERSION_MAJOR==54 && LIBAVCODEC_VERSION_MINOR<25)
  #define AV_CODEC_ID_FLV1 CODEC_ID_FLV1
  #define AV_CODEC_ID_NELLYMOSER CODEC_ID_NELLYMOSER
#endif
#if LIBAVUTIL_VERSION_MAJOR<51 || (LIBAVUTIL_VERSION_MAJOR==51 && LIBAVUTIL_VERSION_MINOR<42)
  #define AV_PIX_FMT_RGB24 PIX_FMT_RGB24
#endif
#if LIBAVUTIL_VERSION_MAJOR<52
  #define av_frame_alloc avcodec_alloc_frame
  #if LIBAVCODEC_VERSION_MAJOR<54 || (LIBAVCODEC_VERSION_MAJOR==54 && LIBAVCODEC_VERSION_MINOR<28)
    #define av_frame_free av_free
  #else
    #define av_frame_free avcodec_free_frame
  #endif
#endif
#if LIBAVCODEC_VERSION_MAJOR<55 || (LIBAVCODEC_VERSION_MAJOR==55 && LIBAVCODEC_VERSION_MINOR<52)
  #define avcodec_free_context(x) \
  { \
    avcodec_close(*x); \
    av_freep(&(*x)->extradata); \
    av_freep(&(*x)->subtitle_header); \
    av_freep(x); \
  }
#endif
