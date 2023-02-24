#ifndef __AMRNB_H__
#define __AMRNB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswresample/swresample.h>

/** Frame type (Table 1a in 3GPP TS 26.101) */
enum Mode {
    MODE_4k75 = 0,                        ///< 4.75 kbit/s
    MODE_5k15,                            ///< 5.15 kbit/s
    MODE_5k9,                             ///< 5.90 kbit/s
    MODE_6k7,                             ///< 6.70 kbit/s
    MODE_7k4,                             ///< 7.40 kbit/s
    MODE_7k95,                            ///< 7.95 kbit/s
    MODE_10k2,                            ///< 10.2 kbit/s
    MODE_12k2,                            ///< 12.2 kbit/s
    MODE_DTX,                             ///< silent frame
    N_MODES,                              ///< number of modes
    NO_DATA = 15                          ///< no transmission
};

// 获取帧长
int amrnb_get_framelen(enum Mode mode);
// 获取 bitrate
enum Mode amrnb_get_bitrate_mode(uint8_t data);
int amrnb_get_bitrate(enum Mode mode);


// amr解码
typedef struct AMRDecodeContext{
    enum AVCodecID codec_id;
    AVCodec *decodec;
    AVCodecContext *decodectx;
    AVFrame *decoded_frame;
    SwrContext *swrContext;
    int out_channerl_nb;
    uint8_t *out_buffer;
} AMRDecodeContext;

int amrnb_decode_init(AMRDecodeContext* amr_ctx);
int amrnb_decode_uninit(AMRDecodeContext* amr_ctx);
int amrnb_decode_process(AMRDecodeContext* amr_ctx, char *pData, int nSize, char *pOutput);

// amr编码
typedef struct AMREncodeContext{
    enum AVCodecID codec_id;
    int bit_rate;
    int64_t pts;
    AVCodec *encodec;
    AVCodecContext *encodectx;
    AVFrame *encoded_frame;
    AVPacket *pkt;
    int frame_bytes;
} AMREncodeContext;

int amrnb_encode_init(AMREncodeContext* amr_ctx, int bit_rate);
int amrnb_encode_uninit(AMREncodeContext* amr_ctx);
int amrnb_encode_process(AMREncodeContext* amr_ctx, char *pData, int nSize, char *pOutput);

#ifdef __cplusplus
}
#endif

#endif
