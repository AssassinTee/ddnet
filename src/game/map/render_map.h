#ifndef GAME_MAP_RENDER_MAP_H
#define GAME_MAP_RENDER_MAP_H

#include <base/color.h>
#include <game/mapitems.h>

#include <chrono>

enum
{
	LAYERRENDERFLAG_OPAQUE = 1,
	LAYERRENDERFLAG_TRANSPARENT = 2,

	TILERENDERFLAG_EXTEND = 4,

	OVERLAYRENDERFLAG_TEXT = 1,
	OVERLAYRENDERFLAG_EDITOR = 2,
};

class IEnvelopePointAccess
{
public:
	virtual ~IEnvelopePointAccess() = default;
	virtual int NumPoints() const = 0;
	virtual const CEnvPoint *GetPoint(int Index) const = 0;
	virtual const CEnvPointBezier *GetBezier(int Index) const = 0;
	int FindPointIndex(int Time) const;
};

class CMapBasedEnvelopePointAccess : public IEnvelopePointAccess
{
	int m_StartPoint;
	int m_NumPoints;
	int m_NumPointsMax;
	CEnvPoint *m_pPoints;
	CEnvPointBezier *m_pPointsBezier;
	CEnvPointBezier_upstream *m_pPointsBezierUpstream;

public:
	CMapBasedEnvelopePointAccess(class CDataFileReader *pReader);
	CMapBasedEnvelopePointAccess(class IMap *pMap);
	void SetPointsRange(int StartPoint, int NumPoints);
	int StartPoint() const;
	int NumPoints() const override;
	int NumPointsMax() const;
	const CEnvPoint *GetPoint(int Index) const override;
	const CEnvPointBezier *GetBezier(int Index) const override;
};

typedef void (*ENVELOPE_EVAL)(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, void *pUser);

class CRenderMap
{
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;
	class CSprites *m_pSprites;

public:
	void Init(class IGraphics *pGraphics, class ITextRender *pTextRender, class CSprites *pSprites);
	class IGraphics *Graphics() const { return m_pGraphics; }
	class ITextRender *TextRender() const { return m_pTextRender; }
	class CSprites *Sprites() const { return m_pSprites; }

	// map render methods (render_map.cpp)
	static void RenderEvalEnvelope(const IEnvelopePointAccess *pPoints, std::chrono::nanoseconds TimeNanos, ColorRGBA &Result, size_t Channels);
	void RenderQuads(CQuad *pQuads, int NumQuads, int Flags, ENVELOPE_EVAL pfnEval, void *pUser) const;
	void ForceRenderQuads(CQuad *pQuads, int NumQuads, int Flags, ENVELOPE_EVAL pfnEval, void *pUser, float Alpha = 1.0f) const;
	void RenderTilemap(CTile *pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags) const;

	// render a rectangle made of IndexIn tiles, over a background made of IndexOut tiles
	// the rectangle include all tiles in [RectX, RectX+RectW-1] x [RectY, RectY+RectH-1]
	void RenderTileRectangle(int RectX, int RectY, int RectW, int RectH, unsigned char IndexIn, unsigned char IndexOut, float Scale, ColorRGBA Color, int RenderFlags) const;

	void RenderTile(int x, int y, unsigned char Index, float Scale, ColorRGBA Color) const;

	// helpers
	void CalcScreenParams(float Aspect, float Zoom, float *pWidth, float *pHeight);
	void MapScreenToWorld(float CenterX, float CenterY, float ParallaxX, float ParallaxY,
		float ParallaxZoom, float OffsetX, float OffsetY, float Aspect, float Zoom, float *pPoints);
	void MapScreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup, float Zoom);
	void MapScreenToInterface(float CenterX, float CenterY, float Zoom = 1.0f);

	// DDRace
	void RenderTeleOverlay(CTeleTile *pTele, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f) const;
	void RenderSpeedupOverlay(CSpeedupTile *pSpeedup, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f) const;
	void RenderSwitchOverlay(CSwitchTile *pSwitch, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f) const;
	void RenderTuneOverlay(CTuneTile *pTune, int w, int h, float Scale, int OverlayRenderFlags, float Alpha = 1.0f) const;
	void RenderTelemap(CTeleTile *pTele, int w, int h, float Scale, ColorRGBA Color, int RenderFlags) const;
	void RenderSwitchmap(CSwitchTile *pSwitch, int w, int h, float Scale, ColorRGBA Color, int RenderFlags) const;
	void RenderTunemap(CTuneTile *pTune, int w, int h, float Scale, ColorRGBA Color, int RenderFlags) const;
};

#endif
