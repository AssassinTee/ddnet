#include <engine/map.h>
#include <game/client/gameclient.h>
#include <game/editor/editor.h>
#include <game/mapitems.h>

#include "envelope_eval.h"
#include "render.h"

using namespace std::chrono_literals;


static float SolveBezier(float x, float p0, float p1, float p2, float p3)
{
	const double x3 = -p0 + 3.0 * p1 - 3.0 * p2 + p3;
	const double x2 = 3.0 * p0 - 6.0 * p1 + 3.0 * p2;
	const double x1 = -3.0 * p0 + 3.0 * p1;
	const double x0 = p0 - x;

	if(x3 == 0.0 && x2 == 0.0)
	{
		// linear
		// a * t + b = 0
		const double a = x1;
		const double b = x0;

		if(a == 0.0)
			return 0.0f;
		return -b / a;
	}
	else if(x3 == 0.0)
	{
		// quadratic
		// t * t + b * t + c = 0
		const double b = x1 / x2;
		const double c = x0 / x2;

		if(c == 0.0)
			return 0.0f;

		const double D = b * b - 4.0 * c;
		const double SqrtD = std::sqrt(D);

		const double t = (-b + SqrtD) / 2.0;

		if(0.0 <= t && t <= 1.0001)
			return t;
		return (-b - SqrtD) / 2.0;
	}
	else
	{
		// cubic
		// t * t * t + a * t * t + b * t * t + c = 0
		const double a = x2 / x3;
		const double b = x1 / x3;
		const double c = x0 / x3;

		// substitute t = y - a / 3
		const double sub = a / 3.0;

		// depressed form x^3 + px + q = 0
		// cardano's method
		const double p = b / 3.0 - a * a / 9.0;
		const double q = (2.0 * a * a * a / 27.0 - a * b / 3.0 + c) / 2.0;

		const double D = q * q + p * p * p;

		if(D > 0.0)
		{
			// only one 'real' solution
			const double s = std::sqrt(D);
			return std::cbrt(s - q) - std::cbrt(s + q) - sub;
		}
		else if(D == 0.0)
		{
			// one single, one double solution or triple solution
			const double s = std::cbrt(-q);
			const double t = 2.0 * s - sub;

			if(0.0 <= t && t <= 1.0001)
				return t;
			return (-s - sub);
		}
		else
		{
			// Casus irreducibilis ... ,_,
			const double phi = std::acos(-q / std::sqrt(-(p * p * p))) / 3.0;
			const double s = 2.0 * std::sqrt(-p);

			const double t1 = s * std::cos(phi) - sub;

			if(0.0 <= t1 && t1 <= 1.0001)
				return t1;

			const double t2 = -s * std::cos(phi + pi / 3.0) - sub;

			if(0.0 <= t2 && t2 <= 1.0001)
				return t2;
			return -s * std::cos(phi - pi / 3.0) - sub;
		}
	}
}

CEnvelopeEvalGame::CEnvelopeEvalGame(CGameClient *pGameClient, IMap *pMap) :
	m_EnvelopePointAccess(pMap)
{
	OnInterfacesInit(pGameClient);
	m_pMap = pMap;
}

