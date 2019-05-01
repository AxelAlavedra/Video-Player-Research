#ifndef _VIDEO_H_
#define _VIDEO_H_

#include "Module.h"
class Video :
	public Module
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
};

#endif
