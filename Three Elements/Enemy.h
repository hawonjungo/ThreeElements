#ifndef ENEMY_OBJECT_H_
#define ENEMY_OBJECT_H_

#include "BaseObject.h"



// Walk-cycle speed in sprite frames per second: the old code advanced one frame per rendered frame at 25 FPS.
const float ENEMY_ANIM_FPS = 25.0f;

// An enemy's sprite sheet and walk cycle. The frame data (clips, frame size, current frame) is BaseObject's own;
// where the enemy is comes from the Practice session.
class EnemyObject : public BaseObject
{
public:

	EnemyObject();
	~EnemyObject();

	bool LoadImg(std::string path, SDL_Renderer* screen, int frame_num);  // horizontal sheet of frame_num frames
	void Update(float dt);  // advances the walk cycle by real time (seconds)
	void Render(SDL_Renderer* screen);  // mirrored: the sheets face right, the enemies walk left
	void RenderFrameScaled(SDL_Renderer* screen, int x, int y, float scale);  // the same, frame drawn `scale` times larger at (x, y)
private:
	float animTime_;  // seconds accumulated towards the next frame
};


#endif // ENEMY_OBJECT_H_