void CEnvelopeEvalGame::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, bool OnlineOnly)
{
	std::shared_ptr<const CMapItemEnvelope> pItem = GetEnvelope(Env);
	if(!pItem || pItem->m_Channels <= 0)
		return;
	Channels = minimum<size_t>(Channels, pItem->m_Channels, CEnvPoint::MAX_CHANNELS);

	m_EnvelopePointAccess.SetPointsRange(pItem->m_StartPoint, pItem->m_NumPoints);
	if(m_EnvelopePointAccess.NumPoints() == 0)
		return;

	static std::chrono::nanoseconds s_Time{0};
	static auto s_LastLocalTime = time_get_nanoseconds();
	if(OnlineOnly && (pItem->m_Version < 2 || pItem->m_Synchronized))
	{
		if(GameClient()->m_Snap.m_pGameInfoObj)
		{
			// get the lerp of the current tick and prev
			const auto TickToNanoSeconds = std::chrono::nanoseconds(1s) / (int64_t)Client()->GameTickSpeed();
			const int MinTick = Client()->PrevGameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			const int CurTick = Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			s_Time = std::chrono::nanoseconds((int64_t)(mix<double>(
									    0,
									    (CurTick - MinTick),
									    (double)Client()->IntraGameTick(g_Config.m_ClDummy)) *
								    TickToNanoSeconds.count())) +
				 MinTick * TickToNanoSeconds;
		}
	}
	else
	{
		const auto CurTime = time_get_nanoseconds();
		s_Time += CurTime - s_LastLocalTime;
		s_LastLocalTime = CurTime;
	}
	CEnvelopeEvalGame::RenderEvalEnvelope(&m_EnvelopePointAccess, s_Time + std::chrono::nanoseconds(std::chrono::milliseconds(TimeOffsetMillis)), Result, Channels);
}

void CEnvelopeEvalGame::RenderEvalEnvelope(const IEnvelopePointAccess *pPoints, std::chrono::nanoseconds TimeNanos, ColorRGBA &Result, size_t Channels)
{
	const int NumPoints = pPoints->NumPoints();
	if(NumPoints == 0)
	{
		return;
	}

	if(NumPoints == 1)
	{
		const CEnvPoint *pFirstPoint = pPoints->GetPoint(0);
		for(size_t c = 0; c < Channels; c++)
		{
			Result[c] = fx2f(pFirstPoint->m_aValues[c]);
		}
		return;
	}

	const CEnvPoint *pLastPoint = pPoints->GetPoint(NumPoints - 1);
	const int64_t MaxPointTime = (int64_t)pLastPoint->m_Time * std::chrono::nanoseconds(1ms).count();
	if(MaxPointTime > 0) // TODO: remove this check when implementing a IO check for maps(in this case broken envelopes)
		TimeNanos = std::chrono::nanoseconds(TimeNanos.count() % MaxPointTime);
	else
		TimeNanos = decltype(TimeNanos)::zero();

	const double TimeMillis = TimeNanos.count() / (double)std::chrono::nanoseconds(1ms).count();

	int FoundIndex = pPoints->FindPointIndex(TimeMillis);
	if(FoundIndex == -1)
	{
		for(size_t c = 0; c < Channels; c++)
		{
			Result[c] = fx2f(pLastPoint->m_aValues[c]);
		}
		return;
	}

	const CEnvPoint *pCurrentPoint = pPoints->GetPoint(FoundIndex);
	const CEnvPoint *pNextPoint = pPoints->GetPoint(FoundIndex + 1);

	const float Delta = pNextPoint->m_Time - pCurrentPoint->m_Time;
	float a = (float)(TimeMillis - pCurrentPoint->m_Time) / Delta;

	switch(pCurrentPoint->m_Curvetype)
	{
	case CURVETYPE_STEP:
		a = 0.0f;
		break;

	case CURVETYPE_SLOW:
		a = a * a * a;
		break;

	case CURVETYPE_FAST:
		a = 1.0f - a;
		a = 1.0f - a * a * a;
		break;

	case CURVETYPE_SMOOTH:
		a = -2.0f * a * a * a + 3.0f * a * a; // second hermite basis
		break;

	case CURVETYPE_BEZIER:
	{
		const CEnvPointBezier *pCurrentPointBezier = pPoints->GetBezier(FoundIndex);
		const CEnvPointBezier *pNextPointBezier = pPoints->GetBezier(FoundIndex + 1);
		if(pCurrentPointBezier == nullptr || pNextPointBezier == nullptr)
			break; // fallback to linear
		for(size_t c = 0; c < Channels; c++)
		{
			// monotonic 2d cubic bezier curve
			const vec2 p0 = vec2(pCurrentPoint->m_Time, fx2f(pCurrentPoint->m_aValues[c]));
			const vec2 p3 = vec2(pNextPoint->m_Time, fx2f(pNextPoint->m_aValues[c]));

			const vec2 OutTang = vec2(pCurrentPointBezier->m_aOutTangentDeltaX[c], fx2f(pCurrentPointBezier->m_aOutTangentDeltaY[c]));
			const vec2 InTang = vec2(pNextPointBezier->m_aInTangentDeltaX[c], fx2f(pNextPointBezier->m_aInTangentDeltaY[c]));

			vec2 p1 = p0 + OutTang;
			vec2 p2 = p3 + InTang;

			// validate bezier curve
			p1.x = std::clamp(p1.x, p0.x, p3.x);
			p2.x = std::clamp(p2.x, p0.x, p3.x);

			// solve x(a) = time for a
			a = std::clamp(SolveBezier(TimeMillis, p0.x, p1.x, p2.x, p3.x), 0.0f, 1.0f);

			// value = y(t)
			Result[c] = bezier(p0.y, p1.y, p2.y, p3.y, a);
		}
		return;
	}

	case CURVETYPE_LINEAR: [[fallthrough]];
	default:
		break;
	}

	for(size_t c = 0; c < Channels; c++)
	{
		const float v0 = fx2f(pCurrentPoint->m_aValues[c]);
		const float v1 = fx2f(pNextPoint->m_aValues[c]);
		Result[c] = v0 + (v1 - v0) * a;
	}
}

