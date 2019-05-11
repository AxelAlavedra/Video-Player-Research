extern "C" {
#include "ffmpeg/include/libavcodec/avcodec.h"
#include "ffmpeg/include/libavdevice/avdevice.h"
#include "ffmpeg/include/libavfilter/avfilter.h"
#include "ffmpeg/include/libavformat/avformat.h"
#include "ffmpeg/include/libavutil/avutil.h"
#include "ffmpeg/include/libswscale/swscale.h"
#include "ffmpeg/include/libswresample/swresample.h"

#pragma comment( lib, "ffmpeg/lib/avcodec.lib" )
#pragma comment( lib, "ffmpeg/lib/avdevice.lib" )
#pragma comment( lib, "ffmpeg/lib/avfilter.lib" )
#pragma comment( lib, "ffmpeg/lib/avformat.lib" )
#pragma comment( lib, "ffmpeg/lib/avutil.lib" )
#pragma comment( lib, "ffmpeg/lib/swscale.lib" )
#pragma comment( lib, "ffmpeg/lib/swresample.lib" )

}

#include "j1App.h"
#include "Render.h"
#include "Input.h"
#include "Window.h"
#include "p2Log.h"
#include "Video.h"

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)


void AudioCallback(void *userdata, Uint8 *stream, int len) {

	Video *video = (Video*)userdata;
	int len1, audio_size;

	while (len > 0) {
		if (video->audio_buf_index >= video->audio_buf_size) {
			/* We have already sent all our data; get more */
			audio_size = video->DecodeAudio();
			if (audio_size < 0) {
				/* If error, output silence */
				video->audio_buf_size = 1024;
				memset(video->audio_buf, 0, video->audio_buf_size);
			}
			else {
				video->audio_buf_size = audio_size;
			}
			video->audio_buf_index = 0;
		}
		len1 = video->audio_buf_size - video->audio_buf_index;
		if (len1 > len)
			len1 = len;
		memcpy(stream, (uint8_t *)video->audio_buf + video->audio_buf_index, len1);
		len -= len1;
		stream += len1;
		video->audio_buf_index += len1;
	}
}

int VideoCallback(Uint32 interval, void* param)
{
	Video* player = (Video*)param;
	player->DecodeVideo();

	return 0;
}

int DecodeThread(void *param) {
	Video* player = (Video*)param;
	AVPacket* pkt = av_packet_alloc();

	while (true)
	{
		if (player->quit)
			break;

		if (player->audio_pktqueue.size > MAX_AUDIOQ_SIZE ||
			player->video_pktqueue.size > MAX_VIDEOQ_SIZE) {
			SDL_Delay(10);
			continue;
		}

		// read an encoded packet from file
		if (av_read_frame(player->format, pkt) < 0)
		{
			LOG("Error reading frame");
		}

		if (pkt->stream_index == player->video_stream) // if packet data is video we add to video queue
		{
			player->video_pktqueue.PutPacket(pkt);
		}
		else if (pkt->stream_index == player->audio_stream)
		{
			player->audio_pktqueue.PutPacket(pkt); // if packet data is audio we add to audio queue
		}
		else 
			av_packet_unref(pkt); // unsuported stream, release the packet
	}

	return 0;
}



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
	file = "videos/audio_video.mp4";

	//Open video file
	if (avformat_open_input(&format, file.c_str(), NULL, NULL) != 0)
		LOG("Error loading video file %s", file);
	else
		LOG("Video file loaded correctly");

	// Retrieve stream information
	if (avformat_find_stream_info(format, NULL) <0)
		return -1; // Couldn't find stream information

	// Find video and audio streams
	for (int i = 0; i < format->nb_streams; i++)
	{
		if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream < 0)
			video_stream = i;
		else if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream < 0)
			audio_stream = i;
	}

	if (video_stream == -1)
		LOG("No video stream found");
	else
		OpenStream(video_stream);

	if (audio_stream == -1)
		LOG("No audio stream found");
	else
		OpenStream(audio_stream);

	parse_thread_id = SDL_CreateThread(DecodeThread, "DecodeThread", this);

	return true;
}

bool Video::PreUpdate()
{
	return true;
}

