#include <stdio.h>
#include <stdlib.h>
#include "aac.h"

 
/* 检测该编码器是否支持该采样格式 */
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;
 
    while (*p != AV_SAMPLE_FMT_NONE) { // 通过AV_SAMPLE_FMT_NONE作为结束符
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}
 
/* 检测该编码器是否支持该采样率 */
static int check_sample_rate(const AVCodec *codec, const int sample_rate)
{
    const int *p = codec->supported_samplerates;
    while (*p != 0)  {// 0作为退出条件，比如libfdk-aacenc.c的aac_sample_rates
        printf("%s support %dhz\n", codec->name, *p);
        if (*p == sample_rate)
            return 1;
        p++;
    }
    return 0;
}
 
/* 检测该编码器是否支持该采样率, 该函数只是作参考 */
static int check_channel_layout(const AVCodec *codec, const uint64_t channel_layout)
{
    // 不是每个codec都给出支持的channel_layout
    const uint64_t *p = codec->channel_layouts;
    if(!p) {
        printf("the codec %s no set channel_layouts\n", codec->name);
        return 1;
    }
    while (*p != 0) { // 0作为退出条件，比如libfdk-aacenc.c的aac_channel_layout
        printf("%s support channel_layout %d\n", codec->name, *p);
        if (*p == channel_layout)
            return 1;
        p++;
    }
    return 0;
}
 
static int check_codec( AVCodec *codec, AVCodecContext *codec_ctx)
{
 
    if (!check_sample_fmt(codec, codec_ctx->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
                av_get_sample_fmt_name(codec_ctx->sample_fmt));
        return 0;
    }
    if (!check_sample_rate(codec, codec_ctx->sample_rate)) {
        fprintf(stderr, "Encoder does not support sample rate %d", codec_ctx->sample_rate);
        return 0;
    }
    if (!check_channel_layout(codec, codec_ctx->channel_layout)) {
        fprintf(stderr, "Encoder does not support channel layout %lu", codec_ctx->channel_layout);
        return 0;
    }
 
    printf("\nAudio encode config\n");
    printf("sample_rate:%d\n", codec_ctx->sample_rate);
    printf("sample_fmt:%s\n", av_get_sample_fmt_name(codec_ctx->sample_fmt));
    printf("channels:%d\n", codec_ctx->channels);
 
    return 1;
}

static void get_adts_header(AVCodecContext *ctx, uint8_t *adts_header, int aac_length)
{
    uint8_t freq_idx = 0;    //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
    switch (ctx->sample_rate) {
        case 96000: freq_idx = 0; break;
        case 88200: freq_idx = 1; break;
        case 64000: freq_idx = 2; break;
        case 48000: freq_idx = 3; break;
        case 44100: freq_idx = 4; break;
        case 32000: freq_idx = 5; break;
        case 24000: freq_idx = 6; break;
        case 22050: freq_idx = 7; break;
        case 16000: freq_idx = 8; break;
        case 12000: freq_idx = 9; break;
        case 11025: freq_idx = 10; break;
        case 8000: freq_idx = 11; break;
        case 7350: freq_idx = 12; break;
        default: freq_idx = 4; break;
    }
    uint8_t chanCfg = ctx->channels;
    uint32_t frame_length = aac_length + 7;
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF1;
    adts_header[2] = ((ctx->profile) << 6) + (freq_idx << 2) + (chanCfg >> 2);
    adts_header[3] = (((chanCfg & 3) << 6) + (frame_length  >> 11));
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    adts_header[6] = 0xFC;
}

