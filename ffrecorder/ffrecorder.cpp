// ffrecorder.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "ffheader.h"


/**
* Convenience macro, the return value should be used only directly in
* function arguments but never stand-alone.
*/
static char *av_ts2str(int64_t ts) {
	char buf[1024] = { 0 };
	return av_ts_make_string(buf, ts);
}
static char * av_ts2timestr(int64_t ts, AVRational * tb){
	char buf[1024] = { 0 };
	return av_ts_make_time_string(buf, ts, tb);
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
	char *buf = av_ts2str(pkt->pts);
	printf("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		tag,
		av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
		av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
		av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
		pkt->stream_index);
}

typedef struct VideoState {
	SDL_Thread *video_tid;
	AVFormatContext *ifmt_ctx;
	char filename[1024];
	int video_id;
	AVPacket packet;
} VideoState;

static int video_thread(void *param)
{
	VideoState *is = (VideoState *)param;
	int ret = 0;
	if ((ret = avformat_open_input(&is->ifmt_ctx, is->filename, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file '%s'", is->filename);
		goto end;
	}

	if ((ret = avformat_find_stream_info(is->ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
		goto end;
	}
	av_dump_format(is->ifmt_ctx, 0, is->filename, 0);

	while (1) {
		AVPacket packet;
		
		ret = av_read_frame(is->ifmt_ctx, &packet);
		if (ret < 0)
			break;
		log_packet(is->ifmt_ctx, &packet, "in1");

		AVCodecContext *avctx = is->ifmt_ctx->streams[packet.stream_index]->codec;

		/* prepare audio output */
		if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			av_free_packet(&packet);
		}
		else{
			//TODO:Mutex...
			av_copy_packet(&is->packet, &packet);
		}
		SDL_Delay(30);
	}
end:
	return -1;
}


static VideoState *stream_open(const char *filename ,int video_id)
{
	VideoState *is;
	is = (VideoState *)av_mallocz(sizeof(VideoState));
	if (!is)
		return NULL;

	strcpy_s(is->filename, sizeof(is->filename), filename);
	is->video_id = video_id;
	char thread_name[100];
	sprintf_s(thread_name, "VideoThread_%d", video_id);

	is->video_tid = SDL_CreateThread(video_thread, thread_name, is);
	if (!is->video_tid) {
		av_free(is);
		return NULL;
	}
}


int _tmain(int argc, _TCHAR* argv[])
{
	AVOutputFormat *ofmt = NULL;
	AVFormatContext *ofmt_ctx = NULL;
	AVFormatContext *ifmt_ctx = NULL;
	AVPacket pkt;
	AVBitStreamFilterContext *vbsf = NULL;
	const char *in_filename,*in_filename1, *out_filename;

	int ret, i;

	in_filename = "G:/workspace/multirecorder/ffrecorder/Debug/xpt.mp4";
	in_filename1 = "G:/workspace/multirecorder/ffrecorder/Debug/xpt2.mp4";
	out_filename = "G:/workspace/multirecorder/ffrecorder/Debug/xpt/xpt.m3u8";

	av_register_all();

	VideoState *is1 = stream_open(in_filename1, 1);

	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file '%s'", in_filename);
		goto end;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
		goto end;
	}
	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	av_opt_set(ofmt_ctx->priv_data, "hls_list_size", "0", 0);
	av_opt_set(ofmt_ctx->priv_data, "hls_time", "3", 0);
	ofmt = ofmt_ctx->oformat;

	for (i = 0; i < 2; i++) {
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			fprintf(stderr, "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) {
			fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

		vbsf = av_bitstream_filter_init("h264_mp4toannexb");
	}
	av_dump_format(ofmt_ctx, 0, out_filename, 1);

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open output file '%s'", out_filename);
			goto end;
		}
	}

	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		goto end;
	}
	int numpackets = 0;
	while (1) {
		AVStream *in_stream, *out_stream;
		if (numpackets > 500){
			av_copy_packet(&pkt, &is1->packet);
		}
		else{
			ret = av_read_frame(ifmt_ctx, &pkt);
			if (ret < 0)
				break;
		}
		in_stream = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];
		is1->packet.pts++;
		is1->packet.dts++;
		log_packet(ifmt_ctx, &pkt, "in0");

		/* copy packet */
		
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		log_packet(ofmt_ctx, &pkt, "out");

		if (pkt.stream_index == 0){
			AVPacket fpkt = pkt;
			int a = av_bitstream_filter_filter(vbsf, out_stream->codec, NULL,&fpkt.data,&fpkt.size,
				pkt.data, pkt.size, pkt.flags & AV_PKT_FLAG_KEY);
			pkt.data = fpkt.data;
			pkt.size = fpkt.size;
		}
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		if (ret < 0) {
			fprintf(stderr, "Error muxing packet\n");
			break;
		}
		av_free_packet(&pkt);
		numpackets++;
		SDL_Delay(10);
	}

	av_write_trailer(ofmt_ctx);

end:

	avformat_close_input(&ifmt_ctx);

	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);

	av_bitstream_filter_close(vbsf);
	vbsf = NULL;

	if (ret < 0 && ret != AVERROR_EOF) {
		char errbuf[100];
		size_t errbuf_size = 100;
		av_strerror(ret, errbuf, errbuf_size);
		fprintf(stderr, "Error occurred: %s\n", errbuf);
		return 1;
	}
	return 0;
}

