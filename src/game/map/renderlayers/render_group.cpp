#include "render_group.h"

#include <engine/shared/config.h>

#include <game/mapitems.h>

CRenderLayerGroup::CRenderLayerGroup(int GroupId, CMapItemGroup *pGroup) :
	CRenderLayer(GroupId, 0, 0), m_pGroup(pGroup) {}

bool CRenderLayerGroup::DoRender(const CRenderLayerParams &Params)
{
	if(!g_Config.m_GfxNoclip || Params.m_RenderType == ERenderType::RENDERTYPE_FULL_DESIGN)
	{
		Graphics()->ClipDisable();
		if(m_pGroup->m_Version >= 2 && m_pGroup->m_UseClipping)
		{
			// set clipping
			Graphics()->MapScreenToInterface(Params.m_Center.x, Params.m_Center.y, Params.m_Zoom);

			float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
			Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
			float ScreenWidth = ScreenX1 - ScreenX0;
			float ScreenHeight = ScreenY1 - ScreenY0;
			float Left = m_pGroup->m_ClipX - ScreenX0;
			float Top = m_pGroup->m_ClipY - ScreenY0;
			float Right = m_pGroup->m_ClipX + m_pGroup->m_ClipW - ScreenX0;
			float Bottom = m_pGroup->m_ClipY + m_pGroup->m_ClipH - ScreenY0;

			if(Right < 0.0f || Left > ScreenWidth || Bottom < 0.0f || Top > ScreenHeight)
				return false;

			// Render debug before enabling the clip
			if(Params.m_DebugRenderGroupClips)
			{
				char aDebugText[32];
				str_format(aDebugText, sizeof(aDebugText), "Group %d", m_GroupId);
				RenderMap()->RenderDebugClip(m_pGroup->m_ClipX, m_pGroup->m_ClipY, m_pGroup->m_ClipW, m_pGroup->m_ClipH, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), Params.m_Zoom, aDebugText);
			}

			int ClipX = (int)std::round(Left * Graphics()->ScreenWidth() / ScreenWidth);
			int ClipY = (int)std::round(Top * Graphics()->ScreenHeight() / ScreenHeight);

			Graphics()->ClipEnable(
				ClipX,
				ClipY,
				(int)std::round(Right * Graphics()->ScreenWidth() / ScreenWidth) - ClipX,
				(int)std::round(Bottom * Graphics()->ScreenHeight() / ScreenHeight) - ClipY);
		}
	}
	return true;
}

void CRenderLayerGroup::Render(const CRenderLayerParams &Params)
{
	int ParallaxZoom = std::clamp((maximum(m_pGroup->m_ParallaxX, m_pGroup->m_ParallaxY)), 0, 100);
	float aPoints[4];
	Graphics()->MapScreenToWorld(Params.m_Center.x, Params.m_Center.y, m_pGroup->m_ParallaxX, m_pGroup->m_ParallaxY, (float)ParallaxZoom,
		m_pGroup->m_OffsetX, m_pGroup->m_OffsetY, Graphics()->ScreenAspect(), Params.m_Zoom, aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}
