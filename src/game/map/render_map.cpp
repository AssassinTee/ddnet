/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/textrender.h>

#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/map.h>

#include "render.h"

#include <game/generated/client_data.h>

#include <game/mapitems.h>
#include <game/mapitems_ex.h>

#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

int IEnvelopePointAccess::FindPointIndex(int Time) const
{
	// binary search for the interval around Time
	int Low = 0;
	int High = NumPoints() - 2;
	int FoundIndex = -1;

	while(Low <= High)
	{
		int Mid = Low + (High - Low) / 2;
		const CEnvPoint *pMid = GetPoint(Mid);
		const CEnvPoint *pNext = GetPoint(Mid + 1);
		if(Time >= pMid->m_Time && Time < pNext->m_Time)
		{
			FoundIndex = Mid;
			break;
		}
		else if(Time < pMid->m_Time)
		{
			High = Mid - 1;
		}
		else
		{
			Low = Mid + 1;
		}
	}
	return FoundIndex;
}

CMapBasedEnvelopePointAccess::CMapBasedEnvelopePointAccess(CDataFileReader *pReader)
{
	bool FoundBezierEnvelope = false;
	int EnvStart, EnvNum;
	pReader->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	for(int EnvIndex = 0; EnvIndex < EnvNum; EnvIndex++)
	{
		CMapItemEnvelope *pEnvelope = static_cast<CMapItemEnvelope *>(pReader->GetItem(EnvStart + EnvIndex));
		if(pEnvelope->m_Version >= CMapItemEnvelope::VERSION_TEEWORLDS_BEZIER)
		{
			FoundBezierEnvelope = true;
			break;
		}
	}

	if(FoundBezierEnvelope)
	{
		m_pPoints = nullptr;
		m_pPointsBezier = nullptr;

		int EnvPointStart, FakeEnvPointNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPointsBezierUpstream = static_cast<CEnvPointBezier_upstream *>(pReader->GetItem(EnvPointStart));
		else
			m_pPointsBezierUpstream = nullptr;

		m_NumPointsMax = pReader->GetItemSize(EnvPointStart) / sizeof(CEnvPointBezier_upstream);
	}
	else
	{
		int EnvPointStart, FakeEnvPointNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPoints = static_cast<CEnvPoint *>(pReader->GetItem(EnvPointStart));
		else
			m_pPoints = nullptr;

		m_NumPointsMax = pReader->GetItemSize(EnvPointStart) / sizeof(CEnvPoint);

		int EnvPointBezierStart, FakeEnvPointBezierNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS_BEZIER, &EnvPointBezierStart, &FakeEnvPointBezierNum);
		const int NumPointsBezier = pReader->GetItemSize(EnvPointBezierStart) / sizeof(CEnvPointBezier);
		if(FakeEnvPointBezierNum > 0 && m_NumPointsMax == NumPointsBezier)
			m_pPointsBezier = static_cast<CEnvPointBezier *>(pReader->GetItem(EnvPointBezierStart));
		else
			m_pPointsBezier = nullptr;

		m_pPointsBezierUpstream = nullptr;
	}

	SetPointsRange(0, m_NumPointsMax);
}

CMapBasedEnvelopePointAccess::CMapBasedEnvelopePointAccess(IMap *pMap) :
	CMapBasedEnvelopePointAccess(static_cast<CMap *>(pMap)->GetReader())
{
}

void CMapBasedEnvelopePointAccess::SetPointsRange(int StartPoint, int NumPoints)
{
	m_StartPoint = std::clamp(StartPoint, 0, m_NumPointsMax);
	m_NumPoints = std::clamp(NumPoints, 0, maximum(m_NumPointsMax - StartPoint, 0));
}

int CMapBasedEnvelopePointAccess::StartPoint() const
{
	return m_StartPoint;
}

int CMapBasedEnvelopePointAccess::NumPoints() const
{
	return m_NumPoints;
}

int CMapBasedEnvelopePointAccess::NumPointsMax() const
{
	return m_NumPointsMax;
}

const CEnvPoint *CMapBasedEnvelopePointAccess::GetPoint(int Index) const
{
	if(Index < 0 || Index >= m_NumPoints)
		return nullptr;
	if(m_pPoints != nullptr)
		return &m_pPoints[Index + m_StartPoint];
	if(m_pPointsBezierUpstream != nullptr)
		return &m_pPointsBezierUpstream[Index + m_StartPoint];
	return nullptr;
}

const CEnvPointBezier *CMapBasedEnvelopePointAccess::GetBezier(int Index) const
{
	if(Index < 0 || Index >= m_NumPoints)
		return nullptr;
	if(m_pPointsBezier != nullptr)
		return &m_pPointsBezier[Index + m_StartPoint];
	if(m_pPointsBezierUpstream != nullptr)
		return &m_pPointsBezierUpstream[Index + m_StartPoint].m_Bezier;
	return nullptr;
}

