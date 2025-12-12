#include "render_game.h"

#include <game/mapitems.h>

CRenderLayerEntityGame::CRenderLayerEntityGame(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

void CRenderLayerEntityGame::Init()
{
	UploadTileData(m_VisualTiles, 0, false, true);
}

void CRenderLayerEntityGame::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNormal();
	if(Params.m_RenderTileBorder)
		RenderKillTileBorder(Color.Multiply(GetDeathBorderColor()));
	RenderTileLayer(Color, Params);
}

void CRenderLayerEntityGame::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderTilemap(m_pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();

	if(Params.m_RenderTileBorder)
	{
		RenderMap()->RenderTileRectangle(-BorderRenderDistance, -BorderRenderDistance, m_pLayerTilemap->m_Width + 2 * BorderRenderDistance, m_pLayerTilemap->m_Height + 2 * BorderRenderDistance,
			TILE_AIR, TILE_DEATH, // display air inside, death outside
			32.0f, Color.Multiply(GetDeathBorderColor()), TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
	}

	RenderMap()->RenderTilemap(m_pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
}

ColorRGBA CRenderLayerEntityGame::GetDeathBorderColor() const
{
	// draw kill tiles outside the entity clipping rectangle
	// slow blinking to hint that it's not a part of the map
	float Seconds = time_get() / (float)time_freq();
	float Alpha = 0.3f + 0.35f * (1.f + std::sin(2.f * pi * Seconds / 3.f));
	return ColorRGBA(1.f, 1.f, 1.f, Alpha);
}