bool Video::Update(float dt)
{
	if (App->input->GetKey(SDL_SCANCODE_F1) == KEY_DOWN)
	{
		pause = !pause;
		SDL_PauseAudio(0);
	}

	frame_amount++;
	if (frame_amount >= frame_ratio)
	{
		DecodeVideo();
		frame_amount = 0;
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
	quit = true;

	return true;
}

void Video::DecodeVideo()
{
	AVPacket* pkt = av_packet_alloc();
	if (video_pktqueue.GetPacket(pkt) < 0)
		return;

	//send packet to video decoder
	int ret = avcodec_send_packet(video_codec_context, pkt);
	if (ret < 0) {
		LOG("Error sending packet for decoding");
	}
	else {
		// receive frame from decoder
		// we may receive multiple frames or we may consume all data from decoder, then return to main loop
		ret = avcodec_receive_frame(video_codec_context, video_frame);
		LOG("%i", ret);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			LOG("Error during decoding");
		}

		sws_scale(sws_context, video_frame->data,
			video_frame->linesize, 0, video_frame->height, video_scaled_frame->data, video_scaled_frame->linesize);

		//Update video texture
		SDL_UpdateYUVTexture(texture, nullptr, video_scaled_frame->data[0], video_scaled_frame->linesize[0], video_scaled_frame->data[1],
			video_scaled_frame->linesize[1], video_scaled_frame->data[2], video_scaled_frame->linesize[2]);
	}
	av_packet_unref(pkt);
}

int Video::DecodeAudio()
{
	int ret;
	int len2, outSize = 0;


	if (audio_pktqueue.GetPacket(audio_pkt) < 0)
		return -1;

	ret = avcodec_send_packet(audio_codec_context, audio_pkt);
	if (ret) 
		return ret;
	else
	{
		ret = avcodec_receive_frame(audio_codec_context, audio_frame);

		outSize = av_samples_get_buffer_size(NULL,
			audio_codec_context->channels,
			audio_frame->nb_samples,
			AV_SAMPLE_FMT_FLT,
			1);

		// returns the number of samples per channel in one audio frame  -- 8192
		len2 = swr_convert(swr_context,
			converted_audio_frame->data,							// output
			audio_frame->nb_samples,
			(const uint8_t**)audio_frame->data,  // input
			audio_frame->nb_samples);



		memcpy(audio_buf, converted_audio_frame->data[0], outSize);

		/* We have data, return it and come back for more later */
		return outSize;
	}
}

