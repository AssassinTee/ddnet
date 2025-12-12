#include "render_front.h"

#include <game/mapitems.h>

CRenderLayerEntityFront::CRenderLayerEntityFront(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerEntityBase(GroupId, LayerId, Flags, pLayerTilemap) {}

int CRenderLayerEntityFront::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CTile);
	return m_pLayerTilemap->m_Front;
}
