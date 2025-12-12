#include "render_tilelayer.h"

#include <base/color.h>

#include <engine/shared/config.h>

#include <game/mapitems.h>

#include <array>

class CTexCoords
{
public:
	std::array<uint8_t, 4> m_aTexX;
	std::array<uint8_t, 4> m_aTexY;
};

constexpr static CTexCoords CalculateTexCoords(unsigned int Flags)
{
	CTexCoords TexCoord;
	TexCoord.m_aTexX = {0, 1, 1, 0};
	TexCoord.m_aTexY = {0, 0, 1, 1};

	if(Flags & TILEFLAG_XFLIP)
		std::rotate(std::begin(TexCoord.m_aTexX), std::begin(TexCoord.m_aTexX) + 2, std::end(TexCoord.m_aTexX));

	if(Flags & TILEFLAG_YFLIP)
		std::rotate(std::begin(TexCoord.m_aTexY), std::begin(TexCoord.m_aTexY) + 2, std::end(TexCoord.m_aTexY));

	if(Flags & (TILEFLAG_ROTATE >> 1))
	{
		std::rotate(std::begin(TexCoord.m_aTexX), std::begin(TexCoord.m_aTexX) + 3, std::end(TexCoord.m_aTexX));
		std::rotate(std::begin(TexCoord.m_aTexY), std::begin(TexCoord.m_aTexY) + 3, std::end(TexCoord.m_aTexY));
	}
	return TexCoord;
}

template<std::size_t N>
constexpr static std::array<CTexCoords, N> MakeTexCoordsTable()
{
	std::array<CTexCoords, N> aTexCoords = {};
	for(std::size_t i = 0; i < N; ++i)
		aTexCoords[i] = CalculateTexCoords(i);
	return aTexCoords;
}

constexpr std::array<CTexCoords, 8> TEX_COORDS_TABLE = MakeTexCoordsTable<8>();

static void FillTmpTile(CGraphicTile *pTmpTile, CGraphicTileTextureCoords *pTmpTex, unsigned char Flags, unsigned char Index, int x, int y, const ivec2 &Offset, int Scale)
{
	if(pTmpTex)
	{
		uint8_t TableFlag = (Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP)) + ((Flags & TILEFLAG_ROTATE) >> 1);
		const auto &aTexX = TEX_COORDS_TABLE[TableFlag].m_aTexX;
		const auto &aTexY = TEX_COORDS_TABLE[TableFlag].m_aTexY;

		pTmpTex->m_TexCoordTopLeft.x = aTexX[0];
		pTmpTex->m_TexCoordTopLeft.y = aTexY[0];
		pTmpTex->m_TexCoordBottomLeft.x = aTexX[3];
		pTmpTex->m_TexCoordBottomLeft.y = aTexY[3];
		pTmpTex->m_TexCoordTopRight.x = aTexX[1];
		pTmpTex->m_TexCoordTopRight.y = aTexY[1];
		pTmpTex->m_TexCoordBottomRight.x = aTexX[2];
		pTmpTex->m_TexCoordBottomRight.y = aTexY[2];

		pTmpTex->m_TexCoordTopLeft.z = Index;
		pTmpTex->m_TexCoordBottomLeft.z = Index;
		pTmpTex->m_TexCoordTopRight.z = Index;
		pTmpTex->m_TexCoordBottomRight.z = Index;

		bool HasRotation = (Flags & TILEFLAG_ROTATE) != 0;
		pTmpTex->m_TexCoordTopLeft.w = HasRotation;
		pTmpTex->m_TexCoordBottomLeft.w = HasRotation;
		pTmpTex->m_TexCoordTopRight.w = HasRotation;
		pTmpTex->m_TexCoordBottomRight.w = HasRotation;
	}

	vec2 TopLeft(x * Scale + Offset.x, y * Scale + Offset.y);
	vec2 BottomRight(x * Scale + Scale + Offset.x, y * Scale + Scale + Offset.y);
	pTmpTile->m_TopLeft = TopLeft;
	pTmpTile->m_BottomLeft.x = TopLeft.x;
	pTmpTile->m_BottomLeft.y = BottomRight.y;
	pTmpTile->m_TopRight.x = BottomRight.x;
	pTmpTile->m_TopRight.y = TopLeft.y;
	pTmpTile->m_BottomRight = BottomRight;
}

static void FillTmpTileSpeedup(CGraphicTile *pTmpTile, CGraphicTileTextureCoords *pTmpTex, unsigned char Flags, int x, int y, const ivec2 &Offset, int Scale, short AngleRotate)
{
	int Angle = AngleRotate % 360;
	FillTmpTile(pTmpTile, pTmpTex, Angle >= 270 ? ROTATION_270 : (Angle >= 180 ? ROTATION_180 : (Angle >= 90 ? ROTATION_90 : 0)), AngleRotate % 90, x, y, Offset, Scale);
}

