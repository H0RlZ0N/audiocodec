#ifndef __AAC_H__
#define __AAC_H__

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
aac: 48k采样率 fltp
 */

// aac解码: aac文件 --> pcm文件
int aac_decode(char *in_file, char *out_file);
// aac编码: pcm文件 --> aac文件 
int aac_encode(char *in_file, char *out_file, int sample_rate);
// 获取aac文件持续时长, 单位微秒 (us)
int64_t get_aac_duration(char *aacfile);

#ifdef __cplusplus
}
#endif

#endif