#ifndef ENGINE_CLIENT_SPRITES_H
#define ENGINE_CLIENT_SPRITES_H

#include <base/vmath.h>

namespace client_data7 {
struct CDataSprite;
}
struct CDataSprite;

// sprite renderings
enum
{
	SPRITE_FLAG_FLIP_Y = 1,
	SPRITE_FLAG_FLIP_X = 2,
};

class CSprites
{
	class IGraphics *m_pGraphics;
	vec2 m_SpriteScale = vec2(-1.0f, -1.0f);
	void SelectSprite(const CDataSprite *pSprite, int Flags);

public:
	class IGraphics *Graphics() const { return m_pGraphics; }
	void Init(class IGraphics *pGraphics) { m_pGraphics = pGraphics; }

	void SelectSprite(int Id, int Flags = 0);
	void SelectSprite7(int Id, int Flags = 0);

	void GetSpriteScale(const CDataSprite *pSprite, float &ScaleX, float &ScaleY) const;
	void GetSpriteScale(int Id, float &ScaleX, float &ScaleY) const;
	void GetSpriteScaleImpl(int Width, int Height, float &ScaleX, float &ScaleY) const;

	void DrawSprite(float x, float y, float Size) const;
	void DrawSprite(float x, float y, float ScaledWidth, float ScaledHeight) const;

	int QuadContainerAddSprite(int QuadContainerIndex, float x, float y, float Size) const;
	int QuadContainerAddSprite(int QuadContainerIndex, float Size) const;
	int QuadContainerAddSprite(int QuadContainerIndex, float Width, float Height) const;
	int QuadContainerAddSprite(int QuadContainerIndex, float X, float Y, float Width, float Height) const;
};

#endif