// aac decode
int aac_decode(char *in_file, char *out_file)
{
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *cod_ctx = NULL;
    AVCodec *cod = NULL;
    int ret = 0;
    int outsize = 0;
    AVPacket packet;
 
    //创建输入文件AVFormatContext
    fmt_ctx = avformat_alloc_context();
    if (fmt_ctx == NULL) {
        ret = -1;
        fprintf(stderr, "alloc fail\n");
        goto __ERROR;
    }
    if (avformat_open_input(&fmt_ctx, in_file, NULL, NULL) != 0) {
        ret = -1;
        fprintf(stderr, "open fail\n");
        goto __ERROR;
    }
 
    //查找文件相关流，并初始化AVFormatContext中的流信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        ret = -1;
        fprintf(stderr, "find stream fail\n");
        goto __ERROR;
    }
 
    av_dump_format(fmt_ctx, 0, in_file, 0);
 
    //查找音频流索引和解码器
    int stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &cod, -1);
 
    //设置解码器上下文并打开解码器
    AVCodecParameters *codecpar = fmt_ctx->streams[stream_index]->codecpar;
    if (!cod) {
        ret = -1;
        fprintf(stderr, "find codec fail\n");
        goto __ERROR;
    }
    cod_ctx = avcodec_alloc_context3(cod);
    avcodec_parameters_to_context(cod_ctx, codecpar);
    ret = avcodec_open2(cod_ctx, cod, NULL);
    if (ret < 0) {
        fprintf(stderr, "can't open codec\n");
        goto __ERROR;
    }

    // 打开输出文件
    FILE* out_fb = fopen(out_file, "wb");
    if (!out_fb) {
        printf("Could not open {%s}\n", out_file);
    }
 
    //创建packet,用于存储解码前的数据
    av_init_packet(&packet);
 
    //设置转码后输出相关参数
    //采样的布局方式
    uint64_t out_channel_layout = AV_CH_LAYOUT_MONO;
    //采样格式
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    //采样率
    int out_sample_rate = 8000;
    //通道数
    int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
    //采样个数
    int out_nb_samples = av_rescale_rnd(codecpar->frame_size, out_sample_rate, codecpar->sample_rate, AV_ROUND_UP);
    int max_dst_nb_samples = out_nb_samples;
    printf("channels: %d, out_nb_samples: %d\n", out_channels, out_nb_samples);

    AVFrame *frame = av_frame_alloc();
    av_frame_get_buffer(frame, 0);
 
    //重采样初始化与设置参数
    struct SwrContext *convert_ctx = swr_alloc();
    convert_ctx = swr_alloc_set_opts(convert_ctx,
                                     out_channel_layout,
                                     out_sample_fmt,
                                     out_sample_rate,
                                     codecpar->channel_layout,
                                     codecpar->format,
                                     codecpar->sample_rate,
                                     0,
                                     NULL);
    swr_init(convert_ctx);

    uint8_t **data = NULL;
    int dst_linesize;
    ret = av_samples_alloc_array_and_samples(&data, &dst_linesize, out_channels,
                                             out_nb_samples, out_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        goto __ERROR;
    }
    //while循环，每次读取一帧，并转码
    while (av_read_frame(fmt_ctx, &packet) >= 0) {
 
        if (packet.stream_index != stream_index) {
            continue;
        }
 
        ret = avcodec_send_packet(cod_ctx, &packet);
        if (ret < 0){
            ret = -1;
            fprintf(stderr, "decode error\n");
            goto __ERROR;
        }
 
        while (avcodec_receive_frame(cod_ctx, frame) >= 0) {
            out_nb_samples = av_rescale_rnd(swr_get_delay(convert_ctx, codecpar->sample_rate) + 
                                                          codecpar->frame_size, out_sample_rate, codecpar->sample_rate, AV_ROUND_UP);
            if (out_nb_samples > max_dst_nb_samples) {
                av_freep(&data[0]);
                ret = av_samples_alloc(data, &dst_linesize, out_channels,
                                    out_nb_samples, out_sample_fmt, 1);
                if (ret < 0)
                    break;
                max_dst_nb_samples = out_nb_samples;
            }
            ret = swr_convert(convert_ctx, data, out_nb_samples, (const uint8_t **)frame->data, frame->nb_samples);
            if (ret < 0) {
                ret = -1;
                fprintf(stderr, "swr_convert error\n");
                goto __ERROR;
            }
            int buffer_size = av_samples_get_buffer_size(&dst_linesize, out_channels, ret, out_sample_fmt, 1);
            fwrite(data[0], 1, buffer_size, out_fb);
            outsize += buffer_size;
        }
 
        av_packet_unref(&packet);
    }

__ERROR:
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        avformat_free_context(fmt_ctx);
    }

    if (cod_ctx) {
        avcodec_close(cod_ctx);
        avcodec_free_context(&cod_ctx);
    }

    if (frame) {
        av_frame_free(&frame);
    }

    if (convert_ctx) {
        swr_free(&convert_ctx);
    }

    return outsize;
}

