#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#define INBUF_SIZE 4096

#define WORD uint16_t
#define DWORD uint32_t
#define LONG int32_t

int timebase;
int avgFrameRate;
uint8_t *images[30];
int count = 0;
int uw;
int uh;

#pragma pack(2)
typedef struct tagBITMAPFILEHEADER {
  WORD  bfType;
  DWORD bfSize;
  WORD  bfReserved1;
  WORD  bfReserved2;
  DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;


typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG  biWidth;
  LONG  biHeight;
  WORD  biPlanes;
  WORD  biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};

int convertPixFmt(struct SwsContext *img_convert_ctx, AVFrame *frame, AVFrame *pFrameRGB)
{
    //1 先进行转换,  YUV420=>RGB24:
    int w = frame->width;
    int h = frame->height;


    int numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, w, h);
    uint8_t *buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));


     /* buffer is going to be written to rawvideo file, no alignment */
    /*
    if (av_image_alloc(pFrameRGB->data, pFrameRGB->linesize,
                              w, h, AV_PIX_FMT_BGR24, pix_fmt, 1) < 0) {
        fprintf(stderr, "Could not allocate destination image\n");
        exit(1);
    }
    */
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24, w, h);

    sws_scale(img_convert_ctx, frame->data, frame->linesize,
              0, h, pFrameRGB->data, pFrameRGB->linesize);

	return 0;
}

void saveImage(int rows)
{
    //2 构造 BITMAPINFOHEADER
    BITMAPINFOHEADER header;
    header.biSize = sizeof(BITMAPINFOHEADER);

    int imagesLen = count;
    int cols = imagesLen / rows;
    rows = imagesLen < rows ? imagesLen : rows;
    if (!cols) cols++;

    header.biWidth = rows * uw;
    header.biHeight = cols * uh*(-1);
    header.biBitCount = 24;
    header.biCompression = 0;
    header.biSizeImage = 0;
    header.biClrImportant = 0;
    header.biClrUsed = 0;
    header.biXPelsPerMeter = 0;
    header.biYPelsPerMeter = 0;
    header.biPlanes = 1;

    //3 构造文件头
    BITMAPFILEHEADER bmpFileHeader = {0,};
    //HANDLE hFile = NULL;
    DWORD dwTotalWriten = 0;
    DWORD dwWriten;

    bmpFileHeader.bfType = 0x4d42; //'BM';
    bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)+ uw * uh * 3 * 3;
    bmpFileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);

    FILE* pf = fopen("test.bmp", "wb");
    fwrite(&bmpFileHeader, sizeof(BITMAPFILEHEADER), 1, pf);
    fwrite(&header, sizeof(BITMAPINFOHEADER), 1, pf);

	// 先分组
	uint8_t *imageGroups[cols][rows];
	int r = 0;
	int c = 0;
	for (int i = 0; i < imagesLen; i++) {
		//printf("aaaaaaa%p", images[i]);
		imageGroups[i / 5][i % 5] = images[i];
	}

    for (int i = 0; i < cols; i++) {
    	for (int j = 0; j < uh; j++) {
		for (int k = 0; k < rows; k++) {
			if (imageGroups[i][k]) {
				//printf("%d, %d, %d", i, j, k);
				fwrite(imageGroups[i][k] + j * uw * 3, 1, uw * 3, pf);
			}
		}
	}
    }
    fclose(pf);


    //释放资源
    //av_free(buffer);
    //av_freep(&pFrameRGB[0]);
//    av_free(images);
}

static int decode_write_frame(const char *outfilename, AVCodecContext *avctx,
                              struct SwsContext *img_convert_ctx, AVFrame *frame, int *frame_count, AVPacket *pkt, int last)
{
    int len, got_frame;
    char buf[1024];

    len = avcodec_decode_video2(avctx, frame, &got_frame, pkt);
    if (len < 0) {
        fprintf(stderr, "Error while decoding frame %d\n", *frame_count);
        return len;
    }
    if (got_frame && *frame_count % (avgFrameRate * 10) == 0) {
        printf("Saving %sframe %3d, %d\n", last ? "last " : "", *frame_count, count);
        fflush(stdout);

        /* the picture is allocated by the decoder, no need to free it */
        snprintf(buf, sizeof(buf), "%s-%d.bmp", outfilename, *frame_count);

        /*
        pgm_save(frame->data[0], frame->linesize[0],
                 frame->width, frame->height, buf);
        */

        // saveBMP(img_convert_ctx, frame, buf);
	AVFrame *frameRGB;
	frameRGB = av_frame_alloc();
	convertPixFmt(img_convert_ctx, frame, frameRGB);
	uw = frame->width;
	uh = frame->height;
//	uint8_t *cp = copyUint8Array(frameRGB->data[0]);
	printf("ptr of data: %p", frameRGB->data[0]);
	images[count] = frameRGB->data[0];
	av_frame_free(&frameRGB);
	count++;
    }

    (*frame_count)++;

    if (last) saveImage(5);

    if (pkt->data) {
        pkt->size -= len;
        pkt->data += len;
    }
    return 0;
}

AVCodexContext *initDecoder(AVFormatContext *av_fmt_ctx, filename)
{
    const AVCodec *codec;
    AVCodecContext *c= NULL;
    AVStream *st = NULL;
    int stream_index;

    /* open input */
    if (avformat_open_input(&av_fmt_ctx, filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open input\n");
        exit(1);
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(av_fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    /* dump input information to stderr */
    av_dump_format(av_fmt_ctx, 0, filename, 0);

    ret = av_find_best_stream(av_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO), filename);
        return ret;
    }

    stream_index = ret;
    st = av_fmt_ctx->streams[stream_index];
    timebase = st->time_base.den;
    avgFrameRate = st->avg_frame_rate.num;

    /* find decoder for the stream */
    codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Failed to find %s codec\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return AVERROR(EINVAL);
    }

    c = avcodec_alloc_context3(NULL);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    /* Copy codec parameters from input stream to output codec context */
    if ((ret = avcodec_parameters_to_context(c, st->codecpar)) < 0) {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return ret;
    }

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    return c;
}

Sprite *generateSprite(AVFormatContext *av_fmt_ctx, char *filename)
{
    int ret;
    int frame_count;
    AVFrame *frame;
    struct SwsContext *img_convert_ctx;
    AVPacket avpkt;
    AVCodexContext *codex_ctx;

    codec_ctx = initDecoder(av_fmt_ctx, filename);

    img_convert_ctx = sws_getContext(c->width, c->height,
                                     c->pix_fmt,
                                     c->width, c->height,
                                     AV_PIX_FMT_RGB24,
                                     SWS_BICUBIC, NULL, NULL, NULL);

    if (img_convert_ctx == NULL)
    {
        fprintf(stderr, "Cannot initialize the conversion context\n");
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    frame_count = 0;
    av_init_packet(&avpkt);
    while (av_read_frame(av_fmt_ctx, &avpkt) >= 0) {
        if(avpkt.stream_index == stream_index){
            if (decode_write_frame(outfilename, c, img_convert_ctx, frame, &frame_count, &avpkt, 0) < 0)
                exit(1);
        }

        av_packet_unref(&avpkt);
    }

    avpkt.data = NULL;
    avpkt.size = 0;
    decode_write_frame(outfilename, c, img_convert_ctx, frame, &frame_count, &avpkt, 1);

    fclose(f);

    avformat_close_input(&av_fmt_ctx);

    sws_freeContext(img_convert_ctx);
    avcodec_free_context(&c);
    av_frame_free(&frame);

    return 0;
}