static bool AddTile(std::vector<CGraphicTile> &vTmpTiles, std::vector<CGraphicTileTextureCoords> &vTmpTileTexCoords, unsigned char Index, unsigned char Flags, int x, int y, bool DoTextureCoords, bool FillSpeedup = false, int AngleRotate = -1, const ivec2 &Offset = ivec2{0, 0}, int Scale = 32)
{
	if(Index <= 0)
		return false;

	vTmpTiles.emplace_back();
	CGraphicTile &Tile = vTmpTiles.back();
	CGraphicTileTextureCoords *pTileTex = nullptr;
	if(DoTextureCoords)
	{
		vTmpTileTexCoords.emplace_back();
		CGraphicTileTextureCoords &TileTex = vTmpTileTexCoords.back();
		pTileTex = &TileTex;
	}
	if(FillSpeedup)
		FillTmpTileSpeedup(&Tile, pTileTex, Flags, x, y, Offset, Scale, AngleRotate);
	else
		FillTmpTile(&Tile, pTileTex, Flags, Index, x, y, Offset, Scale);

	return true;
}

static void mem_copy_special(void *pDest, void *pSource, size_t Size, size_t Count, size_t Steps)
{
	size_t CurStep = 0;
	for(size_t i = 0; i < Count; ++i)
	{
		mem_copy(((char *)pDest) + CurStep + i * Size, ((char *)pSource) + i * Size, Size);
		CurStep += Steps;
	}
}

bool CRenderLayerTile::CTileLayerVisuals::Init(unsigned int Width, unsigned int Height)
{
	m_Width = Width;
	m_Height = Height;
	if(Width == 0 || Height == 0)
		return false;
	if constexpr(sizeof(unsigned int) >= sizeof(ptrdiff_t))
		if(Width >= std::numeric_limits<std::ptrdiff_t>::max() || Height >= std::numeric_limits<std::ptrdiff_t>::max())
			return false;

	m_vTilesOfLayer.resize((size_t)Height * (size_t)Width);

	m_vBorderTop.resize(Width);
	m_vBorderBottom.resize(Width);

	m_vBorderLeft.resize(Height);
	m_vBorderRight.resize(Height);
	return true;
}

CRenderLayerTile::CRenderLayerTile(int GroupId, int LayerId, int Flags, CMapItemLayerTilemap *pLayerTilemap) :
	CRenderLayer(GroupId, LayerId, Flags)
{
	m_pLayerTilemap = pLayerTilemap;
	m_Color = ColorRGBA(m_pLayerTilemap->m_Color.r / 255.0f, m_pLayerTilemap->m_Color.g / 255.0f, m_pLayerTilemap->m_Color.b / 255.0f, pLayerTilemap->m_Color.a / 255.0f);
	m_pTiles = nullptr;
}

void CRenderLayerTile::RenderTileLayer(const ColorRGBA &Color, const CRenderLayerParams &Params, CTileLayerVisuals *pTileLayerVisuals)
{
	CTileLayerVisuals &Visuals = pTileLayerVisuals ? *pTileLayerVisuals : m_VisualTiles.value();
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int ScreenRectY0 = std::floor(ScreenY0 / 32);
	int ScreenRectX0 = std::floor(ScreenX0 / 32);
	int ScreenRectY1 = std::ceil(ScreenY1 / 32);
	int ScreenRectX1 = std::ceil(ScreenX1 / 32);

	if(IsVisibleInClipRegion(m_LayerClip))
	{
		// create the indice buffers we want to draw -- reuse them
		std::vector<char *> vpIndexOffsets;
		std::vector<unsigned int> vDrawCounts;

		int X0 = std::max(ScreenRectX0, 0);
		int X1 = std::min(ScreenRectX1, (int)Visuals.m_Width);
		int XR = X1 - 1;
		if(X0 <= XR)
		{
			int Y0 = std::max(ScreenRectY0, 0);
			int Y1 = std::min(ScreenRectY1, (int)Visuals.m_Height);

			unsigned long long Reserve = absolute(Y1 - Y0) + 1;
			vpIndexOffsets.reserve(Reserve);
			vDrawCounts.reserve(Reserve);

			for(int y = Y0; y < Y1; ++y)
			{
				dbg_assert(Visuals.m_vTilesOfLayer[y * Visuals.m_Width + XR].IndexBufferByteOffset() >= Visuals.m_vTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset(), "Tile offsets are not monotone.");
				unsigned int NumVertices = ((Visuals.m_vTilesOfLayer[y * Visuals.m_Width + XR].IndexBufferByteOffset() - Visuals.m_vTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset()) / sizeof(unsigned int)) + (Visuals.m_vTilesOfLayer[y * Visuals.m_Width + XR].DoDraw() ? 6lu : 0lu);

				if(NumVertices)
				{
					vpIndexOffsets.push_back((offset_ptr_size)Visuals.m_vTilesOfLayer[y * Visuals.m_Width + X0].IndexBufferByteOffset());
					vDrawCounts.push_back(NumVertices);
				}
			}

			int DrawCount = vpIndexOffsets.size();
			if(DrawCount != 0)
			{
				Graphics()->RenderTileLayer(Visuals.m_BufferContainerIndex, Color, vpIndexOffsets.data(), vDrawCounts.data(), DrawCount);
			}
		}
	}

	if(Params.m_RenderTileBorder && (ScreenRectX1 > (int)Visuals.m_Width || ScreenRectY1 > (int)Visuals.m_Height || ScreenRectX0 < 0 || ScreenRectY0 < 0))
	{
		RenderTileBorder(Color, ScreenRectX0, ScreenRectY0, ScreenRectX1, ScreenRectY1, &Visuals);
	}

	if(Params.m_DebugRenderTileClips && m_LayerClip.has_value())
	{
		const CClipRegion &Clip = m_LayerClip.value();
		char aDebugText[32];
		str_format(aDebugText, sizeof(aDebugText), "Group %d LayerId %d", m_GroupId, m_LayerId);
		RenderMap()->RenderDebugClip(Clip.m_X, Clip.m_Y, Clip.m_Width, Clip.m_Height, ColorRGBA(1.0f, 0.5f, 0.0f, 1.0f), Params.m_Zoom, aDebugText);
	}
}

