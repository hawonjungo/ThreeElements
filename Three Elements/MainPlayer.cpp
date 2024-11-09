
#include "MainPlayer.h"
#include "Skill.h"

MainPlayer::MainPlayer() {
	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.y = 0;
	
	x_val_ = 0;
	y_val_ = 0;

	width_frame_ = 0;
	height_frame_ = 0;
	currentFrame_ = 0;
	totalFrame_ = -1;
	m_QKeyNum = 0;
	m_KeyRactive = false;
	SetFrameNum(8);
	passed_time_ = 0;
	iDelay_.resize(totalFrame_, 100);
	for (int i = 0; i < GetFrameNum(); ++i)
	{
		frame_clip_[i].x = 0;
		frame_clip_[i].y = 0;
		frame_clip_[0].w = 0;
		frame_clip_[0].h = 0;
	}

	m_KeyDown = -1;
}

MainPlayer::~MainPlayer() 
{

}




