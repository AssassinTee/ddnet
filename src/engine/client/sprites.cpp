#include "sprites.h"
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/generated/client_data7.h>
#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>

void CSprites::SelectSprite(const CDataSprite *pSprite, int Flags)
{
	int x = pSprite->m_X;
	int y = pSprite->m_Y;
	int w = pSprite->m_W;
	int h = pSprite->m_H;
	int cx = pSprite->m_pSet->m_Gridx;
	int cy = pSprite->m_pSet->m_Gridy;

	GetSpriteScaleImpl(w, h, m_SpriteScale.x, m_SpriteScale.y);

	float x1 = x / (float)cx + 0.5f / (float)(cx * 32);
	float x2 = (x + w) / (float)cx - 0.5f / (float)(cx * 32);
	float y1 = y / (float)cy + 0.5f / (float)(cy * 32);
	float y2 = (y + h) / (float)cy - 0.5f / (float)(cy * 32);

	if(Flags & SPRITE_FLAG_FLIP_Y)
		std::swap(y1, y2);

	if(Flags & SPRITE_FLAG_FLIP_X)
		std::swap(x1, x2);

	Graphics()->QuadsSetSubset(x1, y1, x2, y2);
}

void CSprites::SelectSprite(int Id, int Flags)
{
	dbg_assert(Id >= 0 && Id < g_pData->m_NumSprites, "Id invalid");
	SelectSprite(&g_pData->m_aSprites[Id], Flags);
}

void CSprites::SelectSprite7(int Id, int Flags)
{
	dbg_assert(Id >= 0 && Id < client_data7::g_pData->m_NumSprites, "Id invalid");
	SelectSprite(&client_data7::g_pData->m_aSprites[Id], Flags);
}

void CSprites::GetSpriteScale(const CDataSprite *pSprite, float &ScaleX, float &ScaleY) const
{
	int w = pSprite->m_W;
	int h = pSprite->m_H;
	GetSpriteScaleImpl(w, h, ScaleX, ScaleY);
}

void CSprites::GetSpriteScale(int Id, float &ScaleX, float &ScaleY) const
{
	GetSpriteScale(&g_pData->m_aSprites[Id], ScaleX, ScaleY);
}

void CSprites::GetSpriteScaleImpl(int Width, int Height, float &ScaleX, float &ScaleY) const
{
	const float f = length(vec2(Width, Height));
	ScaleX = Width / f;
	ScaleY = Height / f;
}

void CSprites::DrawSprite(float x, float y, float Size) const
{
	IGraphics::CQuadItem QuadItem(x, y, Size * m_SpriteScale.x, Size * m_SpriteScale.y);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CSprites::DrawSprite(float x, float y, float ScaledWidth, float ScaledHeight) const
{
	IGraphics::CQuadItem QuadItem(x, y, ScaledWidth, ScaledHeight);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

int CSprites::QuadContainerAddSprite(int QuadContainerIndex, float x, float y, float Size) const
{
	IGraphics::CQuadItem QuadItem(x, y, Size, Size);
	return Graphics()->QuadContainerAddQuads(QuadContainerIndex, &QuadItem, 1);
}

int CSprites::QuadContainerAddSprite(int QuadContainerIndex, float Size) const
{
	IGraphics::CQuadItem QuadItem(-(Size) / 2.f, -(Size) / 2.f, (Size), (Size));
	return Graphics()->QuadContainerAddQuads(QuadContainerIndex, &QuadItem, 1);
}

int CSprites::QuadContainerAddSprite(int QuadContainerIndex, float Width, float Height) const
{
	IGraphics::CQuadItem QuadItem(-(Width) / 2.f, -(Height) / 2.f, (Width), (Height));
	return Graphics()->QuadContainerAddQuads(QuadContainerIndex, &QuadItem, 1);
}

int CSprites::QuadContainerAddSprite(int QuadContainerIndex, float X, float Y, float Width, float Height) const
{
	IGraphics::CQuadItem QuadItem(X, Y, Width, Height);
	return Graphics()->QuadContainerAddQuads(QuadContainerIndex, &QuadItem, 1);
}
