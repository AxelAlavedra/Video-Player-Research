#ifndef _VIDEO_H_
#define _VIDEO_H_

struct SDL_Texture;
class AVFormatContext;
struct AVStream;
class AVFrame;
class AVCodecContext;
class AVPacket;
struct SwsContext;
struct SwrContext;
struct AVPacketList;

#include "Module.h"
#include "Timer.h"

struct PacketQueue {
	AVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	SDL_mutex* mutex = nullptr;
	SDL_cond* cond = nullptr;

	void Init();
	int PutPacket(AVPacket* pkt);
	int GetPacket(AVPacket* pkt);
	int Clear();
	bool pause = false;
};

class Video : public Module
{
public:
	Video();
	~Video();
	bool Awake(pugi::xml_node&);
	bool Start();
	bool PreUpdate();
	bool Update(float dt);
	bool PostUpdate();
	bool CleanUp();


	int PlayVideo(std::string file_path);

	void DecodeVideo();
	int DecodeAudio();
	void OpenStream(int stream_index);
	bool Pause();

private:
	SDL_Texture* texture = nullptr;

	AVStream* video_stream = nullptr;
	AVStream* audio_stream = nullptr;
	AVCodecContext* video_context = nullptr;
	AVCodecContext* audio_context = nullptr;
	SwsContext* sws_context = nullptr;
	SwrContext* swr_context = nullptr;

	AVFrame* video_frame = nullptr;
	AVFrame* video_scaled_frame = nullptr;

	AVFrame* audio_frame = nullptr;
	AVFrame* converted_audio_frame = nullptr;

	SDL_Thread* parse_thread_id;

	double audio_clock;
	SDL_mutex* texture_mutex;
	SDL_cond* texture_cond;

	void CleanVideo();

public:
	AVFormatContext * format = nullptr;
	bool pause = false;
	bool playing = false;
	bool quit = false;
	std::string file = "";

	int video_stream_index = -1;
	int audio_stream_index = -1;

	PacketQueue audio_pktqueue;
	PacketQueue video_pktqueue;

	//audio stream stuff
	uint8_t audio_buf[(192000 * 3) / 2]; //TODO move this to a struct or class
	unsigned int audio_buf_size = 0; //TODO move this to a struct or class
	unsigned int audio_buf_index = 0; //TODO move this to a struct or class
};

#endif
