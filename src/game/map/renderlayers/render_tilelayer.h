#ifndef GAME_MAP_RENDERLAYERS_RENDER_TILELAYER_H
#define GAME_MAP_RENDERLAYERS_RENDER_TILELAYER_H

#include "render_layer.h"

#include <cstdint>
#include <optional>

using offset_ptr_size = char *;
using offset_ptr = uintptr_t;
using offset_ptr32 = unsigned int;

class CRenderLayerTile : public CRenderLayer
{
public:
	CRenderLayerTile(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap);
	~CRenderLayerTile() override = default;
	void Render(const CRenderLayerParams &Params) override;
	bool DoRender(const CRenderLayerParams &Params) override;
	void Init() override;
	void OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional) override;

	virtual int GetDataIndex(unsigned int &TileSize) const;
	bool IsValid() const override { return GetRawData() != nullptr; }
	void Unload() override;

protected:
	virtual void *GetRawData() const;
	template<class T>
	T *GetData() const;

	virtual ColorRGBA GetRenderColor(const CRenderLayerParams &Params) const;
	virtual void InitTileData();
	virtual void GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const;
	IGraphics::CTextureHandle GetTexture() const override { return m_TextureHandle; }
	CTile *m_pTiles;

private:
	IGraphics::CTextureHandle m_TextureHandle;

protected:
	class CTileLayerVisuals : public CRenderComponent
	{
	public:
		CTileLayerVisuals()
		{
			m_Width = 0;
			m_Height = 0;
			m_BufferContainerIndex = -1;
			m_IsTextured = false;
		}

		bool Init(unsigned int Width, unsigned int Height);
		void Unload();

		class CTileVisual
		{
		public:
			CTileVisual() :
				m_IndexBufferByteOffset(0) {}

		private:
			offset_ptr32 m_IndexBufferByteOffset;

		public:
			bool DoDraw() const
			{
				return (m_IndexBufferByteOffset & 0x10000000) != 0;
			}

			void Draw(bool SetDraw)
			{
				m_IndexBufferByteOffset = (SetDraw ? 0x10000000 : (offset_ptr32)0) | (m_IndexBufferByteOffset & 0xEFFFFFFF);
			}

			offset_ptr IndexBufferByteOffset() const
			{
				return ((offset_ptr)(m_IndexBufferByteOffset & 0xEFFFFFFF) * 6 * sizeof(uint32_t));
			}

			void SetIndexBufferByteOffset(offset_ptr32 IndexBufferByteOff)
			{
				m_IndexBufferByteOffset = IndexBufferByteOff | (m_IndexBufferByteOffset & 0x10000000);
			}

			void AddIndexBufferByteOffset(offset_ptr32 IndexBufferByteOff)
			{
				m_IndexBufferByteOffset = ((m_IndexBufferByteOffset & 0xEFFFFFFF) + IndexBufferByteOff) | (m_IndexBufferByteOffset & 0x10000000);
			}
		};

		std::vector<CTileVisual> m_vTilesOfLayer;

		CTileVisual m_BorderTopLeft;
		CTileVisual m_BorderTopRight;
		CTileVisual m_BorderBottomRight;
		CTileVisual m_BorderBottomLeft;

		CTileVisual m_BorderKillTile; // end of map kill tile -- game layer only

		std::vector<CTileVisual> m_vBorderTop;
		std::vector<CTileVisual> m_vBorderLeft;
		std::vector<CTileVisual> m_vBorderRight;
		std::vector<CTileVisual> m_vBorderBottom;

		unsigned int m_Width;
		unsigned int m_Height;
		int m_BufferContainerIndex;
		bool m_IsTextured;
	};

	void UploadTileData(std::optional<CTileLayerVisuals> &VisualsOptional, int CurOverlay, bool AddAsSpeedup, bool IsGameLayer = false);

	virtual void RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params);
	virtual void RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params);

	void RenderTileLayer(const ColorRGBA &Color, const CRenderLayerParams &Params, CTileLayerVisuals *pTileLayerVisuals = nullptr);
	void RenderTileBorder(const ColorRGBA &Color, int BorderX0, int BorderY0, int BorderX1, int BorderY1, CTileLayerVisuals *pTileLayerVisuals);
	void RenderKillTileBorder(const ColorRGBA &Color);

	std::optional<CRenderLayerTile::CTileLayerVisuals> m_VisualTiles;
	CMapItemLayerTilemap *m_pLayerTilemap;
	ColorRGBA m_Color;
};

#endif
