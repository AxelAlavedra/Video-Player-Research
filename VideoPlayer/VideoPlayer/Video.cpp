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
#include "Input.h"
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
	video_stream = -1;
	for (int i = 0; i < format->nb_streams; i++)
	{
		if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream = i;
			break;
		}
	}
	if (video_stream == -1)
		return -1; // Didn't find a video stream
			

	// Find the decoder for the video stream
	codec = avcodec_find_decoder(format->streams[video_stream]->codecpar->codec_id);
	if (codec == NULL) {
		LOG("Unsupported codec!\n");
	}

	codec_context = avcodec_alloc_context3(codec);
	// Parameter to context
	if (avcodec_parameters_to_context(codec_context, format->streams[video_stream]->codecpar) != 0) {
		LOG("Failed parameters to context");
	}

	// Open codec
	if (avcodec_open2(codec_context, codec, NULL) < 0)
	{
		LOG("Error opening codec");
	}

	uint dst_w, dst_h;
	App->win->GetWindowSize(dst_w, dst_h);
	sws_context = sws_getContext(format->streams[video_stream]->codecpar->width, format->streams[video_stream]->codecpar->height, (AVPixelFormat)format->streams[video_stream]->codecpar->format,
		dst_w , dst_h, AVPixelFormat::AV_PIX_FMT_YUV420P,
		SWS_BILINEAR, NULL, NULL, NULL);

	// Allocate video frame
	frame = av_frame_alloc();
	scaled_frame = av_frame_alloc();
	pkt = av_packet_alloc();
	av_init_packet(pkt);
	if (!pkt)
	{
		LOG("Error allocating packet");
	}
	if (!frame)
	{
		LOG("Error allocating frame");
	}

	// set up YV12 pixel array (12 bits per pixel)
	yPlaneSz = codec_context->width * codec_context->height;
	uvPlaneSz = codec_context->width * codec_context->height / 4;
	yPlane = (Uint8*)malloc(yPlaneSz);
	uPlane = (Uint8*)malloc(uvPlaneSz);
	vPlane = (Uint8*)malloc(uvPlaneSz);
	uvPitch = codec_context->width / 2;

	scaled_frame->data[0] = yPlane;
	scaled_frame->data[1] = uPlane;
	scaled_frame->data[2] = vPlane;

	scaled_frame->linesize[0] = codec_context->width;
	scaled_frame->linesize[1] = uvPitch;
	scaled_frame->linesize[2] = uvPitch;

	texture = SDL_CreateTexture(App->render->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
		dst_w, dst_h);

	return true;
}

bool Video::PreUpdate()
{
	return true;
}

bool Video::Update(float dt)
{
	if (App->input->GetKey(SDL_SCANCODE_F1) == KEY_DOWN)
		pause = !pause;

	while (!pause)
	{
		// read an encoded packet from file
		if (av_read_frame(format, pkt) < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "cannot read frame");
			break;
		}

		// if packet data is video data then send it to decoder
		if (pkt->stream_index == video_stream)
		{
			DecodeVideo(codec_context, frame, pkt);

			// release packet buffers to be allocated again
			av_packet_unref(pkt);

			break;
		}
	}

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


void Video::DecodeVideo(AVCodecContext* context, AVFrame* frame, AVPacket *pkt)
{
	//send packet to decoder
	int ret = avcodec_send_packet(context, pkt);
	if (ret < 0) {
		LOG("Error sending packet for decoding");
	}
	if (ret >= 0) {
		// receive frame from decoder
		// we may receive multiple frames or we may consume all data from decoder, then return to main loop
		ret = avcodec_receive_frame(context, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			LOG("Error during decoding");
		}

		sws_scale(sws_context, frame->data,
			frame->linesize, 0, frame->height, scaled_frame->data, scaled_frame->linesize);

		//Update video texture
		SDL_UpdateYUVTexture(texture, nullptr, scaled_frame->data[0], scaled_frame->linesize[0], scaled_frame->data[1],
			scaled_frame->linesize[1], scaled_frame->data[2], scaled_frame->linesize[2]);
	}
}