void CRenderLayerTile::RenderTileBorder(const ColorRGBA &Color, int BorderX0, int BorderY0, int BorderX1, int BorderY1, CTileLayerVisuals *pTileLayerVisuals)
{
	CTileLayerVisuals &Visuals = *pTileLayerVisuals;

	int Y0 = std::max(0, BorderY0);
	int X0 = std::max(0, BorderX0);
	int Y1 = std::min((int)Visuals.m_Height, BorderY1);
	int X1 = std::min((int)Visuals.m_Width, BorderX1);

	// corners
	auto DrawCorner = [&](vec2 Offset, vec2 Scale, CTileLayerVisuals::CTileVisual &Visual) {
		Offset *= 32.0f;
		Graphics()->RenderBorderTiles(Visuals.m_BufferContainerIndex, Color, (offset_ptr_size)Visual.IndexBufferByteOffset(), Offset, Scale, 1);
	};

	if(BorderX0 < 0)
	{
		// Draw corners on left side
		if(BorderY0 < 0 && Visuals.m_BorderTopLeft.DoDraw())
		{
			DrawCorner(
				vec2(0, 0),
				vec2(std::abs(BorderX0), std::abs(BorderY0)),
				Visuals.m_BorderTopLeft);
		}
		if(BorderY1 > (int)Visuals.m_Height && Visuals.m_BorderBottomLeft.DoDraw())
		{
			DrawCorner(
				vec2(0, Visuals.m_Height),
				vec2(std::abs(BorderX0), BorderY1 - Visuals.m_Height),
				Visuals.m_BorderBottomLeft);
		}
	}
	if(BorderX1 > (int)Visuals.m_Width)
	{
		// Draw corners on right side
		if(BorderY0 < 0 && Visuals.m_BorderTopRight.DoDraw())
		{
			DrawCorner(
				vec2(Visuals.m_Width, 0),
				vec2(BorderX1 - Visuals.m_Width, std::abs(BorderY0)),
				Visuals.m_BorderTopRight);
		}
		if(BorderY1 > (int)Visuals.m_Height && Visuals.m_BorderBottomRight.DoDraw())
		{
			DrawCorner(
				vec2(Visuals.m_Width, Visuals.m_Height),
				vec2(BorderX1 - Visuals.m_Width, BorderY1 - Visuals.m_Height),
				Visuals.m_BorderBottomRight);
		}
	}

	// borders
	auto DrawBorder = [&](vec2 Offset, vec2 Scale, CTileLayerVisuals::CTileVisual &StartVisual, CTileLayerVisuals::CTileVisual &EndVisual) {
		unsigned int DrawNum = ((EndVisual.IndexBufferByteOffset() - StartVisual.IndexBufferByteOffset()) / (sizeof(unsigned int) * 6)) + (EndVisual.DoDraw() ? 1lu : 0lu);
		offset_ptr_size pOffset = (offset_ptr_size)StartVisual.IndexBufferByteOffset();
		Offset *= 32.0f;
		Graphics()->RenderBorderTiles(Visuals.m_BufferContainerIndex, Color, pOffset, Offset, Scale, DrawNum);
	};

	if(Y0 < (int)Visuals.m_Height && Y1 > 0)
	{
		if(BorderX1 > (int)Visuals.m_Width)
		{
			// Draw right border
			DrawBorder(
				vec2(Visuals.m_Width, 0),
				vec2(BorderX1 - Visuals.m_Width, 1.f),
				Visuals.m_vBorderRight[Y0], Visuals.m_vBorderRight[Y1 - 1]);
		}
		if(BorderX0 < 0)
		{
			// Draw left border
			DrawBorder(
				vec2(0, 0),
				vec2(std::abs(BorderX0), 1),
				Visuals.m_vBorderLeft[Y0], Visuals.m_vBorderLeft[Y1 - 1]);
		}
	}

	if(X0 < (int)Visuals.m_Width && X1 > 0)
	{
		if(BorderY0 < 0)
		{
			// Draw top border
			DrawBorder(
				vec2(0, 0),
				vec2(1, std::abs(BorderY0)),
				Visuals.m_vBorderTop[X0], Visuals.m_vBorderTop[X1 - 1]);
		}
		if(BorderY1 > (int)Visuals.m_Height)
		{
			// Draw bottom border
			DrawBorder(
				vec2(0, Visuals.m_Height),
				vec2(1, BorderY1 - Visuals.m_Height),
				Visuals.m_vBorderBottom[X0], Visuals.m_vBorderBottom[X1 - 1]);
		}
	}
}

