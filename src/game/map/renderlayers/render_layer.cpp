#include "render_layer.h"
#include <game/localization.h>

CRenderLayer::CRenderLayer(int GroupId, int LayerId, int Flags) :
	m_GroupId(GroupId), m_LayerId(LayerId), m_Flags(Flags) {}

void CRenderLayer::OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional)
{
	CRenderComponent::OnInit(pGraphics, pTextRender, pRenderMap);
	m_pMap = pMap;
	m_pMapImages = pMapImages;
	m_RenderUploadCallback = FRenderUploadCallbackOptional;
	m_pEnvelopeManager = pEnvelopeManager;
}

void CRenderLayer::UseTexture(IGraphics::CTextureHandle TextureHandle)
{
	if(TextureHandle.IsValid())
		Graphics()->TextureSet(TextureHandle);
	else
		Graphics()->TextureClear();
}

void CRenderLayer::RenderLoading() const
{
	const char *pLoadingTitle = Localize("Loading map");
	const char *pLoadingMessage = Localize("Uploading map data to GPU");
	if(m_RenderUploadCallback.has_value())
		(*m_RenderUploadCallback)(pLoadingTitle, pLoadingMessage, 0);
}

bool CRenderLayer::IsVisibleInClipRegion(const std::optional<CClipRegion> &ClipRegion) const
{
	// always show unclipped regions
	if(!ClipRegion.has_value())
		return true;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	float Left = ClipRegion->m_X;
	float Top = ClipRegion->m_Y;
	float Right = ClipRegion->m_X + ClipRegion->m_Width;
	float Bottom = ClipRegion->m_Y + ClipRegion->m_Height;

	return Right >= ScreenX0 && Left <= ScreenX1 && Bottom >= ScreenY0 && Top <= ScreenY1;
}
