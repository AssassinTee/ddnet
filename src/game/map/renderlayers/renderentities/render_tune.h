#ifndef GAME_MAP_RENDERLAYERS_RENDERENTITIES_RENDER_TUNE_H
#define GAME_MAP_RENDERLAYERS_RENDERENTITIES_RENDER_TUNE_H

#include <game/map/renderlayers/render_entities.h>

class CRenderLayerEntityTune final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityTune(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
	void InitTileData() override;

protected:
	void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params) override;
	void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const override;

private:
	CTuneTile *m_pTuneTiles;
};

#endif