void CRenderLayerTile::RenderKillTileBorder(const ColorRGBA &Color)
{
	CTileLayerVisuals &Visuals = m_VisualTiles.value();
	if(Visuals.m_BufferContainerIndex == -1)
		return; // no visuals were created

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int BorderY0 = std::floor(ScreenY0 / 32);
	int BorderX0 = std::floor(ScreenX0 / 32);
	int BorderY1 = std::ceil(ScreenY1 / 32);
	int BorderX1 = std::ceil(ScreenX1 / 32);

	if(BorderX0 >= -BorderRenderDistance && BorderY0 >= -BorderRenderDistance && BorderX1 <= (int)Visuals.m_Width + BorderRenderDistance && BorderY1 <= (int)Visuals.m_Height + BorderRenderDistance)
		return;
	if(!Visuals.m_BorderKillTile.DoDraw())
		return;

	BorderX0 = std::clamp(BorderX0, -300, (int)Visuals.m_Width + 299);
	BorderY0 = std::clamp(BorderY0, -300, (int)Visuals.m_Height + 299);
	BorderX1 = std::clamp(BorderX1, -300, (int)Visuals.m_Width + 299);
	BorderY1 = std::clamp(BorderY1, -300, (int)Visuals.m_Height + 299);

	auto DrawKillBorder = [&](vec2 Offset, vec2 Scale) {
		offset_ptr_size pOffset = (offset_ptr_size)Visuals.m_BorderKillTile.IndexBufferByteOffset();
		Offset *= 32.0f;
		Graphics()->RenderBorderTiles(Visuals.m_BufferContainerIndex, Color, pOffset, Offset, Scale, 1);
	};

	// Draw left kill tile border
	if(BorderX0 < -BorderRenderDistance)
	{
		DrawKillBorder(
			vec2(BorderX0, BorderY0),
			vec2(-BorderRenderDistance - BorderX0, BorderY1 - BorderY0));
	}
	// Draw top kill tile border
	if(BorderY0 < -BorderRenderDistance)
	{
		DrawKillBorder(
			vec2(std::max(BorderX0, -BorderRenderDistance), BorderY0),
			vec2(std::min(BorderX1, (int)Visuals.m_Width + BorderRenderDistance) - std::max(BorderX0, -BorderRenderDistance), -BorderRenderDistance - BorderY0));
	}
	// Draw right kill tile border
	if(BorderX1 > (int)Visuals.m_Width + BorderRenderDistance)
	{
		DrawKillBorder(
			vec2(Visuals.m_Width + BorderRenderDistance, BorderY0),
			vec2(BorderX1 - (Visuals.m_Width + BorderRenderDistance), BorderY1 - BorderY0));
	}
	// Draw bottom kill tile border
	if(BorderY1 > (int)Visuals.m_Height + BorderRenderDistance)
	{
		DrawKillBorder(
			vec2(std::max(BorderX0, -BorderRenderDistance), Visuals.m_Height + BorderRenderDistance),
			vec2(std::min(BorderX1, (int)Visuals.m_Width + BorderRenderDistance) - std::max(BorderX0, -BorderRenderDistance), BorderY1 - (Visuals.m_Height + BorderRenderDistance)));
	}
}

