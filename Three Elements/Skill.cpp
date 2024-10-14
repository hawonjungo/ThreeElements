#include "Skill.h"
#include <algorithm>

// MON 10/14/2024 First Setup
Skill::Skill() : activeSpell(NO_SPELL), slotD(NO_SPELL), slotF(NO_SPELL)
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
	frame_ = -1;

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
	spellMap["QQE"] = ICE_WALL;
	spellMap["WWW"] = EMP;
	spellMap["WWQ"] = TORNADO;
	spellMap["WWE"] = ALACRITY;
	spellMap["EEE"] = SUN_STRIKE;
	spellMap["EEQ"] = FORGE_SPIRIT;
	spellMap["WEQ"] = CHAOS_METEOR;
	spellMap["QWE"] = DEAFENING_BLAST;
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
	}
	else {
		activeSpell = NO_SPELL;
	}
}

void Skill::saveSpellToSlot() {
	slotF = slotD;
	slotD = activeSpell;
}

void Skill::handleKeyPress(SDL_Event key) {
	if (key.type == SDL_KEYDOWN) {
		switch (key.key.keysym.sym) {
		case SDLK_q: elements[0] = QUAS; break;
		case SDLK_w: elements[1] = WEX; break;
		case SDLK_e: elements[2] = EXORT; break;
		case SDLK_r: combine(); break;
			// Additional keys (D, F) might be handled separately if needed
		}
	}
}
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
	frame_++;
	if (frame_ == FRAME_NUM)
	{
		frame_ = 0;
	}

	SDL_Rect* current_clip = &frame_clip_[frame_];
	SDL_Rect renderQuad = { rect_.x, rect_.y, width_frame_, height_frame_ };

	//SDL_RenderCopyEx(screen, p_object_, current_clip, &renderQuad, 0, 0, SDL_FLIP_NONE);
	SDL_RenderCopy(screen, p_object_, current_clip, &renderQuad);

}