void Video::OpenStream(int stream_index)
{
	AVCodecContext *codec_context;
	AVCodec *codec;
	SDL_AudioSpec wanted_spec, spec;

	if (stream_index < 0 || stream_index >= format->nb_streams) {
		LOG("Error not valid stream index");
	}

	// Find the decoder for stream
	codec = avcodec_find_decoder(format->streams[stream_index]->codecpar->codec_id);
	if (codec == NULL)
		LOG("Unsupported codec for stream index %i", stream_index);

	codec_context = avcodec_alloc_context3(codec);
	// Parameter to context for video
	if (avcodec_parameters_to_context(codec_context, format->streams[stream_index]->codecpar) != 0) {
		LOG("Failed parameters to context for stream %i", stream_index);
	}

	// Open codecs
	if (avcodec_open2(codec_context, codec, NULL) < 0)
	{
		LOG("Error opening codec");
	}

	switch (codec_context->codec_type)
	{
	case AVMEDIA_TYPE_VIDEO:
		video_codec_context = codec_context;
		video_codec = codec;

		video_frame = av_frame_alloc();
		video_scaled_frame = av_frame_alloc();

		uint dst_w, dst_h;
		App->win->GetWindowSize(dst_w, dst_h);

		//Prepare scaling frame.
		video_scaled_frame->format = AVPixelFormat::AV_PIX_FMT_YUV420P; // Format used for SDL
		video_scaled_frame->width = dst_w; //width of window
		video_scaled_frame->height = dst_h; //height of window
		av_frame_get_buffer(video_scaled_frame, 0); //get buffers for YUV format

		//Get context to scale video frames to format, width and height of our SDL window.
		sws_context = sws_getContext(video_codec_context->width, video_codec_context->height, video_codec_context->pix_fmt,
			dst_w, dst_h, AVPixelFormat::AV_PIX_FMT_YUV420P,
			SWS_BILINEAR, NULL, NULL, NULL);

		//Create texture where we will output the video.
		texture = SDL_CreateTexture(App->render->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
			dst_w, dst_h);

		video_pktqueue.Init();

		frame_ratio = App->GetFrameRate() / format->streams[video_stream]->r_frame_rate.num;
		//SDL_AddTimer(format->streams[video_stream]->r_frame_rate.num / 1000, (SDL_TimerCallback)VideoCallback, this);
		//video_thread_id = SDL_CreateThread(VideoThread, "VideoThread", this);
		break;
	case AVMEDIA_TYPE_AUDIO:
		audio_codec_context = codec_context;
		audio_codec = codec;

		audio_frame = av_frame_alloc();
		converted_audio_frame = av_frame_alloc();
		audio_pkt = av_packet_alloc();

		wanted_spec.freq = audio_codec_context->sample_rate;
		wanted_spec.format = AUDIO_F32;
		wanted_spec.channels = audio_codec_context->channels;
		wanted_spec.silence = 0;
		wanted_spec.samples = 1024;
		wanted_spec.callback = AudioCallback;
		wanted_spec.userdata = this;

		if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
			LOG("SDL_OpenAudio: %s\n", SDL_GetError());
			return;
		}

		swr_context = swr_alloc();
		//Unlike libavcodec and libavformat, this structure is opaque. This means that if you would like to set options, you must use the AVOptions API and cannot directly set values to members of the structure.
		av_opt_set_channel_layout(swr_context, "in_channel_layout", audio_codec_context->channel_layout, 0); // mono or streo or something else
		av_opt_set_channel_layout(swr_context, "out_channel_layout", audio_codec_context->channel_layout, 0);
		av_opt_set_int(swr_context, "in_sample_rate", audio_codec_context->sample_rate, 0);					// number of samples in one second
		av_opt_set_int(swr_context, "out_sample_rate", audio_codec_context->sample_rate, 0);
		av_opt_set_sample_fmt(swr_context, "in_sample_fmt", audio_codec_context->sample_fmt, 0);  // data structure of samples, data size // float planar
		av_opt_set_sample_fmt(swr_context, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0); // float

		//Get buffer to output converted audio
		converted_audio_frame->format = AV_SAMPLE_FMT_FLT; // Format used for SDL Audio
		converted_audio_frame->channel_layout = audio_codec_context->channel_layout;
		converted_audio_frame->nb_samples = 1024;
		av_frame_get_buffer(converted_audio_frame, 1);

		swr_init(swr_context);

		audio_pktqueue.Init();
		SDL_PauseAudio(0);
		break;
	}
}

void PacketQueue::Init()
{
	mutex = SDL_CreateMutex();
	cond = SDL_CreateCond();
}

int PacketQueue::PutPacket(AVPacket* pkt)
{
	AVPacketList *pkt_list;
	AVPacket new_pkt;
	if (av_packet_ref(&new_pkt, pkt) < 0) {
		return -1;
	}
	pkt_list = (AVPacketList*)av_malloc(sizeof(AVPacketList));
	if (!pkt_list)
		return -1;
	pkt_list->pkt = new_pkt;
	pkt_list->next = NULL;

	SDL_LockMutex(mutex);

	if (!last_pkt)
		first_pkt = pkt_list;
	else
		last_pkt->next = pkt_list;

	last_pkt = pkt_list;
	nb_packets++;
	size += pkt_list->pkt.size;

	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);
	return 0;
}

int PacketQueue::GetPacket(AVPacket* pkt)
{
	AVPacketList *pkt_list;
	int ret;

	SDL_LockMutex(mutex);

	for (;;) {

		/*if (quit) {
			ret = -1;
			break;
		}*/

		pkt_list = first_pkt;

		if (pkt_list) {
			first_pkt = pkt_list->next;
			if (!first_pkt)
				last_pkt = NULL;
			nb_packets--;
			size -= pkt_list->pkt.size;
			*pkt = pkt_list->pkt;
			av_free(pkt_list);
			ret = 1;
			break;
		}
		else {
			SDL_CondWait(cond, mutex);
		}
	}

	SDL_UnlockMutex(mutex);
	return ret;
}