ColorRGBA CRenderLayerTile::GetRenderColor(const CRenderLayerParams &Params) const
{
	ColorRGBA Color = m_Color;
	if(Params.m_EntityOverlayVal && Params.m_RenderType != ERenderType::RENDERTYPE_BACKGROUND_FORCE)
		Color.a *= (100 - Params.m_EntityOverlayVal) / 100.0f;

	ColorRGBA ColorEnv = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	m_pEnvelopeManager->EnvelopeEval()->EnvelopeEval(m_pLayerTilemap->m_ColorEnvOffset, m_pLayerTilemap->m_ColorEnv, ColorEnv, 4);
	Color = Color.Multiply(ColorEnv);
	return Color;
}

void CRenderLayerTile::Render(const CRenderLayerParams &Params)
{
	UseTexture(GetTexture());
	ColorRGBA Color = GetRenderColor(Params);
	if(Graphics()->IsTileBufferingEnabled() && Params.m_TileAndQuadBuffering)
	{
		RenderTileLayerWithTileBuffer(Color, Params);
	}
	else
	{
		RenderTileLayerNoTileBuffer(Color, Params);
	}
}

bool CRenderLayerTile::DoRender(const CRenderLayerParams &Params)
{
	// skip rendering if we render background force, but deactivated tile layer and want to render a tilelayer
	if(!g_Config.m_ClBackgroundShowTilesLayers && Params.m_RenderType == ERenderType::RENDERTYPE_BACKGROUND_FORCE)
		return false;

	// skip rendering anything but entities if we only want to render entities
	if(Params.m_EntityOverlayVal == 100 && Params.m_RenderType != ERenderType::RENDERTYPE_BACKGROUND_FORCE)
		return false;

	// skip rendering if detail layers if not wanted
	if(m_Flags & LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && Params.m_RenderType != ERenderType::RENDERTYPE_FULL_DESIGN) // detail but no details
		return false;
	return true;
}

void CRenderLayerTile::RenderTileLayerWithTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNormal();
	RenderTileLayer(Color, Params);
}

void CRenderLayerTile::RenderTileLayerNoTileBuffer(const ColorRGBA &Color, const CRenderLayerParams &Params)
{
	Graphics()->BlendNone();
	RenderMap()->RenderTilemap(m_pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_OPAQUE);
	Graphics()->BlendNormal();
	RenderMap()->RenderTilemap(m_pTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, (Params.m_RenderTileBorder ? TILERENDERFLAG_EXTEND : 0) | LAYERRENDERFLAG_TRANSPARENT);
}

void CRenderLayerTile::Init()
{
	if(m_pLayerTilemap->m_Image >= 0 && m_pLayerTilemap->m_Image < m_pMapImages->Num())
		m_TextureHandle = m_pMapImages->Get(m_pLayerTilemap->m_Image);
	else
		m_TextureHandle.Invalidate();
	UploadTileData(m_VisualTiles, 0, false);
}

