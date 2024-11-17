
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
	m_KeyRactive = false;
	SetFrameNum(8);
	initializeSpellMap();
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


void MainPlayer::initializeSpellMap() {
	spellMap["QQQ"] = COLD_SNAP;
	spellMap["QQW"] = GHOST_WALK;
	spellMap["EQQ"] = ICE_WALL;
	spellMap["WWW"] = EMP;
	spellMap["QWW"] = TORNADO;
	spellMap["EWW"] = ALACRITY;
	spellMap["EEE"] = SUN_STRIKE;
	spellMap["EEQ"] = FORGE_SPIRIT;
	spellMap["EEW"] = CHAOS_METEOR;
	spellMap["EQW"] = DEAFENING_BLAST;
}




void MainPlayer::handleKeyPress(SDL_Event key) {
	if (key.type == SDL_KEYDOWN) {
		switch (key.key.keysym.sym) {
		case SDLK_q:
			elements.push_back(QUAS);
			m_KeyDown = Keyboard::KeyType::KEY_Q;
			printf("=====Q===== "); break;
		case SDLK_w:
			elements.push_back(WEX);
			m_KeyDown = Keyboard::KeyType::KEY_W;
			printf("====W===== "); break;
		case SDLK_e: 
			elements.push_back(EXORT);
			m_KeyDown = Keyboard::KeyType::KEY_E;	
			printf("====E====== "); break;
		case SDLK_r:
			m_KeyDown = Keyboard::KeyType::KEY_R;
			combine();			
			return; 
			// Additional keys (D, F) might be handled separately if needed
		}

		//skill_.incrementElementIndex(); // Loop over 0, 1, 2		
		if (elements.size() > 3) {
			elements.erase(elements.begin());
		}
	}
}



string MainPlayer::getElementCombination() {
	string combo;

	for (Element elem : elements) {
		switch (elem) {
		case QUAS: combo += 'Q'; break;
		case WEX: combo += 'W'; break;
		case EXORT: combo += 'E'; break;
		}
	}

	sort(combo.begin(), combo.end());
	return combo;
}


void MainPlayer::combine() {
	string combo = getElementCombination();
	if (spellMap.find(combo) != spellMap.end()) {
		activeSpell = spellMap[combo];
		saveSpellToSlot();
		printf("Current combination: %s\n", combo.c_str());
	}
	else {
		activeSpell = NO_SPELL;
	}
}

void MainPlayer::saveSpellToSlot() {
	slotF = slotD;
	slotD = activeSpell;
}