// aac encode
int aac_encode(char *in_file, char *out_file, int sample_rate)
{
    int ret;
    int outsize = 0;
    int dst_linesize;
    int64_t cur_pts = 0;
    uint8_t **data = NULL;
    AVCodec *encodec = NULL;
    AVCodecContext *encodectx = NULL;
    struct SwrContext *convert_ctx = NULL;
    AVFifoBuffer *audiofifo = NULL;
    AVFormatContext *oc = NULL;
    AVStream *st = NULL;
    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;

    // 打开输入输出文件
    FILE* in_fb = fopen(in_file, "rb");
    if (!in_fb) {
        printf("Could not open {%s}\n", in_file);
    }

    // 查找编码器
    encodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!encodec) {
        fprintf(stderr, "Codec not found\n");
        return -1;
    }

    encodectx = avcodec_alloc_context3(encodec);
    if (!encodectx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        return -2;
    }
    encodectx->codec_id = AV_CODEC_ID_AAC;
    encodectx->codec_type = AVMEDIA_TYPE_AUDIO;
    encodectx->channel_layout = AV_CH_LAYOUT_MONO;
    encodectx->sample_rate = sample_rate;
    encodectx->channels = av_get_channel_layout_nb_channels(encodectx->channel_layout);
    encodectx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    encodectx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!check_codec(encodec, encodectx)) {
        fprintf(stderr, "Check codec error\n");
        avcodec_close(encodectx);
        avcodec_free_context(&encodectx);
        return -3;
    }

	// 将编码器上下文和编码器进行关联
    if (avcodec_open2(encodectx, encodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        avcodec_close(encodectx);
        avcodec_free_context(&encodectx);
        return -4;
    }

    // 打开输出封装上下文
    avformat_alloc_output_context2(&oc, NULL, NULL, out_file);
	if (!oc) {
        fprintf(stderr, "Could not alloc output context\n");
        avcodec_close(encodectx);
        avcodec_free_context(&encodectx);
        return -1;
	}
    st = avformat_new_stream(oc, NULL);
    st->codecpar->codec_tag = 0;
	avcodec_parameters_from_context(st->codecpar, encodectx);
	av_dump_format(oc, 0, out_file, 1);

    // 输出封装写入头
	ret = avio_open(&oc->pb, out_file, AVIO_FLAG_WRITE);
	if (ret < 0) {
        avcodec_close(encodectx);
        avcodec_free_context(&encodectx);
        avformat_free_context(oc);
		fprintf(stderr, "Could not avio open\n");
		return -1;
 
	}
	ret = avformat_write_header(oc, NULL);

    // 音频重采样 上下文初始化
    uint64_t in_channel_layout = AV_CH_LAYOUT_MONO;
    int in_nb_samples = 1024;
    int in_sample_rate = 8000;
    int in_channels = av_get_channel_layout_nb_channels(in_channel_layout);
    enum AVSampleFormat in_sample_fmt = AV_SAMPLE_FMT_S16;
    //采样个数
    int out_nb_samples = av_rescale_rnd(encodectx->frame_size, encodectx->sample_rate, in_sample_rate, AV_ROUND_UP);
    int max_dst_nb_samples = out_nb_samples;
    printf("channels: %d, nb_samples: %d\n", encodectx->channels, out_nb_samples);

    int pcms16leSize = av_get_bytes_per_sample(in_sample_fmt) \
            * in_channels \
            * in_nb_samples;
    int pcmfltpSize = av_get_bytes_per_sample(encodectx->sample_fmt) \
            * encodectx->channels \
            * encodectx->frame_size;
    char pcm_s16le[pcms16leSize];
    char pcm_pltp[pcmfltpSize];
    printf("s16le: %d, fltp: %d\n", pcms16leSize, pcmfltpSize);
    printf("frame_size %d\n", encodectx->frame_size);


    // 重采样初始化与设置参数
    convert_ctx = swr_alloc();
    convert_ctx = swr_alloc_set_opts(convert_ctx,
                                     encodectx->channel_layout,
                                     encodectx->sample_fmt,
                                     encodectx->sample_rate,
                                     in_channel_layout,
                                     in_sample_fmt,
                                     in_sample_rate,
                                     0,
                                     NULL);
    swr_init(convert_ctx);

    
    ret = av_samples_alloc_array_and_samples(&data, &dst_linesize, encodectx->channels,
                                             out_nb_samples, encodectx->sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        goto __ERROR;
    }


    // 初始化audiofifo
    audiofifo = av_fifo_alloc(1); 
    if (!audiofifo) { 
        fprintf(stderr, "Could not allocate fifo\n");
        goto __ERROR; 
    }


    // 分配frame
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        goto __ERROR;
    }
    frame->nb_samples = encodectx->frame_size;
    frame->format = encodectx->sample_fmt;
    frame->channel_layout = encodectx->channel_layout;
    frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);
    // 为frame分配buffer
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        goto __ERROR;;
    }

    // 分配packet
    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "could not allocate the packet\n");
        goto __ERROR;;
    }

    for (;;)
	{
        out_nb_samples = av_rescale_rnd(swr_get_delay(convert_ctx, in_sample_rate) + 
                                                          in_nb_samples, encodectx->sample_rate, in_sample_rate, AV_ROUND_UP);
        if (out_nb_samples > max_dst_nb_samples) {
            av_freep(&data[0]);
            ret = av_samples_alloc(data, &dst_linesize, encodectx->channels,
                                out_nb_samples, encodectx->sample_fmt, 1);
            if (ret < 0) {
                printf("av_samples_alloc fail: %d\n", ret);
                break;
            }
            max_dst_nb_samples = out_nb_samples;
        }

        memset(pcm_s16le, 0, sizeof(pcm_s16le));
		int len = fread(pcm_s16le, 1, pcms16leSize, in_fb);
		if (len <= 0) {
            break;
        }
        const uint8_t *pcmdata[1];
		pcmdata[0] = (uint8_t*)pcm_s16le;

        // 开始进行重采样 s16le -> fltp
        ret = swr_convert(convert_ctx, data, out_nb_samples, (const uint8_t **)pcmdata, frame->nb_samples);
        if (ret < 0) {
            ret = -1;
            fprintf(stderr, "swr_convert error\n");
            break;
        }
        int buffer_size = av_samples_get_buffer_size(&dst_linesize, encodectx->channels, ret, encodectx->sample_fmt, 1);
        //printf("buffer_size: %d\n", buffer_size);
        /**
         * 重采样出来的pcm无法直接编码成aac，
         * aac每帧需要4096个字节
         * 解决方案是使用AVFifoBuffer缓冲起来
         */
        int cache_size = av_fifo_size(audiofifo);
        av_fifo_realloc2(audiofifo, cache_size + buffer_size);
        av_fifo_generic_write(audiofifo, data[0], buffer_size, NULL);

        av_frame_make_writable(frame);
        while (av_fifo_size(audiofifo) > pcmfltpSize) {
            memset(pcm_pltp, 0, sizeof(pcm_pltp));
            int ret = av_fifo_generic_read(audiofifo, pcm_pltp, pcmfltpSize, NULL);
            if (ret < 0) {
                fprintf(stderr, "av_audio_fifo_read error\n");
                break;
            }
            //fwrite(pcm_pltp, 1, pcmfltpSize, test_fb);

            ret = av_samples_fill_arrays(frame->data, frame->linesize,
                                   pcm_pltp, frame->channels,
                                   frame->nb_samples, frame->format, 0);

            cur_pts += frame->nb_samples;
            frame->pts = cur_pts;

            /* send the frame for encoding */
            ret = avcodec_send_frame(encodectx, frame);
            if (ret < 0) {
                fprintf(stderr, "Error sending the frame to the encoder\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_packet(encodectx, pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error encoding audio frame\n");
                    break;
                }
                // 音频封装进AAC文件
                pkt->stream_index = 0;
                pkt->pts = 0;
                pkt->dts = 0;
                ret = av_interleaved_write_frame(oc, pkt);
                if (ret < 0) {
                    fprintf(stderr, "Error write frame\n");
                    break;
                }
            }
        }
    }

    av_write_trailer(oc);

__ERROR:
    if (encodectx) {
        avcodec_close(encodectx);
        avcodec_free_context(&encodectx);
    }

    avformat_free_context(oc);
    avio_close(oc->pb);

    av_freep(&data[0]);

    if (audiofifo) {
        av_fifo_freep(&audiofifo);
    }

    if (frame) {
        av_frame_free(&frame);
        frame = NULL;
    }

    if (pkt) {
        av_packet_free(&pkt);
        pkt = NULL;
    }

    if (convert_ctx) {
        swr_free(&convert_ctx);
    }

    if (in_fb) {
        fclose(in_fb);
    }

    return outsize;
}