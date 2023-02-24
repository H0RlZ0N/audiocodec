#include <stdio.h>
#include <stdlib.h>
#include "amrnb.h"

enum Mode amrnb_get_bitrate_mode(uint8_t data)
{
    return data >> 3 & 0x0F;
}

int amrnb_get_bitrate(enum Mode mode)
{
    switch (mode)
    {
    case MODE_4k75:
        return 4750;
    case MODE_5k15:
        return 5150;
    case MODE_5k9:
        return 5900;
    case MODE_6k7:
        return 6700;
    case MODE_7k4:
        return 7400;
    case MODE_7k95:
        return 7950;
    case MODE_10k2:
        return 10200;
    case MODE_12k2:
        return 12200;
    default:
        return 4750;
    }
}

int amrnb_get_framelen(enum Mode mode)
{
    switch (mode)
    {
    case MODE_4k75:
        return 13;
    case MODE_5k15:
        return 14;
    case MODE_5k9:
        return 16;
    case MODE_6k7:
        return 18;
    case MODE_7k4:
        return 20;
    case MODE_7k95:
        return 21;
    case MODE_10k2:
        return 27;
    case MODE_12k2:
        return 32;
    default:
        return 32;
    }
}

char error_buf[1024] = {0};
char* av_get_err(int err_num) {
  av_strerror(err_num, error_buf, sizeof(error_buf));
  return error_buf;
}

// amr decode
int amrnb_decode_init(AMRDecodeContext* amr_ctx)
{
	if (amr_ctx == NULL) return -1;
	// 查找解码器
	int ret;
	amr_ctx->codec_id = AV_CODEC_ID_AMR_NB;
    amr_ctx->decodec = avcodec_find_decoder(amr_ctx->codec_id);
    if (!amr_ctx->decodec) {
        fprintf(stderr, "Codec not found\n");
        return -1;
    }

	amr_ctx->decodectx = avcodec_alloc_context3(amr_ctx->decodec);
    if (!amr_ctx->decodectx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        return -2;
    }

    amr_ctx->decodectx->sample_fmt = AV_SAMPLE_FMT_FLT;
	amr_ctx->decodectx->sample_rate = 8000;
    amr_ctx->decodectx->channel_layout = AV_CH_LAYOUT_MONO;
	amr_ctx->decodectx->channels = av_get_channel_layout_nb_channels(amr_ctx->decodectx->channel_layout);

	// 将编码器上下文和编码器进行关联
    if (avcodec_open2(amr_ctx->decodectx, amr_ctx->decodec, NULL) < 0) {
        fprintf(stderr, "open avcodec failed\n");
		avcodec_free_context(&amr_ctx->decodectx);
        return -3;
    }

	amr_ctx->decoded_frame = av_frame_alloc();
	amr_ctx->swrContext = swr_alloc();
	// 音频格式  重采样设置参数
    const enum AVSampleFormat in_sample = amr_ctx->decodectx->sample_fmt; // 原音频的采样位数
    const enum AVSampleFormat out_sample = AV_SAMPLE_FMT_S16; //16位
    int in_sample_rate = amr_ctx->decodectx->sample_rate; // 输入采样率
    int out_sample_rate = amr_ctx->decodectx->sample_rate; //输出采样
	// 输入声道布局
    uint64_t in_ch_layout = amr_ctx->decodectx->channel_layout;
    // 输出声道布局
    uint64_t out_ch_layout = amr_ctx->decodectx->channel_layout;
    swr_alloc_set_opts(amr_ctx->swrContext, out_ch_layout, out_sample, out_sample_rate, in_ch_layout, in_sample,
                        in_sample_rate, 0, NULL);
    swr_init(amr_ctx->swrContext);
    amr_ctx->out_channerl_nb = av_get_channel_layout_nb_channels(out_ch_layout);
    printf("out_channerl_nb %d \n", amr_ctx->out_channerl_nb);
	amr_ctx->out_buffer = (uint8_t *) av_malloc(2 * 44100);

	return 0;
}

