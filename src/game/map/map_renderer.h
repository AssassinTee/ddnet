#ifndef GAME_MAP_MAP_RENDERER_H
#define GAME_MAP_MAP_RENDERER_H

#include <engine/map.h>
#include <game/layers.h>
#include <game/map/render_component.h>
#include <game/map/render_layer.h>

class CMapRenderer : public CRenderComponent
{
public:
	CMapRenderer() {}

	void Clear();
	void Load(ERenderType Type, CLayers *pLayers, IMapImages *pMapImages, IEnvelopeEval *pEnvelopeEval, std::optional<FRenderUploadCallback> FRenderCallbackOptional);
	void Render(const CRenderLayerParams &Params);

private:
	int GetLayerType(const CMapItemLayer *pLayer, const CLayers *pLayers) const;

	std::vector<std::unique_ptr<CRenderLayer>> m_vpRenderLayers;
};

#endif
