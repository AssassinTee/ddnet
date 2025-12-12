#ifndef GAME_MAP_RENDERLAYERS_RENDER_GROUP_H
#define GAME_MAP_RENDERLAYERS_RENDER_GROUP_H

#include "render_layer.h"

class CRenderLayerGroup : public CRenderLayer
{
public:
	CRenderLayerGroup(int GroupId, CMapItemGroup *pGroup);
	~CRenderLayerGroup() override = default;
	void Init() override {}
	void Render(const CRenderLayerParams &Params) override;
	bool DoRender(const CRenderLayerParams &Params) override;
	bool IsValid() const override { return m_pGroup != nullptr; }
	bool IsGroup() const override { return true; }
	void Unload() override {}

protected:
	IGraphics::CTextureHandle GetTexture() const override { return IGraphics::CTextureHandle(); }

	CMapItemGroup *m_pGroup;
};
#endif
