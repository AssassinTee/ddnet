#ifndef GAME_MAP_RENDERLAYERS_RENDER_QUADS_H
#define GAME_MAP_RENDERLAYERS_RENDER_QUADS_H

#include "render_layer.h"

#include <optional>
#include <vector>

class CRenderLayerQuads : public CRenderLayer
{
public:
	CRenderLayerQuads(int GroupId, int LayerId, int Flags, CMapItemLayerQuads *pLayerQuads);
	void OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional) override;
	void Init() override;
	bool IsValid() const override { return m_pLayerQuads->m_NumQuads > 0 && m_pQuads; }
	void Render(const CRenderLayerParams &Params) override;
	bool DoRender(const CRenderLayerParams &Params) override;
	void Unload() override;

protected:
	IGraphics::CTextureHandle GetTexture() const override { return m_TextureHandle; }

	class CQuadLayerVisuals : public CRenderComponent
	{
	public:
		CQuadLayerVisuals() :
			m_QuadNum(0), m_BufferContainerIndex(-1), m_IsTextured(false) {}
		void Unload();

		int m_QuadNum;
		int m_BufferContainerIndex;
		bool m_IsTextured;
	};
	void RenderQuadLayer(float Alpha, const CRenderLayerParams &Params);

	std::optional<CRenderLayerQuads::CQuadLayerVisuals> m_VisualQuad;
	CMapItemLayerQuads *m_pLayerQuads;

	class CQuadCluster
	{
	public:
		bool m_Grouped;
		int m_StartIndex;
		int m_NumQuads;

		int m_PosEnv;
		float m_PosEnvOffset;
		int m_ColorEnv;
		float m_ColorEnvOffset;

		std::vector<SQuadRenderInfo> m_vQuadRenderInfo;
		std::optional<CClipRegion> m_ClipRegion;
	};
	void CalculateClipping(CQuadCluster &QuadCluster);
	bool CalculateQuadClipping(const CQuadCluster &QuadCluster, float aQuadOffsetMin[2], float aQuadOffsetMax[2]) const;

	std::vector<CQuadCluster> m_vQuadClusters;
	CQuad *m_pQuads;

private:
	IGraphics::CTextureHandle m_TextureHandle;
};

#endif
