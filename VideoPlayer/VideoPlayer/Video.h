#ifndef _VIDEO_H_
#define _VIDEO_H_

struct SDL_Texture;
class AVCodecContext;
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
	void Decode(AVCodecContext* context, AVFrame* frame, AVPacket *pkt);
	void DisplayFrame(AVFrame* frame, AVCodecContext* context);
};

#endif
