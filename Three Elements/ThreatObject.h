#pragma once

#ifndef THREAT_OBJECT_H_
#define THREAT_OBJECT_H_

#include "GameObject.h"
#include "BaseObject.h"

#define THREAT_fRAME_NUM 8

class ThreatObject :public BaseObject {
public:
	ThreatObject();
	~ThreatObject();



private:

	int map_x_;
	int map_y_; // limit map for enemy

	float x_val_;
	float y_val_;
	float x_pos_;
	float y_pos_;

	bool comeback_time;
	SDL_Rect frame_clip_[THREAT_fRAME_NUM];
	int width_frame_;
	int height_frame_;
	int frame_;
};

#endif