#include "render_speedtiles.h"

#include <game/mapitems.h>

CRenderLayerEntitySpeedup::CRenderLayerEntitySpeedup(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

IGraphics::CTextureHandle CRenderLayerEntitySpeedup::GetTexture() const
{
	return m_pMapImages->GetSpeedupArrow();
}

int CRenderLayerEntitySpeedup::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CSpeedupTile);
	return m_pLayerTilemap->m_Speedup;
}

void CRenderLayerEntitySpeedup::Init()
{
	UploadTileData(m_VisualTiles, 0, true);
	UploadTileData(m_VisualForce, 1, false);
	UploadTileData(m_VisualMaxSpeed, 2, false);
}

void CRenderLayerEntitySpeedup::InitTileData()
{
	m_pSpeedupTiles = CRenderLayerTile::GetData<CSpeedupTile>();
}

void CRenderLayerEntitySpeedup::Unload()
{
	CRenderLayerTile::Unload();
	if(m_VisualForce.has_value())
	{
		m_VisualForce->Unload();
		m_VisualForce = std::nullopt;
	}
	if(m_VisualMaxSpeed.has_value())
	{
		m_VisualMaxSpeed->Unload();
		m_VisualMaxSpeed = std::nullopt;
	}
}

void CRenderLayerEntitySpeedup::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pIndex = m_pSpeedupTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	unsigned char Force = m_pSpeedupTiles[y * m_pLayerTilemap->m_Width + x].m_Force;
	unsigned char MaxSpeed = m_pSpeedupTiles[y * m_pLayerTilemap->m_Width + x].m_MaxSpeed;
	*pFlags = 0;
	*pAngleRotate = m_pSpeedupTiles[y * m_pLayerTilemap->m_Width + x].m_Angle;
	if((Force == 0 && *pIndex == TILE_SPEED_BOOST_OLD) || (Force == 0 && MaxSpeed == 0 && *pIndex == TILE_SPEED_BOOST) || !IsValidSpeedupTile(*pIndex))
		*pIndex = 0;
	else if(CurOverlay == 1)
		*pIndex = Force;
	else if(CurOverlay == 2)
		*pIndex = MaxSpeed;
}

void CRenderLayerEntitySpeedup::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	// draw arrow -- clamp to the edge of the arrow image
	Graphics()->WrapClamp();
	UseTexture(GetTexture());
	RenderTileLayer(Color, Params);
	Graphics()->WrapNormal();

	if(Params.m_RenderText)
	{
		Graphics()->TextureSet(m_pMapImages->GetOverlayBottom());
		RenderTileLayer(Color, Params, &m_VisualForce.value());
		Graphics()->TextureSet(m_pMapImages->GetOverlayTop());
		RenderTileLayer(Color, Params, &m_VisualMaxSpeed.value());
	}
}

void CRenderLayerEntitySpeedup::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	int OverlayRenderFlags = (Params.m_RenderText ? OVERLAYRENDERFLAG_TEXT : 0) | (Params.m_RenderInvalidTiles ? OVERLAYRENDERFLAG_EDITOR : 0);
	RenderMap()->RenderSpeedupOverlay(m_pSpeedupTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
}
