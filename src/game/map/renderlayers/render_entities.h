#ifndef GAME_MAP_RENDERLAYERS_RENDER_ENTITIES_H
#define GAME_MAP_RENDERLAYERS_RENDER_ENTITIES_H

#include "render_tilelayer.h"

class ColorRGBA;

class CRenderLayerEntityBase : public CRenderLayerTile
{
public:
	CRenderLayerEntityBase(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	~CRenderLayerEntityBase() override = default;
	bool DoRender(const CRenderLayerParams &Params) override;

protected:
	ColorRGBA GetRenderColor(const CRenderLayerParams &Params) const override { return ColorRGBA(1.0f, 1.0f, 1.0f, Params.m_EntityOverlayVal / 100.0f); }
	IGraphics::CTextureHandle GetTexture() const override;
};

#endif