void CRenderMap::Rotate(const CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	int x = pPoint->x - pCenter->x;
	int y = pPoint->y - pCenter->y;
	pPoint->x = (int)(x * std::cos(Rotation) - y * std::sin(Rotation) + pCenter->x);
	pPoint->y = (int)(x * std::sin(Rotation) + y * std::cos(Rotation) + pCenter->y);
}

void CRenderMap::ForceRenderQuads(CQuad *pQuads, int NumQuads, int RenderFlags, ENVELOPE_EVAL pfnEval, void *pUser, float Alpha) const
{
	Graphics()->TrianglesBegin();
	float Conv = 1 / 255.0f;
	for(int i = 0; i < NumQuads; i++)
	{
		CQuad *pQuad = &pQuads[i];

		ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		pfnEval(pQuad->m_ColorEnvOffset, pQuad->m_ColorEnv, Color, 4, pUser);

		if(Color.a <= 0.0f)
			continue;

		bool Opaque = false;
		/* TODO: Analyze quadtexture
		if(a < 0.01f || (q->m_aColors[0].a < 0.01f && q->m_aColors[1].a < 0.01f && q->m_aColors[2].a < 0.01f && q->m_aColors[3].a < 0.01f))
			Opaque = true;
		*/
		if(Opaque && !(RenderFlags & LAYERRENDERFLAG_OPAQUE))
			continue;
		if(!Opaque && !(RenderFlags & LAYERRENDERFLAG_TRANSPARENT))
			continue;

		Graphics()->QuadsSetSubsetFree(
			fx2f(pQuad->m_aTexcoords[0].x), fx2f(pQuad->m_aTexcoords[0].y),
			fx2f(pQuad->m_aTexcoords[1].x), fx2f(pQuad->m_aTexcoords[1].y),
			fx2f(pQuad->m_aTexcoords[2].x), fx2f(pQuad->m_aTexcoords[2].y),
			fx2f(pQuad->m_aTexcoords[3].x), fx2f(pQuad->m_aTexcoords[3].y));

		ColorRGBA Position = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		pfnEval(pQuad->m_PosEnvOffset, pQuad->m_PosEnv, Position, 3, pUser);
		const vec2 Offset = vec2(Position.r, Position.g);
		const float Rotation = Position.b / 180.0f * pi;

		IGraphics::CColorVertex Array[4] = {
			IGraphics::CColorVertex(0, pQuad->m_aColors[0].r * Conv * Color.r, pQuad->m_aColors[0].g * Conv * Color.g, pQuad->m_aColors[0].b * Conv * Color.b, pQuad->m_aColors[0].a * Conv * Color.a * Alpha),
			IGraphics::CColorVertex(1, pQuad->m_aColors[1].r * Conv * Color.r, pQuad->m_aColors[1].g * Conv * Color.g, pQuad->m_aColors[1].b * Conv * Color.b, pQuad->m_aColors[1].a * Conv * Color.a * Alpha),
			IGraphics::CColorVertex(2, pQuad->m_aColors[2].r * Conv * Color.r, pQuad->m_aColors[2].g * Conv * Color.g, pQuad->m_aColors[2].b * Conv * Color.b, pQuad->m_aColors[2].a * Conv * Color.a * Alpha),
			IGraphics::CColorVertex(3, pQuad->m_aColors[3].r * Conv * Color.r, pQuad->m_aColors[3].g * Conv * Color.g, pQuad->m_aColors[3].b * Conv * Color.b, pQuad->m_aColors[3].a * Conv * Color.a * Alpha)};
		Graphics()->SetColorVertex(Array, 4);

		CPoint *pPoints = pQuad->m_aPoints;

		CPoint aRotated[4];
		if(Rotation != 0.0f)
		{
			for(size_t p = 0; p < std::size(aRotated); ++p)
			{
				aRotated[p] = pQuad->m_aPoints[p];
				Rotate(&pQuad->m_aPoints[4], &aRotated[p], Rotation);
			}
			pPoints = aRotated;
		}

		IGraphics::CFreeformItem Freeform(
			fx2f(pPoints[0].x) + Offset.x, fx2f(pPoints[0].y) + Offset.y,
			fx2f(pPoints[1].x) + Offset.x, fx2f(pPoints[1].y) + Offset.y,
			fx2f(pPoints[2].x) + Offset.x, fx2f(pPoints[2].y) + Offset.y,
			fx2f(pPoints[3].x) + Offset.x, fx2f(pPoints[3].y) + Offset.y);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	}
	Graphics()->TrianglesEnd();
}