std::shared_ptr<const CMapItemEnvelope> CEnvelopeEvalGame::GetEnvelope(int Env)
{
	int EnvStart, EnvNum;
	m_pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	if(Env < 0 || Env >= EnvNum)
		return nullptr;
	const CMapItemEnvelope *pItem = (CMapItemEnvelope *)m_pMap->GetItem(EnvStart + Env);

	// specify no-op deleter as this memory is not handled by the pointer
	return std::shared_ptr<const CMapItemEnvelope>(pItem, [](const CMapItemEnvelope *) {});
}

CEnvelopeEvalEditor::CEnvelopeEvalEditor(CEditor *pEditor)
{
	m_pEditor = pEditor;
}

void CEnvelopeEvalEditor::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, bool OnlineOnly)
{
	if(Env < 0 || Env >= (int)m_pEditor->m_Map.m_vpEnvelopes.size())
		return;

	std::shared_ptr<CEnvelope> pEnv = m_pEditor->m_Map.m_vpEnvelopes[Env];
	float Time = m_pEditor->m_AnimateTime;
	Time *= m_pEditor->m_AnimateSpeed;
	Time += (TimeOffsetMillis / 1000.0f);
	pEnv->Eval(Time, Result, Channels);
}

const IEnvelopePointAccess &CEnvelopeEvalEditor::PointAccess(int Env) const
{
	std::shared_ptr<CEnvelope> pEnv = m_pEditor->m_Map.m_vpEnvelopes[Env];
	return pEnv->PointAccess();
}

std::shared_ptr<const CMapItemEnvelope> CEnvelopeEvalEditor::GetEnvelope(int Env)
{
	if(Env < 0 || Env >= (int)m_pEditor->m_Map.m_vpEnvelopes.size())
		return nullptr;

	std::shared_ptr<CMapItemEnvelope> Item = std::make_shared<CMapItemEnvelope>();
	Item->m_Version = 2;
	Item->m_Channels = m_pEditor->m_Map.m_vpEnvelopes[Env]->GetChannels();
	Item->m_StartPoint = 0;
	Item->m_NumPoints = m_pEditor->m_Map.m_vpEnvelopes[Env]->m_vPoints.size();
	Item->m_Synchronized = m_pEditor->m_Map.m_vpEnvelopes[Env]->m_Synchronized;
	StrToInts(Item->m_aName, std::size(Item->m_aName), m_pEditor->m_Map.m_vpEnvelopes[Env]->m_aName);
	return Item;
}
