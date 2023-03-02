#ifndef __WEBM_H__
#define __WEBM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswresample/swresample.h>
#include "libavutil/audio_fifo.h"
#include "libavutil/fifo.h"

/* 
pcm: 8k采样率 s16le
webm: 48k采样率 fltp
 */

// webm解码: webm文件 --> pcm文件
int webm_decode(char *in_file, char *out_file);

#ifdef __cplusplus
}
#endif

#endif