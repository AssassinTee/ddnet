#include "render_tele.h"

#include <game/mapitems.h>

CRenderLayerEntityTele::CRenderLayerEntityTele(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

int CRenderLayerEntityTele::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CTeleTile);
	return m_pLayerTilemap->m_Tele;
}

void CRenderLayerEntityTele::Init()
{
	UploadTileData(m_VisualTiles, 0, false);
	UploadTileData(m_VisualTeleNumbers, 1, false);
}

void CRenderLayerEntityTele::InitTileData()
{
	m_pTeleTiles = GetData<CTeleTile>();
}

void CRenderLayerEntityTele::Unload()
{
	CRenderLayerTile::Unload();
	if(m_VisualTeleNumbers.has_value())
	{
		m_VisualTeleNumbers->Unload();
		m_VisualTeleNumbers = std::nullopt;
	}
}

void CRenderLayerEntityTele::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNormal();
	RenderTileLayer(Color, Params);
	if(Params.m_RenderText)
	{
		Graphics()->TextureSet(m_pMapImages->GetOverlayCenter());
		RenderTileLayer(Color, Params, &m_VisualTeleNumbers.value());
	}
}

void CRenderLayerEntityTele::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderTelemap(m_pTeleTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	RenderMap()->RenderTelemap(m_pTeleTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
	int OverlayRenderFlags = (Params.m_RenderText ? OVERLAYRENDERFLAG_TEXT : 0) | (Params.m_RenderInvalidTiles ? OVERLAYRENDERFLAG_EDITOR : 0);
	RenderMap()->RenderTeleOverlay(m_pTeleTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
}

void CRenderLayerEntityTele::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pIndex = m_pTeleTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	*pFlags = 0;
	if(CurOverlay == 1)
	{
		if(IsTeleTileNumberUsedAny(*pIndex))
			*pIndex = m_pTeleTiles[y * m_pLayerTilemap->m_Width + x].m_Number;
		else
			*pIndex = 0;
	}
}