void CRenderLayerTile::UploadTileData(std::optional<CTileLayerVisuals> &VisualsOptional, int CurOverlay, bool AddAsSpeedup, bool IsGameLayer)
{
	if(!Graphics()->IsTileBufferingEnabled())
		return;

	// prepare all visuals for all tile layers
	std::vector<CGraphicTile> vTmpTiles;
	std::vector<CGraphicTileTextureCoords> vTmpTileTexCoords;
	std::vector<CGraphicTile> vTmpBorderTopTiles;
	std::vector<CGraphicTileTextureCoords> vTmpBorderTopTilesTexCoords;
	std::vector<CGraphicTile> vTmpBorderLeftTiles;
	std::vector<CGraphicTileTextureCoords> vTmpBorderLeftTilesTexCoords;
	std::vector<CGraphicTile> vTmpBorderRightTiles;
	std::vector<CGraphicTileTextureCoords> vTmpBorderRightTilesTexCoords;
	std::vector<CGraphicTile> vTmpBorderBottomTiles;
	std::vector<CGraphicTileTextureCoords> vTmpBorderBottomTilesTexCoords;
	std::vector<CGraphicTile> vTmpBorderCorners;
	std::vector<CGraphicTileTextureCoords> vTmpBorderCornersTexCoords;

	const bool DoTextureCoords = GetTexture().IsValid();

	// create the visual and set it in the optional, afterwards get it
	CTileLayerVisuals v;
	v.OnInit(this);
	VisualsOptional = v;
	CTileLayerVisuals &Visuals = VisualsOptional.value();

	if(!Visuals.Init(m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height))
		return;

	Visuals.m_IsTextured = DoTextureCoords;

	if(!DoTextureCoords)
	{
		vTmpTiles.reserve((size_t)m_pLayerTilemap->m_Width * m_pLayerTilemap->m_Height);
		vTmpBorderTopTiles.reserve((size_t)m_pLayerTilemap->m_Width);
		vTmpBorderBottomTiles.reserve((size_t)m_pLayerTilemap->m_Width);
		vTmpBorderLeftTiles.reserve((size_t)m_pLayerTilemap->m_Height);
		vTmpBorderRightTiles.reserve((size_t)m_pLayerTilemap->m_Height);
		vTmpBorderCorners.reserve((size_t)4);
	}
	else
	{
		vTmpTileTexCoords.reserve((size_t)m_pLayerTilemap->m_Width * m_pLayerTilemap->m_Height);
		vTmpBorderTopTilesTexCoords.reserve((size_t)m_pLayerTilemap->m_Width);
		vTmpBorderBottomTilesTexCoords.reserve((size_t)m_pLayerTilemap->m_Width);
		vTmpBorderLeftTilesTexCoords.reserve((size_t)m_pLayerTilemap->m_Height);
		vTmpBorderRightTilesTexCoords.reserve((size_t)m_pLayerTilemap->m_Height);
		vTmpBorderCornersTexCoords.reserve((size_t)4);
	}

	int DrawLeft = m_pLayerTilemap->m_Width;
	int DrawRight = 0;
	int DrawTop = m_pLayerTilemap->m_Height;
	int DrawBottom = 0;

	int x = 0;
	int y = 0;
	for(y = 0; y < m_pLayerTilemap->m_Height; ++y)
	{
		for(x = 0; x < m_pLayerTilemap->m_Width; ++x)
		{
			unsigned char Index = 0;
			unsigned char Flags = 0;
			int AngleRotate = -1;
			GetTileData(&Index, &Flags, &AngleRotate, x, y, CurOverlay);

			// the amount of tiles handled before this tile
			int TilesHandledCount = vTmpTiles.size();
			Visuals.m_vTilesOfLayer[y * m_pLayerTilemap->m_Width + x].SetIndexBufferByteOffset((offset_ptr32)(TilesHandledCount));

			if(AddTile(vTmpTiles, vTmpTileTexCoords, Index, Flags, x, y, DoTextureCoords, AddAsSpeedup, AngleRotate))
			{
				Visuals.m_vTilesOfLayer[y * m_pLayerTilemap->m_Width + x].Draw(true);

				// calculate clip region boundaries based on draws
				DrawLeft = std::min(DrawLeft, x);
				DrawRight = std::max(DrawRight, x);
				DrawTop = std::min(DrawTop, y);
				DrawBottom = std::max(DrawBottom, y);
			}

			// do the border tiles
			if(x == 0)
			{
				if(y == 0)
				{
					Visuals.m_BorderTopLeft.SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, -32}))
						Visuals.m_BorderTopLeft.Draw(true);
				}
				else if(y == m_pLayerTilemap->m_Height - 1)
				{
					Visuals.m_BorderBottomLeft.SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, 0}))
						Visuals.m_BorderBottomLeft.Draw(true);
				}
				Visuals.m_vBorderLeft[y].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderLeftTiles.size()));
				if(AddTile(vTmpBorderLeftTiles, vTmpBorderLeftTilesTexCoords, Index, Flags, 0, y, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{-32, 0}))
					Visuals.m_vBorderLeft[y].Draw(true);
			}
			else if(x == m_pLayerTilemap->m_Width - 1)
			{
				if(y == 0)
				{
					Visuals.m_BorderTopRight.SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, -32}))
						Visuals.m_BorderTopRight.Draw(true);
				}
				else if(y == m_pLayerTilemap->m_Height - 1)
				{
					Visuals.m_BorderBottomRight.SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderCorners.size()));
					if(AddTile(vTmpBorderCorners, vTmpBorderCornersTexCoords, Index, Flags, 0, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
						Visuals.m_BorderBottomRight.Draw(true);
				}
				Visuals.m_vBorderRight[y].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderRightTiles.size()));
				if(AddTile(vTmpBorderRightTiles, vTmpBorderRightTilesTexCoords, Index, Flags, 0, y, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
					Visuals.m_vBorderRight[y].Draw(true);
			}
			if(y == 0)
			{
				Visuals.m_vBorderTop[x].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderTopTiles.size()));
				if(AddTile(vTmpBorderTopTiles, vTmpBorderTopTilesTexCoords, Index, Flags, x, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, -32}))
					Visuals.m_vBorderTop[x].Draw(true);
			}
			else if(y == m_pLayerTilemap->m_Height - 1)
			{
				Visuals.m_vBorderBottom[x].SetIndexBufferByteOffset((offset_ptr32)(vTmpBorderBottomTiles.size()));
				if(AddTile(vTmpBorderBottomTiles, vTmpBorderBottomTilesTexCoords, Index, Flags, x, 0, DoTextureCoords, AddAsSpeedup, AngleRotate, ivec2{0, 0}))
					Visuals.m_vBorderBottom[x].Draw(true);
			}
		}
	}

	// shrink clip region
	// we only apply the clip once for the first overlay type (tile visuals). Physic layers can have multiple layers for text, e.g. speedup force
	// the first overlay is always the largest and you will never find an overlay, where the text is written over AIR
	if(CurOverlay == 0)
	{
		if(DrawLeft > DrawRight || DrawTop > DrawBottom)
		{
			// we are drawing nothing, layer is empty
			m_LayerClip->m_Height = 0.0f;
			m_LayerClip->m_Width = 0.0f;
		}
		else
		{
			m_LayerClip->m_X = DrawLeft * 32.0f;
			m_LayerClip->m_Y = DrawTop * 32.0f;
			m_LayerClip->m_Width = (DrawRight - DrawLeft + 1) * 32.0f;
			m_LayerClip->m_Height = (DrawBottom - DrawTop + 1) * 32.0f;
		}
	}

	// append one kill tile to the gamelayer
	if(IsGameLayer)
	{
		Visuals.m_BorderKillTile.SetIndexBufferByteOffset((offset_ptr32)(vTmpTiles.size()));
		if(AddTile(vTmpTiles, vTmpTileTexCoords, TILE_DEATH, 0, 0, 0, DoTextureCoords))
			Visuals.m_BorderKillTile.Draw(true);
	}

	// inserts and clears tiles and tile texture coords
	auto InsertTiles = [&](std::vector<CGraphicTile> &vTiles, std::vector<CGraphicTileTextureCoords> &vTexCoords) {
		vTmpTiles.insert(vTmpTiles.end(), vTiles.begin(), vTiles.end());
		vTmpTileTexCoords.insert(vTmpTileTexCoords.end(), vTexCoords.begin(), vTexCoords.end());
		vTiles.clear();
		vTexCoords.clear();
	};

	// add the border corners, then the borders and fix their byte offsets
	int TilesHandledCount = vTmpTiles.size();
	Visuals.m_BorderTopLeft.AddIndexBufferByteOffset(TilesHandledCount);
	Visuals.m_BorderTopRight.AddIndexBufferByteOffset(TilesHandledCount);
	Visuals.m_BorderBottomLeft.AddIndexBufferByteOffset(TilesHandledCount);
	Visuals.m_BorderBottomRight.AddIndexBufferByteOffset(TilesHandledCount);

	// add the Corners to the tiles
	InsertTiles(vTmpBorderCorners, vTmpBorderCornersTexCoords);

	// now the borders
	int TilesHandledCountTop = vTmpTiles.size();
	int TilesHandledCountBottom = TilesHandledCountTop + vTmpBorderTopTiles.size();
	int TilesHandledCountLeft = TilesHandledCountBottom + vTmpBorderBottomTiles.size();
	int TilesHandledCountRight = TilesHandledCountLeft + vTmpBorderLeftTiles.size();

	if(m_pLayerTilemap->m_Width > 0 && m_pLayerTilemap->m_Height > 0)
	{
		for(int i = 0; i < std::max(m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height); ++i)
		{
			if(i < m_pLayerTilemap->m_Width)
			{
				Visuals.m_vBorderTop[i].AddIndexBufferByteOffset(TilesHandledCountTop);
				Visuals.m_vBorderBottom[i].AddIndexBufferByteOffset(TilesHandledCountBottom);
			}
			if(i < m_pLayerTilemap->m_Height)
			{
				Visuals.m_vBorderLeft[i].AddIndexBufferByteOffset(TilesHandledCountLeft);
				Visuals.m_vBorderRight[i].AddIndexBufferByteOffset(TilesHandledCountRight);
			}
		}
	}

	InsertTiles(vTmpBorderTopTiles, vTmpBorderTopTilesTexCoords);
	InsertTiles(vTmpBorderBottomTiles, vTmpBorderBottomTilesTexCoords);
	InsertTiles(vTmpBorderLeftTiles, vTmpBorderLeftTilesTexCoords);
	InsertTiles(vTmpBorderRightTiles, vTmpBorderRightTilesTexCoords);

	// setup params
	float *pTmpTiles = vTmpTiles.empty() ? nullptr : (float *)vTmpTiles.data();
	unsigned char *pTmpTileTexCoords = vTmpTileTexCoords.empty() ? nullptr : (unsigned char *)vTmpTileTexCoords.data();

	Visuals.m_BufferContainerIndex = -1;
	size_t UploadDataSize = vTmpTileTexCoords.size() * sizeof(CGraphicTileTextureCoords) + vTmpTiles.size() * sizeof(CGraphicTile);
	if(UploadDataSize > 0)
	{
		char *pUploadData = (char *)malloc(sizeof(char) * UploadDataSize);

		mem_copy_special(pUploadData, pTmpTiles, sizeof(vec2), vTmpTiles.size() * 4, (DoTextureCoords ? sizeof(ubvec4) : 0));
		if(DoTextureCoords)
		{
			mem_copy_special(pUploadData + sizeof(vec2), pTmpTileTexCoords, sizeof(ubvec4), vTmpTiles.size() * 4, sizeof(vec2));
		}

		// first create the buffer object
		int BufferObjectIndex = Graphics()->CreateBufferObject(UploadDataSize, pUploadData, 0, true);

		// then create the buffer container
		SBufferContainerInfo ContainerInfo;
		ContainerInfo.m_Stride = (DoTextureCoords ? (sizeof(float) * 2 + sizeof(ubvec4)) : 0);
		ContainerInfo.m_VertBufferBindingIndex = BufferObjectIndex;
		ContainerInfo.m_vAttributes.emplace_back();
		SBufferContainerInfo::SAttribute *pAttr = &ContainerInfo.m_vAttributes.back();
		pAttr->m_DataTypeCount = 2;
		pAttr->m_Type = GRAPHICS_TYPE_FLOAT;
		pAttr->m_Normalized = false;
		pAttr->m_pOffset = nullptr;
		pAttr->m_FuncType = 0;
		if(DoTextureCoords)
		{
			ContainerInfo.m_vAttributes.emplace_back();
			pAttr = &ContainerInfo.m_vAttributes.back();
			pAttr->m_DataTypeCount = 4;
			pAttr->m_Type = GRAPHICS_TYPE_UNSIGNED_BYTE;
			pAttr->m_Normalized = false;
			pAttr->m_pOffset = (void *)(sizeof(vec2));
			pAttr->m_FuncType = 1;
		}

		Visuals.m_BufferContainerIndex = Graphics()->CreateBufferContainer(&ContainerInfo);
		// and finally inform the backend how many indices are required
		Graphics()->IndicesNumRequiredNotify(vTmpTiles.size() * 6);
	}
	RenderLoading();
}