int amrnb_decode_uninit(AMRDecodeContext* amr_ctx)
{
	if (!amr_ctx) return -1;
	if (amr_ctx->decodectx) {
        avcodec_close(amr_ctx->decodectx);
		avcodec_free_context(&amr_ctx->decodectx);
		amr_ctx->decodectx = NULL;
	}
	if (amr_ctx->decoded_frame) {
		av_frame_free(&amr_ctx->decoded_frame);
		amr_ctx->decoded_frame = NULL;
	}
	if (amr_ctx->out_buffer) {
		av_free(amr_ctx->out_buffer);
		amr_ctx->out_buffer = NULL;
	}
	if (amr_ctx->swrContext) {
		swr_close(amr_ctx->swrContext);
		swr_free(&amr_ctx->swrContext);
		amr_ctx->swrContext = NULL;
	}
	
	return 0;
}

int amrnb_decode_process(AMRDecodeContext* amr_ctx, char *pData, int nSize, char *pOutput)
{
	if (!amr_ctx) return -1;
	if (!amr_ctx->decodec || !amr_ctx->decodectx || !amr_ctx->decoded_frame || !amr_ctx->swrContext || !amr_ctx->out_buffer) {
		fprintf(stderr, "amrnb ctx error\n");
        return -2;
	}

	int outsize = 0;
	AVPacket pkt;
	av_new_packet(&pkt, nSize);
	pkt.data = pData;

	if (pkt.size > 0) {
		// send the packet with the compressed data to the decoder
		int ret = avcodec_send_packet(amr_ctx->decodectx, &pkt);
		if (ret == AVERROR(EAGAIN)) {
			printf("Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
		} else if (ret < 0) {
			printf("Error submitting the packet to the decoder, err: {%d}, pkt_size: {%d}",
				av_get_err(ret), pkt.size);
			return ret;
		}

		// read all the output frames (in general there may be any number of them
		while (ret >= 0) {
			ret = avcodec_receive_frame(amr_ctx->decodectx, amr_ctx->decoded_frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				printf("Error during decoding\n");
			}

			int data_size = av_get_bytes_per_sample(amr_ctx->decodectx->sample_fmt);
			if (data_size < 0) {
				// This should not occur, checking just for paranoia
				printf("Failed to calculate data size\n");
			}

			swr_convert(amr_ctx->swrContext, &amr_ctx->out_buffer, 2 * 44100,
					(const uint8_t **) amr_ctx->decoded_frame->data, amr_ctx->decoded_frame->nb_samples);
			int out_buffer_size = av_samples_get_buffer_size(NULL, amr_ctx->out_channerl_nb, amr_ctx->decoded_frame->nb_samples,
																AV_SAMPLE_FMT_S16, 1);
			
			memcpy(pOutput+outsize , amr_ctx->out_buffer, out_buffer_size);
			outsize += out_buffer_size;
		}

		av_packet_unref(&pkt);
		
		return outsize;
	}

	return 0;
}

// amr encode
int amrnb_encode_init(AMREncodeContext* amr_ctx, int bit_rate)
{
	if (amr_ctx == NULL) return -1;
	// 查找编码器
	int ret;
	amr_ctx->codec_id = AV_CODEC_ID_AMR_NB;
	amr_ctx->encodec = avcodec_find_encoder(amr_ctx->codec_id);
    if (!amr_ctx->encodec) {
        fprintf(stderr, "Codec not found\n");
        return -2;
    }

    amr_ctx->encodectx = avcodec_alloc_context3(amr_ctx->encodec);
    if (!amr_ctx->encodectx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        return -3;
    }
    amr_ctx->encodectx->codec_id = amr_ctx->codec_id;
    amr_ctx->encodectx->codec_type = AVMEDIA_TYPE_AUDIO;
    amr_ctx->encodectx->channel_layout = AV_CH_LAYOUT_MONO;
    amr_ctx->encodectx->sample_rate = 8000;
    amr_ctx->encodectx->bit_rate = (int64_t)bit_rate;
    amr_ctx->encodectx->channels = av_get_channel_layout_nb_channels(amr_ctx->encodectx->channel_layout);
    amr_ctx->encodectx->sample_fmt = AV_SAMPLE_FMT_S16;

	// 将编码器上下文和编码器进行关联
    if (avcodec_open2(amr_ctx->encodectx, amr_ctx->encodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return -4;
    }

	printf("frame_size %d\n", amr_ctx->encodectx->frame_size);
 
    // 分配frame
    amr_ctx->encoded_frame = av_frame_alloc();
    if (!amr_ctx->encoded_frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        return -5;
    }

	amr_ctx->encoded_frame->nb_samples     = amr_ctx->encodectx->frame_size;
    amr_ctx->encoded_frame->format         = amr_ctx->encodectx->sample_fmt;
    amr_ctx->encoded_frame->channel_layout = amr_ctx->encodectx->channel_layout;
    amr_ctx->encoded_frame->channels = av_get_channel_layout_nb_channels(amr_ctx->encoded_frame->channel_layout);

	// 为frame分配buffer
    ret = av_frame_get_buffer(amr_ctx->encoded_frame, 0);
    if (ret < 0) {
		av_frame_free(&amr_ctx->encoded_frame);
        fprintf(stderr, "Could not allocate audio data buffers\n");
        return -6;
    }

	// 分配packet
    amr_ctx->pkt = av_packet_alloc();
    if (!amr_ctx->pkt) {
		av_frame_free(&amr_ctx->encoded_frame);
        fprintf(stderr, "could not allocate the packet\n");
        return -7;
    }

	amr_ctx->pts = 0;
	amr_ctx->frame_bytes = av_get_bytes_per_sample(amr_ctx->encoded_frame->format) \
            * amr_ctx->encoded_frame->channels \
            * amr_ctx->encoded_frame->nb_samples;
    printf("frame_bytes %d\n", amr_ctx->frame_bytes);

	return 0;
}

int amrnb_encode_uninit(AMREncodeContext* amr_ctx)
{
	if (!amr_ctx) return -1;
	if (amr_ctx->encodectx) {
		avcodec_free_context(&amr_ctx->encodectx);
		amr_ctx->encodectx = NULL;
	}
	if (amr_ctx->pkt) {
		av_packet_free(&amr_ctx->pkt);
		amr_ctx->pkt = NULL;
	}
	if (amr_ctx->encoded_frame) {
		av_frame_free(&amr_ctx->encoded_frame);
		amr_ctx->encoded_frame = NULL;
	}
	
	return 0;
}

int amrnb_encode_process(AMREncodeContext* amr_ctx, char *pData, int nSize, char *pOutput)
{
	if (!amr_ctx) return -1;
	if (!amr_ctx->encodec || !amr_ctx->encodectx || !amr_ctx->encoded_frame) {
		fprintf(stderr, "amrnb ctx error\n");
        return -2;
	}

	int ret;
	int outsize = 0;

	// 填充音频帧
	if(AV_SAMPLE_FMT_S16 == amr_ctx->encoded_frame->format) {
		// 将读取到的PCM数据填充到frame去，但要注意格式的匹配, 是planar还是packed都要区分清楚
		ret = av_samples_fill_arrays(amr_ctx->encoded_frame->data, amr_ctx->encoded_frame->linesize,
								pData, amr_ctx->encoded_frame->channels,
								amr_ctx->encoded_frame->nb_samples, amr_ctx->encoded_frame->format, 0);
	}

	// 编码
	amr_ctx->pts += amr_ctx->encoded_frame->nb_samples;
	amr_ctx->encoded_frame->pts = amr_ctx->pts;       // 使用采样率作为pts的单位，具体换算成秒 pts*1/采样率

	/* send the frame for encoding */
    ret = avcodec_send_frame(amr_ctx->encodectx, amr_ctx->encoded_frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        return -3;
    }

	while (ret >= 0) {
        ret = avcodec_receive_packet(amr_ctx->encodectx, amr_ctx->pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            return -4;
        }
 
        memcpy(pOutput+outsize , amr_ctx->pkt->data, amr_ctx->pkt->size);
		outsize += amr_ctx->pkt->size;
    }

    return outsize;
}
