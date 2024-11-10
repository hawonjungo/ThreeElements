#include "Skill.h"
#include <algorithm>

// MON 10/14/2024 First Setup
Skill::Skill() : activeSpell(NO_SPELL), slotD(NO_SPELL), slotF(NO_SPELL), nextElementIndex(0)
{
	// MON 10/14/2024 First Setup
	elements[0] = QUAS;
	elements[1] = WEX;
	elements[2] = EXORT;
	initializeSpellMap();

	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.y = 0;

	x_val_ = 0;
	y_val_ = 0;

	width_frame_ = 0;
	height_frame_ = 0;
	currentFrame_ = -1;

	for (int i = 0; i < FRAME_NUM; ++i)
	{
		frame_clip_[i].x = 0;
		frame_clip_[i].y = 0;
		frame_clip_[0].w = 0;
		frame_clip_[0].h = 0;
	}
	m_type = 0;
	m_active = false;
}

Skill::~Skill()
{

}

void Skill::keyHandle(SDL_Event event)
{
}
// <Start First Setup>========================

void Skill::initializeSpellMap() {
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
void Skill::setElement(Element element) {
	elements[nextElementIndex] = element; 
}
void Skill::resetElementIndex() {
	nextElementIndex = 0;
}
void Skill::incrementElementIndex() {
	nextElementIndex = (nextElementIndex + 1) % 3; // Loop over 0, 1, 2
}
std::string Skill::getElementCombination() {
	std::string combo;
	for (Element elem : elements) {
		switch (elem) {
		case QUAS: combo += 'Q'; break;
		case WEX: combo += 'W'; break;
		case EXORT: combo += 'E'; break;
		}
	}
	std::sort(combo.begin(), combo.end());
	return combo;
}

void Skill::combine() {
	std::string combo = getElementCombination();
	if (spellMap.find(combo) != spellMap.end()) {
		activeSpell = spellMap[combo];
		saveSpellToSlot();		
		printf("Current combination: %s\n", combo.c_str());
	}
	else {
		activeSpell = NO_SPELL;
	}
}

void Skill::saveSpellToSlot() {
	slotF = slotD;
	slotD = activeSpell;
}
//void Skill::handleKeyPress(SDL_Event key) {
//	if (key.type == SDL_KEYDOWN) {
//		switch (key.key.keysym.sym) {
//		case SDLK_q: elements[nextElementIndex] = QUAS; printf("=====Q===== "); break;
//		case SDLK_w: elements[nextElementIndex] = WEX; printf("====W===== "); break;
//		case SDLK_e: elements[nextElementIndex] = EXORT; printf("====E====== "); break;
//		case SDLK_r: combine(); nextElementIndex = 0; return; // Reset index after combining
//			// Additional keys (D, F) might be handled separately if needed
//		}
//		nextElementIndex = (nextElementIndex + 1) % 3; // Loop over 0, 1, 2
//	}
//}

//=========================== <End First Setup>==================================
bool Skill::LoadImg(std::string path, SDL_Renderer* screen)
{
	bool ret = BaseObject::LoadImg(path, screen);
	if (ret == true)
	{
		width_frame_ = rect_.w / FRAME_NUM;
		height_frame_ = rect_.h;
	}

	return ret;
}

void Skill::set_clips()
{
	if (width_frame_ > 0 && height_frame_ > 0)
	{
		for (int i = 0; i < FRAME_NUM; ++i)
		{
			frame_clip_[i].x = i * width_frame_;
			frame_clip_[i].y = 0;
			frame_clip_[i].w = width_frame_;
			frame_clip_[i].h = height_frame_;
		}
	}
}

void Skill::Render(SDL_Renderer* screen)
{
	currentFrame_++;
	if (currentFrame_ == FRAME_NUM)
	{
		currentFrame_ = 0;
	}

	SDL_Rect* current_clip = &frame_clip_[currentFrame_];
	SDL_Rect renderQuad = { rect_.x, rect_.y, width_frame_, height_frame_ };

	//SDL_RenderCopyEx(screen, p_object_, current_clip, &renderQuad, 0, 0, SDL_FLIP_NONE);
	SDL_RenderCopy(screen, p_object_, current_clip, &renderQuad);

}
