#ifndef GAME_MAP_RENDER_PARAMS_H
#define GAME_MAP_RENDER_PARAMS_H

#include <base/vmath.h>

#include <functional>

typedef std::function<void(const char *pCaption, const char *pContent, int IncreaseCounter)> FRenderUploadCallback;

class CRenderLayerParams
{
public:
	int m_RenderType;
	int m_EntityOverlayVal;
	vec2 m_Center;
	float m_Zoom;
	bool m_RenderText;
	bool m_RenderInvalidTiles;
	bool m_TileAndQuadBuffering;
	bool m_RenderTileBorder;
	bool m_DebugRenderGroupClips;
	bool m_DebugRenderQuadClips;
	bool m_DebugRenderClusterClips;
	bool m_DebugRenderTileClips;
};

#endif
