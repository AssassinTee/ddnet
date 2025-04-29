#ifndef GAME_EDITOR_MAPITEMS_LAYER_LIQUID_H
#define GAME_EDITOR_MAPITEMS_LAYER_LIQUID_H

#include "layer_tiles.h"

class CLayerLiquid : public CLayerTiles
{
public:
	CLayerLiquid(CEditor *pEditor, int w, int h);

	void Resize(int NewW, int NewH) override;
	void SetTile(int x, int y, CTile Tile) override;
	const char *TypeName() const override;
};

#endif
