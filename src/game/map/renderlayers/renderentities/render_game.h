#ifndef GAME_MAP_RENDERLAYERS_RENDERENTITIES_RENDER_GAME_H
#define GAME_MAP_RENDERLAYERS_RENDERENTITIES_RENDER_GAME_H

#include <game/map/renderlayers/render_entities.h>

class CRenderLayerEntityGame final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityGame(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	void Init() override;

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;

private:
	ColorRGBA GetDeathBorderColor() const;
};

#endif
