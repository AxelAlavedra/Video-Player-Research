#include "p2Defs.h"
#include "p2Log.h"
#include "j1App.h"
#include "Audio.h"

#include "SDL/include/SDL.h"
#include "SDL_mixer\include\SDL_mixer.h"
#pragma comment( lib, "SDL_mixer/libx86/SDL2_mixer.lib" )

Audio::Audio() : Module()
{
	music = NULL;
	name = "audio";
}

// Destructor
Audio::~Audio()
{

}

// Called before render is available
bool Audio::Awake(pugi::xml_node& config)
{
	LOG("Loading Audio Mixer");
	bool ret = true;
	SDL_Init(0);

	if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		LOG("SDL_INIT_AUDIO could not initialize! SDL_Error: %s\n", SDL_GetError());
		active = false;
		ret = true;
	}

	// load support for the JPG and PNG image formats
	int flags = MIX_INIT_OGG;
	int init = Mix_Init(flags);

	if((init & flags) != flags)
	{
		LOG("Could not initialize Mixer lib. Mix_Init: %s", Mix_GetError());
		active = false;
		ret = true;
	}

	//Initialize SDL_mixer
	if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
	{
		LOG("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
		active = false;
		ret = true;
	}

	Mix_Volume(-1, MIX_MAX_VOLUME / 2);
	Mix_VolumeMusic(20);
	
	return ret;
}

// Called before quitting
bool Audio::CleanUp()
{
	if(!active)
		return true;

	LOG("Freeing sound FX, closing Mixer and Audio subsystem");

	if(music != NULL)
	{
		Mix_FreeMusic(music);
	}

	std::map<std::string, Mix_Chunk*>::iterator item;

	for (item = fx.begin(); item != fx.end(); ++item)
	{
		Mix_FreeChunk(item->second);
	}

	fx.clear();

	Mix_CloseAudio();
	Mix_Quit();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	return true;
}

// Play a music file
bool Audio::PlayMusic(const char* path, float fade_time)
{
	bool ret = true;

	if(!active)
		return false;

	if(music != NULL)
	{
		if(fade_time > 0.0f)
		{
			Mix_FadeOutMusic(int(fade_time * 1000.0f));
		}
		else
		{
			Mix_HaltMusic();
		}

		// this call blocks until fade out is done
		Mix_FreeMusic(music);
	}

	music = Mix_LoadMUS(path);

	if(music == NULL)
	{
		LOG("Cannot load music %s. Mix_GetError(): %s\n", path, Mix_GetError());
		ret = false;
	}
	else
	{
		if(fade_time > 0.0f)
		{
			if(Mix_FadeInMusic(music, -1, (int) (fade_time * 1000.0f)) < 0)
			{
				LOG("Cannot fade in music %s. Mix_GetError(): %s", path, Mix_GetError());
				ret = false;
			}
		}
		else
		{
			if(Mix_PlayMusic(music, -1) < 0)
			{
				LOG("Cannot play in music %s. Mix_GetError(): %s", path, Mix_GetError());
				ret = false;
			}
		}
	}

	LOG("Successfully playing %s", path);
	return ret;
}

// Load WAV
const char* Audio::LoadFx(const char* path)
{
	std::string ret = " ";

	if(!active)
		return ret.c_str();	

	std::map<std::string, Mix_Chunk*>::iterator item = fx.find(path);

	if (item == fx.end())
	{
		Mix_Chunk* chunk = Mix_LoadWAV(path);

		if (chunk == NULL)
		{
			LOG("Cannot load wav %s. Mix_GetError(): %s", path, Mix_GetError());
		}
		else
		{
			fx.insert({ path,chunk });
			ret = path;
		}
	}
	else 
		ret = path;

	return ret.c_str();
}

// Play WAV
bool Audio::PlayFx(const char* id, int repeat, int channel)
{
	bool ret = false;

	if(!active)
		return false;

	std::map<std::string, Mix_Chunk*>::iterator item = fx.find(id);

	if(item != fx.end())
	{
		Mix_PlayChannel(channel, item->second, repeat);
	}

	return ret;
}

void Audio::SetFXVolume(const char * path, int volume)
{
	

	std::map<std::string, Mix_Chunk*>::iterator item = fx.find(path);

	if (item != fx.end())
	{
		Mix_VolumeChunk(item->second,volume);
	}



}

