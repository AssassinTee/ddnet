#include "render_tune.h"

#include <game/mapitems.h>

CRenderLayerEntityTune::CRenderLayerEntityTune(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

void CRenderLayerEntityTune::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pIndex = m_pTuneTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	*pFlags = 0;
}

int CRenderLayerEntityTune::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CTuneTile);
	return m_pLayerTilemap->m_Tune;
}

void CRenderLayerEntityTune::InitTileData()
{
	m_pTuneTiles = CRenderLayerTile::GetData<CTuneTile>();
}

void CRenderLayerEntityTune::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderTunemap(m_pTuneTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	RenderMap()->RenderTunemap(m_pTuneTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
}
