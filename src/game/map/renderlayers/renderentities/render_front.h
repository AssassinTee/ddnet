#ifndef GAME_MAP_RENDERLAYERS_RENDERENTITIES_RENDER_FRONT_H
#define GAME_MAP_RENDERLAYERS_RENDERENTITIES_RENDER_FRONT_H

#include <game/map/renderlayers/render_entities.h>

class CRenderLayerEntityFront final : public CRenderLayerEntityBase
{
public:
	CRenderLayerEntityFront(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	int GetDataIndex(unsigned int &TileSize) const override;
};

#endif
