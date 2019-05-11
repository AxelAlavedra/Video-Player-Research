#ifndef _VIDEO_H_
#define _VIDEO_H_

struct SDL_Texture;
class AVFormatContext;
class AVCodecContext;
class AVCodec;
class AVFrame;
class AVPacket;
struct SwsContext;
struct SwrContext;
struct AVPacketList;

#include "Module.h"

struct PacketQueue {
	AVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	SDL_mutex* mutex;
	SDL_cond* cond;

	void Init();
	int PutPacket(AVPacket* pkt);
	int GetPacket(AVPacket* pkt);
};

class Video : public Module
{
public:
	Video();
	~Video();
	bool Awake();
	bool Start();
	bool PreUpdate();
	bool Update(float dt);
	bool PostUpdate();
	bool CleanUp();


	void DecodeVideo();
	int DecodeAudio();
	void OpenStream(int stream_index);

private:
	SDL_Texture* texture = nullptr;

	AVCodecContext* video_codec_context = nullptr;
	AVCodecContext* audio_codec_context = nullptr;
	AVCodec* video_codec = nullptr;
	AVCodec* audio_codec = nullptr;
	SwsContext* sws_context = nullptr;
	SwrContext* swr_context = nullptr;

	AVFrame* video_frame = nullptr;
	AVFrame* video_scaled_frame = nullptr;

	AVFrame* audio_frame = nullptr;
	AVFrame* converted_audio_frame = nullptr;

	AVPacket* audio_pkt = nullptr;

	SDL_Thread* parse_thread_id;
	SDL_Thread* video_thread_id;

	int frame_amount = 0;
	int frame_ratio = 0;

public:
	AVFormatContext * format = nullptr;
	bool pause = false;
	bool quit = false;
	std::string file = "";
	int video_stream = -1;
	int audio_stream = -1;

	PacketQueue audio_pktqueue;
	PacketQueue video_pktqueue;

	//audio stream stuff
	uint8_t audio_buf[(192000 * 3) / 2]; //TODO move this to a struct or class
	unsigned int audio_buf_size = 0; //TODO move this to a struct or class
	unsigned int audio_buf_index = 0; //TODO move this to a struct or class
};

#endif
