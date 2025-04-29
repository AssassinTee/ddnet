#include <game/editor/editor.h>

CLayerLiquid::CLayerLiquid(CEditor *pEditor, int w, int h) :
	CLayerTiles(pEditor, w, h)
{
	str_copy(m_aName, "Front");
	m_HasFront = true;
}

void CLayerLiquid::SetTile(int x, int y, CTile Tile)
{
	if(m_pEditor->IsAllowPlaceUnusedTiles() || IsValidLiquidTile(Tile.m_Index))
	{
		CLayerTiles::SetTile(x, y, Tile);
	}
	else
	{
		CTile air = {TILE_AIR};
		CLayerTiles::SetTile(x, y, air);
		ShowPreventUnusedTilesWarning();
	}
}

void CLayerLiquid::Resize(int NewW, int NewH)
{
	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(m_pEditor->m_Map.m_pGameLayer->m_Width != NewW || m_pEditor->m_Map.m_pGameLayer->m_Height != NewH)
		m_pEditor->m_Map.m_pGameLayer->Resize(NewW, NewH);
}

const char *CLayerLiquid::TypeName() const
{
	return "Liquids";
}
