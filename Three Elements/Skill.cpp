#include "Skill.h"

// MON 10/14/2024 First Setup
Skill::Skill() {



	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.y = 0;

	width_frame_ = 0;
	height_frame_ = 0;
	SetFrameNum(1);
	for (int i = 0; i < GetFrameNum(); ++i)
	{
		frame_clip_[i].x = 0;
		frame_clip_[i].y = 0;
		frame_clip_[0].w = 0;
		frame_clip_[0].h = 0;
	}
}

Skill::~Skill()
{

}
