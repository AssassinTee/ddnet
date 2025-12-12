#ifndef GAME_MAP_RENDERLAYERS_RENDER_LAYER_H
#define GAME_MAP_RENDERLAYERS_RENDER_LAYER_H

#include <engine/graphics.h>

#include <game/map/envelope_manager.h>
#include <game/map/render_component.h>
#include <game/map/render_map.h>
#include <game/map/render_params.h>

#include <memory>
#include <optional>

class CMapLayers;
class CMapItemLayerTilemap;
class CMapItemLayerQuads;
class IMap;
class CMapImages;

constexpr int BorderRenderDistance = 201;

class CClipRegion
{
public:
	CClipRegion() = default;
	CClipRegion(float X, float Y, float Width, float Height) :
		m_X(X), m_Y(Y), m_Width(Width), m_Height(Height) {}

	float m_X;
	float m_Y;
	float m_Width;
	float m_Height;
};

class CRenderLayer : public CRenderComponent
{
public:
	CRenderLayer(int GroupId, int LayerId, int Flags);
	virtual void OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional);

	virtual void Init() = 0;
	virtual void Render(const CRenderLayerParams &Params) = 0;
	virtual bool DoRender(const CRenderLayerParams &Params) = 0;
	virtual bool IsValid() const { return true; }
	virtual bool IsGroup() const { return false; }
	virtual void Unload() = 0;

	bool IsVisibleInClipRegion(const std::optional<CClipRegion> &ClipRegion) const;
	int GetGroup() const { return m_GroupId; }

protected:
	int m_GroupId;
	int m_LayerId;
	int m_Flags;

	void UseTexture(IGraphics::CTextureHandle TextureHandle);
	virtual IGraphics::CTextureHandle GetTexture() const = 0;
	void RenderLoading() const;

	class IMap *m_pMap = nullptr;
	IMapImages *m_pMapImages = nullptr;
	std::shared_ptr<CEnvelopeManager> m_pEnvelopeManager;
	std::optional<FRenderUploadCallback> m_RenderUploadCallback;
	std::optional<CClipRegion> m_LayerClip;
};

#endif
