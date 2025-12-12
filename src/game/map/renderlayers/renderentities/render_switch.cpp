#include "render_switch.h"

#include <game/mapitems.h>

CRenderLayerEntitySwitch::CRenderLayerEntitySwitch(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

IGraphics::CTextureHandle CRenderLayerEntitySwitch::GetTexture() const
{
	return m_pMapImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH);
}

int CRenderLayerEntitySwitch::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CSwitchTile);
	return m_pLayerTilemap->m_Switch;
}

void CRenderLayerEntitySwitch::Init()
{
	UploadTileData(m_VisualTiles, 0, false);
	UploadTileData(m_VisualSwitchNumberTop, 1, false);
	UploadTileData(m_VisualSwitchNumberBottom, 2, false);
}

void CRenderLayerEntitySwitch::InitTileData()
{
	m_pSwitchTiles = GetData<CSwitchTile>();
}

void CRenderLayerEntitySwitch::Unload()
{
	CRenderLayerTile::Unload();
	if(m_VisualSwitchNumberTop.has_value())
	{
		m_VisualSwitchNumberTop->Unload();
		m_VisualSwitchNumberTop = std::nullopt;
	}
	if(m_VisualSwitchNumberBottom.has_value())
	{
		m_VisualSwitchNumberBottom->Unload();
		m_VisualSwitchNumberBottom = std::nullopt;
	}
}

void CRenderLayerEntitySwitch::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pFlags = 0;
	*pIndex = m_pSwitchTiles[y * m_pLayerTilemap->m_Width + x].m_Type;
	if(CurOverlay == 0)
	{
		*pFlags = m_pSwitchTiles[y * m_pLayerTilemap->m_Width + x].m_Flags;
		if(*pIndex == TILE_SWITCHTIMEDOPEN)
			*pIndex = 8;
	}
	else if(CurOverlay == 1)
		*pIndex = m_pSwitchTiles[y * m_pLayerTilemap->m_Width + x].m_Number;
	else if(CurOverlay == 2)
		*pIndex = m_pSwitchTiles[y * m_pLayerTilemap->m_Width + x].m_Delay;
}

void CRenderLayerEntitySwitch::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNormal();
	RenderTileLayer(Color, Params);
	if(Params.m_RenderText)
	{
		Graphics()->TextureSet(m_pMapImages->GetOverlayTop());
		RenderTileLayer(Color, Params, &m_VisualSwitchNumberTop.value());
		Graphics()->TextureSet(m_pMapImages->GetOverlayBottom());
		RenderTileLayer(Color, Params, &m_VisualSwitchNumberBottom.value());
	}
}

void CRenderLayerEntitySwitch::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderSwitchmap(m_pSwitchTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	RenderMap()->RenderSwitchmap(m_pSwitchTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
	int OverlayRenderFlags = (Params.m_RenderText ? OVERLAYRENDERFLAG_TEXT : 0) | (Params.m_RenderInvalidTiles ? OVERLAYRENDERFLAG_EDITOR : 0);
	RenderMap()->RenderSwitchOverlay(m_pSwitchTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, OverlayRenderFlags, Color.a);
}
