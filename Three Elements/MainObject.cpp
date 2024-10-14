
#include "MainObject.h"


MainObject::MainObject() {
	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.y = 0;
	
	x_val_ = 0;
	y_val_ = 0;

	width_frame_ = 0;
	height_frame_ = 0;
	frame_ = -1;
	m_QKeyNum = 0;
	m_KeyRactive = false;

	for (int i = 0; i < FRAME_NUM; ++i)
	{
		frame_clip_[i].x = 0;
		frame_clip_[i].y = 0;
		frame_clip_[0].w = 0;
		frame_clip_[0].h = 0;
	}

	m_KeyDown = -1;
}

MainObject::~MainObject() 
{

}

void MainObject::keyHandle(SDL_Event eventKey)
{
	printf("init KEy /n");
	unsigned int pointQ = 0;
	unsigned int pointW = 0;
	unsigned int pointE = 0;
	printf("while loop KEyboard------/n");

	switch (eventKey.type)
	{
	case SDL_KEYDOWN:
	{
		if (eventKey.key.keysym.sym == 'q')
		{
			m_KeyDown = KEY_Q;
			if (m_QKeyNum < 3)
			{
				m_QKeyNum++;
			}
		}
		else if (eventKey.key.keysym.sym == 'w')
		{
			m_KeyDown = KEY_W;
			if (m_WKeyNum < 3)
			{
				m_WKeyNum++;
			}
		}
		else if (eventKey.key.keysym.sym == 'e')
		{
			m_KeyDown = KEY_E;
			if (m_WKeyNum < 3)
			{
				m_WKeyNum++;
			}
		}
		else if (eventKey.key.keysym.sym == 'r')
		{
			m_KeyRactive = true;
		}
		else if (eventKey.key.keysym.sym == 'd')
		{
			// cho nay bam phim d se thuc hien giet quai vat
			//1. tu vi tri nay, lam sao tim ra dc con quai can giet
			//2. sau do dua m_active cua skill ve = false.
			
		}
	}
	break;
	case SDL_KEYUP:
	{
		m_KeyDown = KEY_NONE;		
	}
	break;
	default:
		break;
	}
}


bool MainObject::LoadImg(std::string path, SDL_Renderer* screen)
{
	bool ret = BaseObject::LoadImg(path, screen);
	if (ret == true)
	{
		width_frame_ = rect_.w / FRAME_NUM;
		height_frame_ = rect_.h;
	}

	return ret;
}

void MainObject::set_clips()
{
    if (width_frame_ > 0 && height_frame_ > 0)
    {
        for (int i = 0; i < FRAME_NUM; ++i)
        {
            frame_clip_[i].x = i*width_frame_;
            frame_clip_[i].y = 0;
            frame_clip_[i].w = width_frame_;
            frame_clip_[i].h = height_frame_;
        }
    }
}

void MainObject::Render(SDL_Renderer* screen)
{
	frame_++;
	if (frame_ == FRAME_NUM)
	{
		frame_ = 0;
	}

	SDL_Rect* current_clip = &frame_clip_[frame_];
	SDL_Rect renderQuad = { rect_.x, rect_.y, width_frame_, height_frame_ };
	SDL_RenderCopy(screen, p_object_, current_clip, &renderQuad);
}
