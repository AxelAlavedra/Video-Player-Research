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
#define MAX_AUDIOQ_SIZE (5 * 256 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 512 * 1024)

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

	player->refresh = true;

	return 0;
}

int DecodeThread(void *param) {
	Video* player = (Video*)param;
	AVPacket pkt1, *pkt = &pkt1;

	while (player->playing)
	{
		if (player->audio_pktqueue.size >= MAX_AUDIOQ_SIZE 
			|| player->video_pktqueue.size >= MAX_VIDEOQ_SIZE)
		{
			Sleep(10);
			continue;
		}

		// read an encoded packet from file
		if (av_read_frame(player->format, pkt) < 0)
		{
			LOG("Error reading packet");
			break;
		}

		if (pkt->stream_index == player->video_stream_index) // if packet data is video we add to video queue
		{
			player->video_pktqueue.PutPacket(pkt);
		}
		else if (pkt->stream_index == player->audio_stream_index)
		{
			player->audio_pktqueue.PutPacket(pkt); // if packet data is audio we add to audio queue
		}
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

bool Video::Awake(pugi::xml_node&)
{
	return true;
}

bool Video::Start()
{
	texture_cond = SDL_CreateCond();
	texture_mutex = SDL_CreateMutex();
	return true;
}

bool Video::PreUpdate()
{
	return true;
}

bool Video::Update(float dt)
{
	if (App->input->GetKey(SDL_SCANCODE_F1) == KEY_DOWN && !playing)
		PlayVideo("videos/Warcraft III_ Reforged Cinematic Trailer.mp4");

	if (App->input->GetKey(SDL_SCANCODE_F2) == KEY_DOWN && playing)
		Pause();

	if (App->input->GetKey(SDL_SCANCODE_F3) == KEY_DOWN && playing)
		CleanVideo();


	if (refresh)
	{
		refresh = false;
		DecodeVideo();
	}
	return true;
}

bool Video::PostUpdate()
{
	if (playing)
	{
		if(texture)
			App->render->Blit(texture, 0, 0, nullptr);
	}
	return true;
}

bool Video::CleanUp()
{
	if(playing)
		CleanVideo();

	return true;
}

int Video::PlayVideo(std::string file_path)
{
	file = file_path;

	//Open video file
	if (avformat_open_input(&format, file.c_str(), NULL, NULL) != 0)
	{
		LOG("Error loading video file %s", file);
		return -1;
	}
	else
		LOG("Video file loaded correctly");

	// Retrieve stream information
	if (avformat_find_stream_info(format, NULL) <0)
		return -1; // Couldn't find stream information

				   // Find video and audio streams
	for (int i = 0; i < format->nb_streams; i++)
	{
		if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index < 0)
			video_stream_index = i;
		else if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_index < 0)
			audio_stream_index = i;
	}

	if (video_stream_index == -1)
		LOG("No video stream found");
	else
		OpenStream(video_stream_index);

	if (audio_stream_index == -1)
		LOG("No audio stream found");
	else
		OpenStream(audio_stream_index);

	playing = true;
	parse_thread_id = SDL_CreateThread(DecodeThread, "DecodeThread", this);
	SDL_AddTimer(40, (SDL_TimerCallback)VideoCallback, this);
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
		video_stream = format->streams[stream_index];
		video_context = codec_context;
		video_stream_index = stream_index;

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
		sws_context = sws_getContext(codec_context->width, codec_context->height, codec_context->pix_fmt,
			dst_w, dst_h, AVPixelFormat::AV_PIX_FMT_YUV420P,
			SWS_BILINEAR, NULL, NULL, NULL);

		//Create texture where we will output the video.
		texture = SDL_CreateTexture(App->render->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
			dst_w, dst_h);


		video_pktqueue.Init();

		break;
	case AVMEDIA_TYPE_AUDIO:
		audio_stream = format->streams[stream_index];
		audio_context = codec_context;
		audio_stream_index = stream_index;

		audio_frame = av_frame_alloc();
		converted_audio_frame = av_frame_alloc();

		wanted_spec.freq = codec_context->sample_rate;
		wanted_spec.format = AUDIO_S16SYS;
		wanted_spec.channels = codec_context->channels;
		wanted_spec.silence = 0;
		wanted_spec.samples = 1148;
		wanted_spec.callback = AudioCallback;
		wanted_spec.userdata = this;

		if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
			LOG("SDL_OpenAudio: %s\n", SDL_GetError());
		}

		swr_context = swr_alloc_set_opts(NULL, codec_context->channel_layout, AV_SAMPLE_FMT_FLT, codec_context->sample_rate,
			codec_context->channel_layout, codec_context->sample_fmt, codec_context->sample_rate, 0, NULL);
		swr_init(swr_context);

		//Get buffer to output converted audio
		converted_audio_frame->format = AV_SAMPLE_FMT_FLT; // Format used for SDL Audio
		converted_audio_frame->channel_layout = codec_context->channel_layout;
		converted_audio_frame->nb_samples = 1148;
		av_frame_get_buffer(converted_audio_frame, 0);


		audio_pktqueue.Init();
		SDL_PauseAudio(0);

		break;
	}
}

bool Video::Pause()
{
	pause = !pause;

	SDL_PauseAudio(0);
	audio_pktqueue.pause = pause;
	video_pktqueue.pause = pause;

	return true;
}

