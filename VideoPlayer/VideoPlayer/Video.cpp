extern "C" {
#include "ffmpeg/include/libavcodec/avcodec.h"
#include "ffmpeg/include/libavdevice/avdevice.h"
#include "ffmpeg/include/libavfilter/avfilter.h"
#include "ffmpeg/include/libavformat/avformat.h"
#include "ffmpeg/include/libavutil/avutil.h"

#pragma comment( lib, "ffmpeg/lib/avcodec.lib" )
#pragma comment( lib, "ffmpeg/lib/avdevice.lib" )
#pragma comment( lib, "ffmpeg/lib/avfilter.lib" )
#pragma comment( lib, "ffmpeg/lib/avformat.lib" )
#pragma comment( lib, "ffmpeg/lib/avutil.lib" )
}

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
	AVDictionary* dictionary = NULL;

	avformat_open_input(&format, "videos/test_video.mp4", NULL, NULL);

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
	return true;
}

bool Video::CleanUp()
{
	return true;
}
