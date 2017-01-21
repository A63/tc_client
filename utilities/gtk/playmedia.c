/*
    tc_client-gtk, a graphical user interface for tc_client
    Copyright (C) 2016-2017  alicia@ion.nu

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
#ifdef HAVE_AVFORMAT
#include <stdio.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/opt.h>
#include "media.h"
#include "main.h"
#include "config.h"
#include "compat.h"
#include "playmedia.h"

struct frame
{
  unsigned long int duration;
  unsigned long int pts;
  void* buf;
  unsigned int size;
};

struct framequeue
{
  struct frame* frames;
  unsigned int size;
  unsigned int len;
};

struct playmediadata
{
  char* id;
  int fd;
  long int seek;
};

static GThread* playingmedia=0;
static long int playmedia_seek=-1;

static void queue_push(struct framequeue* queue, AVPacket* packet, unsigned long int duration, unsigned long int pts)
{
  if(queue->len==queue->size)
  {
    queue->size+=10;
    queue->frames=realloc(queue->frames, sizeof(struct frame)*queue->size);
  }
  queue->frames[queue->len].duration=duration;
  queue->frames[queue->len].pts=pts;
  queue->frames[queue->len].buf=malloc(packet->size);
  queue->frames[queue->len].size=packet->size;
  // Prefer pts (presentation timestamp) when available (more reliable than the provided duration)
  if(queue->len && pts!=AV_NOPTS_VALUE && queue->frames[queue->len-1].pts!=AV_NOPTS_VALUE)
  {
    queue->frames[queue->len-1].duration=pts-queue->frames[queue->len-1].pts;
  }
  memcpy(queue->frames[queue->len].buf, packet->data, packet->size);
  ++queue->len;
}

static void queue_pop(struct framequeue* queue)
{
  free(queue->frames[0].buf);
  usleep(queue->frames[0].duration);
  --queue->len;
  memmove(queue->frames, &queue->frames[1], sizeof(struct frame)*queue->len);
}

static void queue_empty(struct framequeue* queue)
{
  unsigned int i;
  for(i=0; i<queue->len; ++i)
  {
    free(queue->frames[i].buf);
  }
  free(queue->frames);
  queue->frames=0;
  queue->len=0;
  queue->size=0;
}

// TODO: Handle soundcloud
// TODO: Handle /mbsp (start paused), /mbpl (resume), /mbpa (pause)
void* playmedia(void* data)
{
  struct playmediadata* pmd=data;
  int fd=pmd->fd;
  playmedia_seek=pmd->seek;
  // Get the URL for the actual file
  if(strchr(pmd->id, '"')){free(pmd->id); free(pmd); return 0;}
  char cmd[strlen("youtube-dl -f \"best[height<=480]\" -g -- \"\"0")+strlen(pmd->id)];
  strcpy(cmd, "youtube-dl -f \"best[height<=480]\" -g -- \"");
  strcat(cmd, pmd->id);
  strcat(cmd, "\"");
  free(pmd->id);
  free(pmd);
  FILE* ytdl=popen(cmd, "r");
  if(!ytdl){return 0;}
  char url[2048];
  fgets(url, 2048, ytdl);
  pclose(ytdl);
  char* end;
  if((end=strchr(url, '\n'))){end[0]=0;}
  if((end=strchr(url, '\r'))){end[0]=0;}
  if(!url[0]){return 0;}

  av_register_all();
  avformat_network_init();
  AVDictionary* options=0;
  av_dict_set(&options, "user_agent", "tc_client-gtk (using libavformat)", 0);
  AVFormatContext* avfctx=0;
  if(avformat_open_input(&avfctx, url, 0, &options)){return 0;}
  avformat_find_stream_info(avfctx, 0);
  AVCodec* vcodec=0;
  AVCodec* acodec=0;
  int stream_v=av_find_best_stream(avfctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);
  int stream_a=av_find_best_stream(avfctx, AVMEDIA_TYPE_AUDIO, -1, -1, &acodec, 0);

  // Ask the main thread to set up the camera for us
  write(fd, "Starting media stream for YouTube (media)\n", 42);
  struct camera* cam;
  while(!(cam=camera_find("media")) && playingmedia){usleep(1000);}
  if(!cam)
  {
    write(fd, "VideoEnd: media\n", 16);
    avformat_close_input(&avfctx);
    return 0;
  }
  // Set up video decoder
  cam->vctx=avcodec_alloc_context3(vcodec);
  avcodec_parameters_to_context(cam->vctx, avfctx->streams[stream_v]->codecpar);
  avcodec_open2(cam->vctx, vcodec, 0);
  // Set up audio decoder+resampler
  cam->actx=avcodec_alloc_context3(acodec);
  avcodec_parameters_to_context(cam->actx, avfctx->streams[stream_a]->codecpar);
  avcodec_open2(cam->actx, acodec, 0);
  cam->samplerate=cam->actx->sample_rate;
  // TODO: Figure out why planar formats don't resample well without pretending they're non-planar (sample case: PXj51mQHz5I)
  //  For now, just pass off the first planar channel as non-planar single-channel
  enum AVSampleFormat samplefmt=av_get_packed_sample_fmt(cam->actx->sample_fmt);
  int chanlayout=cam->actx->channel_layout;
  if(samplefmt!=cam->actx->sample_fmt){chanlayout=AV_CH_FRONT_CENTER;}
  #ifdef HAVE_AVRESAMPLE
    cam->resamplectx=avresample_alloc_context();
    av_opt_set_int(cam->resamplectx, "in_channel_layout", chanlayout, 0);
    av_opt_set_int(cam->resamplectx, "in_sample_fmt", samplefmt, 0);
    av_opt_set_int(cam->resamplectx, "in_sample_rate", cam->actx->sample_rate, 0);
    av_opt_set_int(cam->resamplectx, "out_channel_layout", AV_CH_FRONT_CENTER, 0);
    av_opt_set_int(cam->resamplectx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int(cam->resamplectx, "out_sample_rate", SAMPLERATE_OUT, 0);
    avresample_open(cam->resamplectx);
  #elif defined(HAVE_SWRESAMPLE)
    cam->swrctx=swr_alloc_set_opts(0, AV_CH_FRONT_CENTER, AV_SAMPLE_FMT_S16, SAMPLERATE_OUT, chanlayout, samplefmt, cam->actx->sample_rate, 0, 0);
    swr_init(cam->swrctx);
  #endif

  AVPacket packet={0};
  struct framequeue vqueue={0,0,0};
  struct framequeue aqueue={0,0,0};
  while(playingmedia && av_read_frame(avfctx, &packet)==0)
  {
    unsigned long int num=avfctx->streams[packet.stream_index]->time_base.num;
    unsigned long int den=avfctx->streams[packet.stream_index]->time_base.den;
    unsigned long int pts=packet.pts;
    if(pts!=AV_NOPTS_VALUE){pts=num*1000000*pts/den;}
    if(packet.stream_index==stream_v)
    {
      queue_push(&vqueue, &packet, num*1000000*packet.duration/den, pts);
    }
    else if(packet.stream_index==stream_a)
    {
      queue_push(&aqueue, &packet, num*1000000*packet.duration/den, pts);
    }
    // TODO: Handle video-only and audio-only?
    if(aqueue.len<2 || vqueue.len<2){continue;}
    if(aqueue.frames[0].duration>vqueue.frames[0].duration)
    {
      aqueue.frames[0].duration-=vqueue.frames[0].duration;
      dprintf(fd, "Video: media %u\n", vqueue.frames[0].size+1);
      write(fd, "\x22", 1);
      write(fd, vqueue.frames[0].buf, vqueue.frames[0].size);
      queue_pop(&vqueue);
    }else{
      vqueue.frames[0].duration-=aqueue.frames[0].duration;
      dprintf(fd, "Audio: media %u\n", aqueue.frames[0].size+1);
      write(fd, "\x22", 1);
      write(fd, aqueue.frames[0].buf, aqueue.frames[0].size);
      queue_pop(&aqueue);
    }

    if(playmedia_seek!=-1)
    {
      // Convert tinychat's milliseconds to whatever libavformat uses (microseconds)
      av_seek_frame(avfctx, -1, playmedia_seek*AV_TIME_BASE/1000, AVSEEK_FLAG_ANY);
      playmedia_seek=-1;
      queue_empty(&vqueue);
      queue_empty(&aqueue);
    }
  }
  write(fd, "VideoEnd: media\n", 16);
  queue_empty(&vqueue);
  queue_empty(&aqueue);
  playingmedia=0;
  avformat_close_input(&avfctx);
  return 0;
}

void media_play(const char* args)
{
  const char* id=strchr(args, ' ');
  if(!id){return;}
  id=&id[1];
  const char* timestampstr=strchr(id, ' ');
  if(!timestampstr){return;}
  long int timestamp=strtol(&timestampstr[1], 0, 0);
  int p[2];
  pipe(p);
  GIOChannel* iochannel=g_io_channel_unix_new(p[0]);
  g_io_channel_set_encoding(iochannel, 0, 0);
  g_io_add_watch(iochannel, G_IO_IN, handledata, 0);

  struct playmediadata* pmd=malloc(sizeof(struct playmediadata));
  pmd->id=strndup(id, timestampstr-id);
  pmd->fd=p[1];
  pmd->seek=(timestamp?timestamp:-1);
  media_close();
  playingmedia=g_thread_new("media", playmedia, pmd);
}

void media_seek(long int ms)
{
  playmedia_seek=ms;
}

void media_close(void)
{
  if(playingmedia)
  {
    GThread* t=playingmedia;
    playingmedia=0;
    g_thread_join(t);
    camera_remove("media", 0);
  }
}
#endif