void CRenderLayerTile::Unload()
{
	if(m_VisualTiles.has_value())
	{
		m_VisualTiles->Unload();
		m_VisualTiles = std::nullopt;
	}
}

void CRenderLayerTile::CTileLayerVisuals::Unload()
{
	Graphics()->DeleteBufferContainer(m_BufferContainerIndex);
}

int CRenderLayerTile::GetDataIndex(unsigned int &TileSize) const
{
	TileSize = sizeof(CTile);
	return m_pLayerTilemap->m_Data;
}

void *CRenderLayerTile::GetRawData() const
{
	unsigned int TileSize;
	unsigned int DataIndex = GetDataIndex(TileSize);
	void *pTiles = m_pMap->GetData(DataIndex);
	int Size = m_pMap->GetDataSize(DataIndex);

	if(!pTiles || Size < m_pLayerTilemap->m_Width * m_pLayerTilemap->m_Height * (int)TileSize)
		return nullptr;

	return pTiles;
}

void CRenderLayerTile::OnInit(IGraphics *pGraphics, ITextRender *pTextRender, CRenderMap *pRenderMap, std::shared_ptr<CEnvelopeManager> &pEnvelopeManager, IMap *pMap, IMapImages *pMapImages, std::optional<FRenderUploadCallback> &FRenderUploadCallbackOptional)
{
	CRenderLayer::OnInit(pGraphics, pTextRender, pRenderMap, pEnvelopeManager, pMap, pMapImages, FRenderUploadCallbackOptional);
	InitTileData();
	m_LayerClip = CClipRegion(0.0f, 0.0f, m_pLayerTilemap->m_Width * 32.0f, m_pLayerTilemap->m_Height * 32.0f);
}

void CRenderLayerTile::InitTileData()
{
	m_pTiles = GetData<CTile>();
}

template<class T>
T *CRenderLayerTile::GetData() const
{
	return (T *)GetRawData();
}

void CRenderLayerTile::GetTileData(unsigned char *pIndex, unsigned char *pFlags, int *pAngleRotate, unsigned int x, unsigned int y, int CurOverlay) const
{
	*pIndex = m_pTiles[y * m_pLayerTilemap->m_Width + x].m_Index;
	*pFlags = m_pTiles[y * m_pLayerTilemap->m_Width + x].m_Flags;
}

template CSpeedupTile *CRenderLayerTile::GetData<CSpeedupTile>() const;
template CSwitchTile *CRenderLayerTile::GetData<CSwitchTile>() const;
template CTeleTile *CRenderLayerTile::GetData<CTeleTile>() const;
template CTuneTile *CRenderLayerTile::GetData<CTuneTile>() const;
