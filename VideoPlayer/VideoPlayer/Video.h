#ifndef _VIDEO_H_
#define _VIDEO_H_

struct SDL_Texture;

class AVFormatContext;
class AVCodecContext;
class AVCodec;
class AVFrame;
class AVPacket;

#include "Module.h"

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

private:
	SDL_Texture* texture = nullptr;

	AVFormatContext* format = nullptr;
	AVCodecContext* codec_context = nullptr;
	AVCodec* codec = nullptr;
	AVFrame *frame = nullptr;
	AVPacket *pkt = nullptr;

	int video_stream = -1;
	int audio_stream = -1;
	bool pause = false;

	void DecodeVideo(AVCodecContext* context, AVFrame* frame, AVPacket *pkt);

};

#endif
