#include "render_entities.h"

#include <game/mapitems.h>

CRenderLayerEntityBase::CRenderLayerEntityBase(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayerTile(GroupId, LayerId, Flags, pLayerTilemap) {}

bool CRenderLayerEntityBase::DoRender(const CRenderLayerParams &Params)
{
	// skip rendering if we render background force or full design
	if(Params.m_RenderType == ERenderType::RENDERTYPE_BACKGROUND_FORCE || Params.m_RenderType == ERenderType::RENDERTYPE_FULL_DESIGN)
		return false;

	// skip rendering of entities if don't want them
	if(!Params.m_EntityOverlayVal)
		return false;

	return true;
}

IGraphics::CTextureHandle CRenderLayerEntityBase::GetTexture() const
{
	return m_pMapImages->GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH);
}
