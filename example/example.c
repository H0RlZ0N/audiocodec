#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amrnb.h"
#include "aac.h"

// ----------------------------------------> test functions

int amrdecode(char* input, char* output)
{
	// test:
	int ret;
	AMRDecodeContext amrCtx;
	ret = amrnb_decode_init(&amrCtx);
	if (ret < 0) {
		printf("amrnb_decode_init error: %d\n", ret);
		return ret;
	}


	// 打开输入和输出文件
    FILE *infile = fopen(input, "rb");
    if (!infile) {
        fprintf(stderr, "Could not open %s\n", input);
        exit(1);
    }
    FILE* pcm_file = fopen(output, "wb");
    if (!pcm_file) {
        printf("Could not open {%s}", output);
    }

    int n;
    uint8_t header[6];
    n = fread(header, 1, 6, infile);
	if (n != 6 || memcmp(header, "#!AMR\n", 6)) {
		fprintf(stderr, "Bad header\n");
		return 1;
	}
    uint8_t bitdata[1];
    fread(bitdata, 1, 1, infile);
    fseek(infile, 6, SEEK_SET);
    int framelen = amrnb_get_framelen(amrnb_get_bitrate_mode(bitdata[0]));
    printf("----> mode: %d, framelen %d \n", bitdata[0] >> 3 & 0x0F, framelen);
	uint8_t amr_buf[500];

	for (;;) {
		size_t data_size;
        memset(amr_buf, 0, sizeof(amr_buf));
        data_size = fread(amr_buf, 1, framelen, infile);
        if(data_size <= 0) {
            printf("read file finish\n");
            break;
        }
		char pOutput[40960];
		int outsize = amrnb_decode_process(&amrCtx, amr_buf, data_size, pOutput);
		printf("outsize: %d\n", outsize);
		fwrite(pOutput, 1, outsize, pcm_file);
	}

	amrnb_decode_uninit(&amrCtx);
}

int amrencode(char* input, char* output)
{
	// test:
	int ret;
	AMREncodeContext amrCtx;
	ret = amrnb_encode_init(&amrCtx, amrnb_get_bitrate(MODE_7k95));
	if (ret < 0) {
		printf("amrnb_encode_init error: %d\n", ret);
		return ret;
	}

	// 打开输入和输出文件
    FILE *infile = fopen(input, "rb");
    if (!infile) {
        fprintf(stderr, "Could not open %s\n", input);
        exit(1);
    }
    FILE *outfile = fopen(output, "wb");
    if (!outfile) {
        fprintf(stderr, "Could not open %s\n", output);
        exit(1);
    }

	uint8_t pcm_buf[amrCtx.frame_bytes];

	for (;;) {
		size_t read_bytes;
        memset(pcm_buf, 0, sizeof(pcm_buf));
        read_bytes = fread(pcm_buf, 1, amrCtx.frame_bytes, infile);
        if(read_bytes <= 0) {
            printf("read file finish\n");
            break;
        }

		char pOutput[40960];
		int outsize = amrnb_encode_process(&amrCtx, pcm_buf, read_bytes, pOutput);
		fwrite(pOutput, 1, outsize, outfile);
	}
}

int main()
{
	printf("test\n");
	char* amrfile = "test.amr";
    char* pcmfile = "out.pcm";
	char* aacfile = "test.aac";
	//amrdecode(amrfile, pcmfile);
	//amrencode(pcmfile, "demotest.amr");
	//aac_decode(aacfile, "aac.pcm");
	aac_encode("aac.pcm", "out.aac", 48000);
}
