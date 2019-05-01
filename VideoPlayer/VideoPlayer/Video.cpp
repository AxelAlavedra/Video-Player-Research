extern "C" {
#include "ffmpeg/include/libavcodec/avcodec.h"
#include "ffmpeg/include/libavdevice/avdevice.h"
#include "ffmpeg/include/libavfilter/avfilter.h"
#include "ffmpeg/include/libavformat/avformat.h"
#include "ffmpeg/include/libavutil/avutil.h"
#include "ffmpeg/include/libswscale/swscale.h"

#pragma comment( lib, "ffmpeg/lib/avcodec.lib" )
#pragma comment( lib, "ffmpeg/lib/avdevice.lib" )
#pragma comment( lib, "ffmpeg/lib/avfilter.lib" )
#pragma comment( lib, "ffmpeg/lib/avformat.lib" )
#pragma comment( lib, "ffmpeg/lib/avutil.lib" )
#pragma comment( lib, "ffmpeg/lib/swscale.lib" )

}

#include "j1App.h"
#include "Render.h"
#include "Window.h"
#include "p2Log.h"
#include "Video.h"



Video::Video()
{
}


Video::~Video()
{
}

bool Video::Awake()
{


	return true;
}

bool Video::Start()
{
	AVFormatContext* format = NULL;
	AVCodecContext* codec_context = NULL;
	AVCodec* codec = NULL;

	std::string file = "videos/test_video.mp4";

	//Open video file
	if (avformat_open_input(&format, file.c_str(), NULL, NULL) != 0)
		LOG("Error loading video file %s", file);
	else
		LOG("Video file loaded correctly");

	// Retrieve stream information
	if (avformat_find_stream_info(format, NULL) <0)
		return -1; // Couldn't find stream information

	// Find the first video stream
	int videoStream = -1;
	for (int i = 0; i < format->nb_streams; i++)
	{
		if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	}
	if (videoStream == -1)
		return -1; // Didn't find a video stream
			

	// Find the decoder for the video stream
	codec = avcodec_find_decoder(format->streams[videoStream]->codecpar->codec_id);
	if (codec == NULL) {
		LOG("Unsupported codec!\n");
	}

	codec_context = avcodec_alloc_context3(codec);
	// Parameter to context
	if (avcodec_parameters_to_context(codec_context, format->streams[videoStream]->codecpar) != 0) {
		LOG("Failed parameters to context");
	}

	// Open codec
	if (avcodec_open2(codec_context, codec, NULL) < 0)
	{
		LOG("Error opening codec");
	}

	// Allocate video frame
	AVFrame *frame = av_frame_alloc();
	AVPacket *pkt = av_packet_alloc();
	av_init_packet(pkt);
	if (!pkt)
	{
		LOG("Error allocating packet");
	}
	if (!frame)
	{
		LOG("Error allocating frame");
	}


	texture = SDL_CreateTexture(App->render->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
		codec_context->width, codec_context->height);

	while (1)
	{
		// read an encoded packet from file
		if (av_read_frame(format, pkt) < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "cannot read frame");
			break;
		}
		// if packet data is video data then send it to decoder
		if (pkt->stream_index == videoStream)
		{
			Decode(codec_context, frame, pkt);
			break;
		}

		// release packet buffers to be allocated again
		av_packet_unref(pkt);
	}

	return true;
}

bool Video::PreUpdate()
{
	return true;
}

bool Video::Update(float dt)
{
	return true;
}

bool Video::PostUpdate()
{
	App->render->Blit(texture, 0, 0, nullptr);
	return true;
}

bool Video::CleanUp()
{
	return true;
}


void Video::Decode(AVCodecContext* context, AVFrame* frame, AVPacket *pkt)
{
	uint w, h;
	App->win->GetWindowSize(w, h);

	int ret;

	//send packet to decoder
	ret = avcodec_send_packet(context, pkt);
	if (ret < 0) {
		LOG("Error sending packet for decoding");
	}
	while (ret >= 0) {
		// receive frame from decoder
		// we may receive multiple frames or we may consume all data from decoder, then return to main loop
		ret = avcodec_receive_frame(context, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			// something wrong, quit program
			LOG("Error during decoding");
		}

		/*sws_scale(sws_ctx, (uint8_t const * const *)frame->data,
			frame->linesize, 0, context->height,
			frame->data, frame->linesize);*/
		DisplayFrame(frame, context);
	}
}

void Video::DisplayFrame(AVFrame* frame, AVCodecContext* context)
{
	SDL_UpdateYUVTexture(texture, nullptr, frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2]);
	/*SDL_RenderClear(App->render->renderer);
	SDL_RenderCopy(App->render->renderer, texture, NULL, NULL);
	SDL_RenderPresent(App->render->renderer);*/
}