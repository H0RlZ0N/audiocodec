#ifndef __AAC_H__
#define __AAC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 
pcm: 8k采样率 s16le
aac: 48k采样率 fltp
 */

// aac解码: aac文件 --> pcm文件
int aac_decode(char *in_file, char *out_file);
// aac编码: pcm文件 --> aac文件 
int aac_encode(char *in_file, char *out_file, int sample_rate);

#ifdef __cplusplus
}
#endif

#endif