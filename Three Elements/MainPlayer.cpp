
#include "MainPlayer.h"
#include "Skill.h"
#include <algorithm>

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
	m_KeyRActive = false;
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


void MainPlayer::handleKeyPress(SDL_Event key) {
	if (key.type == SDL_KEYDOWN) {
		switch (key.key.keysym.sym) {
		case SDLK_q:
			elements.push_back(QUAS);			
			printf("=====Q===== "); break;
		case SDLK_w:
			elements.push_back(WEX);			
			printf("====W===== "); break;
		case SDLK_e: 
			elements.push_back(EXORT);
			printf("====E====== "); break;
		case SDLK_r:
			getCombineComb();		
			m_KeyRActive = true;
			return; 
			// Additional keys (D, F) might be handled separately if needed
		}
		if (elements.size() > 3) {
			elements.erase(elements.begin());
		}
	}
}



string MainPlayer::getElementComb() {
	string combo;

	for (Element elem : elements) {
		switch (elem) {
		case QUAS: combo += 'Q'; break;
		case WEX: combo += 'W'; break;
		case EXORT: combo += 'E'; break;
		}
	}	
	return combo;
}


string MainPlayer::getCombineComb() {
	string combo = getElementComb();
	sort(combo.begin(), combo.end());
	if (skill.spellMap.find(combo) != skill.spellMap.end()) {
		activeSpell = skill.spellMap[combo];
		saveSpellToSlot(combo);
		printf("Current combination: %s\n", combo.c_str());
	}
	else {
		activeSpell = NO_SPELL;
	}
	return combo;
}
// good thinking
void MainPlayer::saveSpellToSlot(string combo) {
			
	if (!slotD.empty() && slotD != slotF && slotD != combo) {
			slotF = slotD;
	
	}
		slotD = combo;
		
		printf("Slot D: %s, Slot F: %s\n", slotD.c_str(), slotF.c_str());
}