#include "webm.h"

int webm_decode(char *in_file, char *out_file)
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
    int out_nb_samples = av_rescale_rnd(3600, out_sample_rate, codecpar->sample_rate, AV_ROUND_UP);
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
                                                          3600, out_sample_rate, codecpar->sample_rate, AV_ROUND_UP);
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
    if (out_fb) {
        fclose(out_fb);
    }

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