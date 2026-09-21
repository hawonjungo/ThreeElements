
#include "MainPlayer.h"



MainPlayer::MainPlayer() {
	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.y = 0;
	
	width_frame_ = 0;
	height_frame_ = 0;
	currentFrame_ = 0;
	totalFrame_ = -1;
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
}

MainPlayer::~MainPlayer() 
{

}


// SDL keyboard event -> logical Invoker action (the only place that knows SDL key codes).
bool MainPlayer::TranslateKey(const SDL_Event& e, invoker::InputAction& action) {
	if (e.type != SDL_KEYDOWN)
		return false;
	if (e.key.repeat)  // the OS auto-repeat of a held key is not a new press
		return false;

	switch (e.key.keysym.sym) {
	case SDLK_q: action = invoker::InputAction::Q; return true;
	case SDLK_w: action = invoker::InputAction::W; return true;
	case SDLK_e: action = invoker::InputAction::E; return true;
	case SDLK_r: action = invoker::InputAction::R; return true;
	case SDLK_d: action = invoker::InputAction::D; return true;
	case SDLK_f: action = invoker::InputAction::F; return true;
	default:     return false;
	}
}
