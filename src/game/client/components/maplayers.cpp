/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/client/gameclient.h>
#include <game/localization.h>

#include "maplayers.h"

#include <chrono>

using namespace std::chrono_literals;

const int LAYER_DEFAULT_TILESET = -1;

void CMapLayers::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, IMap *pMap, CMapBasedEnvelopePointAccess *pEnvelopePoints, IClient *pClient, CGameClient *pGameClient, bool OnlineOnly)
{
	int EnvStart, EnvNum;
	pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	if(Env < 0 || Env >= EnvNum)
		return;

	const CMapItemEnvelope *pItem = (CMapItemEnvelope *)pMap->GetItem(EnvStart + Env);
	if(pItem->m_Channels <= 0)
		return;
	Channels = minimum<size_t>(Channels, pItem->m_Channels, CEnvPoint::MAX_CHANNELS);

	pEnvelopePoints->SetPointsRange(pItem->m_StartPoint, pItem->m_NumPoints);
	if(pEnvelopePoints->NumPoints() == 0)
		return;

	static std::chrono::nanoseconds s_Time{0};
	static auto s_LastLocalTime = time_get_nanoseconds();
	if(OnlineOnly && (pItem->m_Version < 2 || pItem->m_Synchronized))
	{
		if(pGameClient->m_Snap.m_pGameInfoObj)
		{
			// get the lerp of the current tick and prev
			const auto TickToNanoSeconds = std::chrono::nanoseconds(1s) / (int64_t)pClient->GameTickSpeed();
			const int MinTick = pClient->PrevGameTick(g_Config.m_ClDummy) - pGameClient->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			const int CurTick = pClient->GameTick(g_Config.m_ClDummy) - pGameClient->m_Snap.m_pGameInfoObj->m_RoundStartTick;
			s_Time = std::chrono::nanoseconds((int64_t)(mix<double>(
									    0,
									    (CurTick - MinTick),
									    (double)pClient->IntraGameTick(g_Config.m_ClDummy)) *
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
	CRenderTools::RenderEvalEnvelope(pEnvelopePoints, s_Time + std::chrono::nanoseconds(std::chrono::milliseconds(TimeOffsetMillis)), Result, Channels);
}

void CMapLayers::EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels)
{
	EnvelopeEval(TimeOffsetMillis, Env, Result, Channels, this->m_pLayers->Map(), this->m_pEnvelopePoints.get(), this->Client(), this->GameClient(), this->m_OnlineOnly);
}

CMapLayers::CMapLayers(int Type, bool OnlineOnly)
{
	m_Type = Type;
	m_OnlineOnly = OnlineOnly;

	// static parameters for ingame rendering
	m_Params.m_RenderType = m_Type;
	m_Params.m_RenderInvalidTiles = false;
	m_Params.m_TileAndQuadBuffering = true;
	m_Params.m_RenderTileBorder = true;
}

void CMapLayers::OnInit()
{
	m_pLayers = Layers();
	m_pImages = &GameClient()->m_MapImages;
}

CCamera *CMapLayers::GetCurCamera()
{
	return &GameClient()->m_Camera;
}

void CMapLayers::OnMapLoad()
{
	m_pEnvelopePoints = std::make_shared<CMapBasedEnvelopePointAccess>(m_pLayers->Map());
	bool PassedGameLayer = false;
	m_vRenderLayers.clear();

	const char *pLoadingTitle = Localize("Loading map");
	const char *pLoadingMessage = Localize("Uploading map data to GPU");
	GameClient()->m_Menus.RenderLoading(pLoadingTitle, pLoadingMessage, 0);

	for(int g = 0; g < m_pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = m_pLayers->GetGroup(g);
		std::unique_ptr<CRenderLayer> pRenderLayerGroup = std::make_unique<CRenderLayerGroup>(g, pGroup);
		pRenderLayerGroup->OnInit(GameClient(), m_pLayers->Map(), m_pImages, m_pEnvelopePoints, m_OnlineOnly);
		if(!pRenderLayerGroup->IsValid())
		{
			dbg_msg("maplayers", "error group was null, group number = %d, total groups = %d", g, m_pLayers->NumGroups());
			dbg_msg("maplayers", "this is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			dbg_msg("maplayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
		}

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->GetLayer(pGroup->m_StartLayer + l);
			int LayerType = GetLayerType(pLayer);
			PassedGameLayer |= LayerType == LAYER_GAME;

			if(m_Type == TYPE_BACKGROUND_FORCE || m_Type == TYPE_BACKGROUND)
			{
				if(PassedGameLayer)
					return;
			}
			else if(m_Type == TYPE_FOREGROUND)
			{
				if(!PassedGameLayer)
					continue;
			}

			if(pRenderLayerGroup)
				m_vRenderLayers.push_back(std::move(pRenderLayerGroup));

			std::unique_ptr<CRenderLayer> pRenderLayer;

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTileLayer = (CMapItemLayerTilemap *)pLayer;

				switch(LayerType)
				{
				case LAYER_DEFAULT_TILESET:
					pRenderLayer = std::make_unique<CRenderLayerTile>(
						g, l,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_GAME:
					pRenderLayer = std::make_unique<CRenderLayerEntityGame>(
						g, l,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_FRONT:
					pRenderLayer = std::make_unique<CRenderLayerEntityFront>(
						g, l,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_TELE:
					pRenderLayer = std::make_unique<CRenderLayerEntityTele>(
						g, l,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_SPEEDUP:
					pRenderLayer = std::make_unique<CRenderLayerEntitySpeedup>(
						g, l,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_SWITCH:
					pRenderLayer = std::make_unique<CRenderLayerEntitySwitch>(
						g, l,
						pLayer->m_Flags,
						pTileLayer);
					break;
				case LAYER_TUNE:
					pRenderLayer = std::make_unique<CRenderLayerEntityTune>(
						g, l,
						pLayer->m_Flags,
						pTileLayer);
					break;
				default:
					dbg_assert(false, "Unknown LayerType %d", LayerType);
					break;
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;

				IGraphics::CTextureHandle TextureHandle;
				if(pQLayer->m_Image >= 0 && pQLayer->m_Image < m_pImages->Num())
					TextureHandle = m_pImages->Get(pQLayer->m_Image);
				else
					TextureHandle.Invalidate();

				pRenderLayer = std::make_unique<CRenderLayerQuads>(
					g, l,
					TextureHandle,
					pLayer->m_Flags,
					pQLayer);
			}

			// just ignore invalid layers from rendering
			if(pRenderLayer)
			{
				pRenderLayer->OnInit(GameClient(), m_pLayers->Map(), m_pImages, m_pEnvelopePoints, m_OnlineOnly);
				if(pRenderLayer->IsValid())
				{
					pRenderLayer->Init();
					m_vRenderLayers.push_back(std::move(pRenderLayer));
				}
			}
		}
	}
}

void CMapLayers::OnRender()
{
	if(m_OnlineOnly && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);

	// dynamic parameters for ingame rendering
	m_Params.m_EntityOverlayVal = m_Type == TYPE_FULL_DESIGN ? 0 : g_Config.m_ClOverlayEntities;
	m_Params.m_Center = GetCurCamera()->m_Center;
	m_Params.m_Zoom = GetCurCamera()->m_Zoom;
	m_Params.m_RenderText = g_Config.m_ClTextEntities;

	bool DoRenderGroup = true;
	for(auto &&pRenderLayer : m_vRenderLayers)
	{
		if(pRenderLayer->IsGroup())
			DoRenderGroup = pRenderLayer->DoRender(m_Params);

		if(!DoRenderGroup)
			continue;

		if(pRenderLayer->DoRender(m_Params))
			pRenderLayer->Render(m_Params);
	}

	// Reset clip from last group
	Graphics()->ClipDisable();

	// don't reset screen on background
	if(m_Type != TYPE_BACKGROUND && m_Type != TYPE_BACKGROUND_FORCE)
	{
		// reset the screen like it was before
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
	}
	else
	{
		// reset the screen to the default interface
		RenderTools()->MapScreenToInterface(m_Params.m_Center.x, m_Params.m_Center.y, m_Params.m_Zoom);
	}
}

int CMapLayers::GetLayerType(const CMapItemLayer *pLayer) const
{
	if(pLayer == (CMapItemLayer *)m_pLayers->GameLayer())
		return LAYER_GAME;
	else if(pLayer == (CMapItemLayer *)m_pLayers->FrontLayer())
		return LAYER_FRONT;
	else if(pLayer == (CMapItemLayer *)m_pLayers->SwitchLayer())
		return LAYER_SWITCH;
	else if(pLayer == (CMapItemLayer *)m_pLayers->TeleLayer())
		return LAYER_TELE;
	else if(pLayer == (CMapItemLayer *)m_pLayers->SpeedupLayer())
		return LAYER_SPEEDUP;
	else if(pLayer == (CMapItemLayer *)m_pLayers->TuneLayer())
		return LAYER_TUNE;
	return LAYER_DEFAULT_TILESET;
}
