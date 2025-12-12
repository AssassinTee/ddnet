#ifndef GAME_MAP_RENDERLAYERS_RENDERENTITIES_RENDER_TELE_H
#define GAME_MAP_RENDERLAYERS_RENDERENTITIES_RENDER_TELE_H

#include <game/map/renderlayers/render_entities.h>

class CRenderLayerEntityTele final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityTele(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
	void Init() override;
	void InitTileData() override;
	void Unload() override;

protected:
	void RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;

private:
	std::optional<CRenderLayerTile::CTileLayerVisuals> m_VisualTeleNumbers;
	CTeleTile *m_pTeleTiles;
};

#endif