void Video::CleanVideo()
{
	playing = false;
	audio_pktqueue.pause = true;
	video_pktqueue.pause = true;

	SDL_WaitThread(parse_thread_id, NULL);
	SDL_CondSignal(video_pktqueue.cond);
	SDL_CondSignal(audio_pktqueue.cond);

	SDL_Delay(40);

	if(!pause)
		SDL_PauseAudio(0);

	video_pktqueue.Clear();
	audio_pktqueue.Clear();


	sws_freeContext(sws_context);
	sws_context = nullptr;
	swr_free(&swr_context);

	av_frame_free(&video_frame);
	av_frame_free(&video_scaled_frame);
	av_frame_free(&audio_frame);
	av_frame_free(&converted_audio_frame);

	avcodec_close(audio_context);
	avcodec_close(video_context);
	avcodec_free_context(&audio_context);
	avcodec_free_context(&video_context);
	avformat_close_input(&format);

	SDL_DestroyTexture(texture);
	texture = nullptr;
	SDL_CloseAudio();
	audio_buf_index = 0;
	audio_buf_size = 0;
	video_clock = 0;
	audio_clock = 0;
	video_stream_index = -1;
	audio_stream_index = -1;
}

void Video::DecodeVideo()
{
	AVPacket pkt;
	int ret;

	if (!playing)
		return;
	if (video_pktqueue.GetPacket(&pkt) < 0)
	{
		SDL_AddTimer(1, (SDL_TimerCallback)VideoCallback, this);
		return;
	}

	//send packet to video decoder
	ret = avcodec_send_packet(video_context, &pkt);
	if (ret < 0)
	{
		LOG("Error sending packet for decoding");
		return;
	}

	ret = avcodec_receive_frame(video_context, video_frame);
	sws_scale(sws_context, video_frame->data,
		video_frame->linesize, 0, video_frame->height, video_scaled_frame->data, video_scaled_frame->linesize);


	SDL_LockMutex(texture_mutex);
	//Update video texture
	SDL_UpdateYUVTexture(texture, nullptr, video_scaled_frame->data[0], video_scaled_frame->linesize[0], video_scaled_frame->data[1],
		video_scaled_frame->linesize[1], video_scaled_frame->data[2], video_scaled_frame->linesize[2]);
	SDL_CondSignal(texture_cond);
	SDL_UnlockMutex(texture_mutex);

	double pts = video_frame->pts;
	if (pts == AV_NOPTS_VALUE)
	{
		pts = video_clock +
			(1.f / av_q2d(video_stream->avg_frame_rate)) / av_q2d(video_stream->time_base);
	}
	video_clock = pts;


	double delay = (video_clock*av_q2d(video_stream->time_base)) - (audio_clock*av_q2d(audio_stream->time_base));
	if (delay < 0.010)
		delay = 0.010; //Maybe skip frame if video is too far behind from audio instead of fast refresh.

	static char title[256];
	sprintf_s(title, 256, " Video seconds: %.2f Audio seconds: %.2f Calculated delay %.2f",
		video_clock*av_q2d(video_stream->time_base), audio_clock*av_q2d(audio_stream->time_base), delay);
	App->win->SetTitle(title);


	//Prepare VideoCallback on ms
	SDL_AddTimer((Uint32)(delay * 1000 + 0.5), (SDL_TimerCallback)VideoCallback, this);
	av_packet_unref(&pkt);
}

int Video::DecodeAudio()
{
	AVPacket pkt;
	int ret;
	int len2, data_size = 0;

	while (true)
	{
		if (!playing)
			return -1;
		if (audio_pktqueue.GetPacket(&pkt) < 0)
			return -1;

		ret = avcodec_send_packet(audio_context, &pkt);
		if (ret<0) return ret;

		while (ret >= 0) {
			ret = avcodec_receive_frame(audio_context, audio_frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			}
			len2 = swr_convert(swr_context,
				converted_audio_frame->data,	// output
				audio_frame->nb_samples,
				(const uint8_t**)audio_frame->data,  // input
				audio_frame->nb_samples);

			// returns the number of samples per channel in one audio frame
			data_size = av_samples_get_buffer_size(NULL,
				audio_context->channels,
				audio_frame->nb_samples,
				AV_SAMPLE_FMT_FLT,
				1);

			memcpy(audio_buf, converted_audio_frame->data[0], data_size);

			double pts = audio_frame->pts;
			if (pts == AV_NOPTS_VALUE)
			{
				pts = audio_clock +
					(1.f / av_q2d(audio_stream->avg_frame_rate)) / av_q2d(audio_stream->time_base);
			}
			audio_clock = pts;

			/* We have data, return it and come back for more later */
			av_packet_unref(&pkt);
			return data_size;
		}
	}
	return -1;
}

void PacketQueue::Init()
{
	pause = false;
	mutex = SDL_CreateMutex();
	cond = SDL_CreateCond();
}

int PacketQueue::PutPacket(AVPacket* pkt)
{
	AVPacketList *pkt_list;
	AVPacket new_pkt;

	if (pause)
		return -1;

	av_packet_ref(&new_pkt, pkt);

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
	while (true)
	{
		if (pause) {
			ret = -1;
			break;
		}

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

int PacketQueue::Clear()
{
	AVPacketList *pkt, *pkt1;
	SDL_LockMutex(mutex);
	for (pkt = first_pkt; pkt != NULL; pkt = pkt1) {
		pkt1 = pkt->next;
		av_packet_unref(&(pkt->pkt));
		av_freep(&pkt);
	}
	last_pkt = NULL;
	first_pkt = NULL;
	nb_packets = 0;
	size = 0;
	SDL_UnlockMutex(mutex);

	return 0;
}
