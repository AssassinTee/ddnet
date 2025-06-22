/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include "player.h"

#include <engine/server.h>
#include "gamecontext.h"
#include <game/gamecore.h>
#include <game/version.h>
#include <game/server/teams.h>
#include "gamemodes/DDRace.h"
#include <time.h>


MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, int Team)
{
	m_pGameServer = pGameServer;
	m_ClientID = ClientID;
	m_Team = GameServer()->m_pController->ClampTeam(Team);
	m_pCharacter = 0;
	m_NumInputs = 0;
	m_KillMe = 0;
	Reset();
}

CPlayer::~CPlayer()
{
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Reset()
{
	m_DieTick = Server()->Tick();
	m_JoinTick = Server()->Tick();
	delete m_pCharacter;
	m_pCharacter = 0;
	m_KillMe = 0;
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();
	m_LastInvited = 0;
	m_WeakHookSpawn = false;

	int *pIdMap = Server()->GetIdMap(m_ClientID);
	for (int i = 1;i < VANILLA_MAX_CLIENTS;i++)
	{
		pIdMap[i] = -1;
	}
	pIdMap[0] = m_ClientID;

	// DDRace

	m_LastCommandPos = 0;
	m_LastPlaytime = time_get();
	m_Sent1stAfkWarning = 0;
	m_Sent2ndAfkWarning = 0;
	m_ChatScore = 0;
	m_Moderating = false;
	m_EyeEmote = true;
	m_TimerType = (g_Config.m_SvDefaultTimerType == CPlayer::TIMERTYPE_GAMETIMER || g_Config.m_SvDefaultTimerType == CPlayer::TIMERTYPE_GAMETIMER_AND_BROADCAST) ? CPlayer::TIMERTYPE_BROADCAST : g_Config.m_SvDefaultTimerType;
	m_DefEmote = EMOTE_NORMAL;
	m_Afk = false;
	m_LastWhisperTo = -1;
	m_LastSetSpectatorMode = 0;
	m_TimeoutCode[0] = '\0';
	m_ModhelpTick = -1;

	m_TuneZone = 0;
	m_TuneZoneOld = m_TuneZone;
	m_Halloween = false;
	m_FirstPacket = true;

	m_SendVoteIndex = -1;

	if (g_Config.m_SvEvents)
	{
		time_t rawtime;
		struct tm* timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		if ((timeinfo->tm_mon == 11 && timeinfo->tm_mday == 31) || (timeinfo->tm_mon == 0 && timeinfo->tm_mday == 1))
		{ // New Year
			m_DefEmote = EMOTE_HAPPY;
		}
		else if ((timeinfo->tm_mon == 9 && timeinfo->tm_mday == 31) || (timeinfo->tm_mon == 10 && timeinfo->tm_mday == 1))
		{ // Halloween
			m_DefEmote = EMOTE_ANGRY;
			m_Halloween = true;
		}
		else
		{
			m_DefEmote = EMOTE_NORMAL;
		}
	}
	m_DefEmoteReset = -1;

	GameServer()->Score()->PlayerData(m_ClientID)->Reset();

	m_ClientVersion = VERSION_VANILLA;
	m_ShowOthers = g_Config.m_SvShowOthersDefault;
	m_ShowAll = g_Config.m_SvShowAllDefault;
	m_SpecTeam = 0;
	m_NinjaJetpack = false;
	m_ShowFlag = true;
	m_FastcapSpawnAt = 1;

	m_Paused = PAUSE_NONE;
	m_DND = false;

	m_LastPause = 0;
	m_Score = -9999;
	m_HasFinishScore = false;

	// Variable initialized:
	m_Last_Team = 0;
#if defined(CONF_SQL)
	m_LastSQLQuery = 0;
#endif

	int64 Now = Server()->Tick();
	int64 TickSpeed = Server()->TickSpeed();
	// If the player joins within ten seconds of the server becoming
	// non-empty, allow them to vote immediately. This allows players to
	// vote after map changes or when they join an empty server.
	//
	// Otherwise, block voting for 60 seconds after joining.
	if(Now > GameServer()->m_NonEmptySince + 10 * TickSpeed)
		m_FirstVoteTick = Now + g_Config.m_SvJoinVoteDelay * TickSpeed;
	else
		m_FirstVoteTick = Now;
}

void CPlayer::Tick()
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	if(m_KillMe != 0)
	{
		KillCharacter(m_KillMe);
		m_KillMe = 0;
		return;
	}

	if (m_ChatScore > 0)
		m_ChatScore--;

	Server()->SetClientScore(m_ClientID, m_Score);

	if (m_Moderating && m_Afk)
	{
		m_Moderating = false;
		GameServer()->SendChatTarget(m_ClientID, "Active moderator mode disabled because you are afk.");

		if (!GameServer()->PlayerModerating())
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");
	}

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick()%Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum/Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(Server()->GetNetErrorString(m_ClientID)[0])
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' would have timed out, but can use timeout protection now", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		Server()->ResetNetErrorString(m_ClientID);
	}

	if(!GameServer()->m_World.m_Paused)
	{
		if(!m_pCharacter && m_DieTick+Server()->TickSpeed()*3 <= Server()->Tick())
			m_Spawning = true;

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
			{
				ProcessPause();
				if(!m_Paused)
					m_ViewPos = m_pCharacter->m_Pos;
			}
			else if(!m_pCharacter->IsPaused())
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else
			TryRespawn();
	}
	else
	{
		++m_DieTick;
		++m_JoinTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
	}

	m_TuneZoneOld = m_TuneZone; // determine needed tunings with viewpos
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_ViewPos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if (m_TuneZone != m_TuneZoneOld) // don't send tunigs all the time
	{
		GameServer()->SendTuningParams(m_ClientID, m_TuneZone);
	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators
	if((m_Team == TEAM_SPECTATORS || m_Paused) && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID] && GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter())
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter()->m_Pos;
}

void CPlayer::PostPostTick()
{
	#ifdef CONF_DEBUG
		if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
	#endif
		if(!Server()->ClientIngame(m_ClientID))
			return;

	if(!GameServer()->m_World.m_Paused && !m_pCharacter && m_Spawning && m_WeakHookSpawn)
		TryRespawn();
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!Server()->ClientIngame(m_ClientID))
		return;

	int id = m_ClientID;
	if (SnappingClient > -1 && !Server()->Translate(id, SnappingClient)) return;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, id, sizeof(CNetObj_ClientInfo)));

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
	pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
	pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
	pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;

	int Size = Server()->IsSixup(SnappingClient) ? 3*4 : sizeof(CNetObj_PlayerInfo);
	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, Size));
	if(!pPlayerInfo)
		return;

	int Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	int Score = abs(m_Score) * -1;

	// send 0 if times of others are not shown
	if(SnappingClient != m_ClientID && g_Config.m_SvHideScore)
		Score = -9999;
	else
		Score = abs(m_Score) * -1;

	CPlayerData *pData = GameServer()->Score()->PlayerData(GetCID());
	if(!Server()->IsSixup(SnappingClient) && m_Team != TEAM_SPECTATORS && GameServer()->m_apPlayers[SnappingClient] && GameServer()->m_apPlayers[SnappingClient]->GetCharacter() && GameServer()->m_apPlayers[SnappingClient]->GetCharacter()->m_ShowTimesInNames)
	{
		if(pData->m_BestTime)
		{
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf),
					"%02d:%06.3f ",
					(int)pData->m_BestTime / 60, pData->m_BestTime - ((int)pData->m_BestTime / 60 * 60));
			const char *Name = Server()->ClientName(m_ClientID);
			int LastStart = 0;
			for(int i = 0; i < 6; i++)
			{
				aBuf[10+i] = Name[i];
				if(str_utf8_isstart(Name[i]))
					LastStart = i;
				if(!Name[i])
					break;
			}
			aBuf[10+LastStart] = 0;
			StrToInts(&pClientInfo->m_Name0, 4, aBuf);
		}
		GameServer()->SortPlayerScores();
		Score = m_SortedScore;
	}

	if(Server()->IsSixup(SnappingClient))
	{
		((int*)pPlayerInfo)[0] = m_PlayerFlags&PLAYERFLAG_CHATTING ? (1<<1) : 0;
		((int*)pPlayerInfo)[1] = (pData->m_BestTime < 0.000001f) ? -1 : round_to_int(pData->m_BestTime * 1000);
		((int*)pPlayerInfo)[2] = Latency;
	}
	else
	{
		pPlayerInfo->m_Latency = Latency;
		pPlayerInfo->m_Local = 0;
		pPlayerInfo->m_ClientID = id;
		pPlayerInfo->m_Score = Score;
		pPlayerInfo->m_Team = (m_ClientVersion < VERSION_DDNET_OLD || m_Paused != PAUSE_PAUSED || m_ClientID != SnappingClient) && m_Paused < PAUSE_SPEC ? m_Team : TEAM_SPECTATORS;

		if(m_ClientID == SnappingClient && m_Paused == PAUSE_PAUSED && m_ClientVersion < VERSION_DDNET_OLD)
			pPlayerInfo->m_Team = TEAM_SPECTATORS;

		if(m_ClientID == SnappingClient && (m_Paused != PAUSE_PAUSED || m_ClientVersion >= VERSION_DDNET_OLD))
			pPlayerInfo->m_Local = 1;
	}

	if(m_ClientID == SnappingClient && (m_Team == TEAM_SPECTATORS || m_Paused))
	{
		Size = Server()->IsSixup(SnappingClient) ? 4*4 : sizeof(CNetObj_SpectatorInfo);
		CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, Size));
		if(!pSpectatorInfo)
			return;

		if(Server()->IsSixup(SnappingClient))
		{
			((int*)pSpectatorInfo)[0] = m_SpectatorID == SPEC_FREEVIEW ? 0 : 1; // m_SpecMode
			((int*)pSpectatorInfo)[1] = m_SpectatorID;
			((int*)pSpectatorInfo)[2] = m_ViewPos.x;
			((int*)pSpectatorInfo)[3] = m_ViewPos.y;
		}
		else
		{
			pSpectatorInfo->m_SpectatorID = m_SpectatorID;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
	}
}

void CPlayer::FakeSnap()
{
	// This is problematic when it's sent before we know whether it's a non-64-player-client
	// Then we can't spectate players at the start

	if(Server()->IsSixup(m_ClientID))
		return;
	if(m_ClientVersion >= VERSION_DDNET_OLD)
		return;

	int FakeID = VANILLA_MAX_CLIENTS - 1;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, FakeID, sizeof(CNetObj_ClientInfo)));

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, " ");
	StrToInts(&pClientInfo->m_Clan0, 3, "");
	StrToInts(&pClientInfo->m_Skin0, 6, "default");

	if(m_Paused != PAUSE_PAUSED)
		return;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, FakeID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = m_Latency.m_Min;
	pPlayerInfo->m_Local = 1;
	pPlayerInfo->m_ClientID = FakeID;
	pPlayerInfo->m_Score = -9999;
	pPlayerInfo->m_Team = TEAM_SPECTATORS;

	CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, FakeID, sizeof(CNetObj_SpectatorInfo)));
	if(!pSpectatorInfo)
		return;

	pSpectatorInfo->m_SpectatorID = m_SpectatorID;
	pSpectatorInfo->m_X = m_ViewPos.x;
	pSpectatorInfo->m_Y = m_ViewPos.y;
}

void CPlayer::OnDisconnect(const char *pReason)
{
	KillCharacter();

	if(Server()->ClientIngame(m_ClientID))
	{
		char aBuf[512];
		if(pReason && *pReason)
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(m_ClientID), pReason);
		else
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, true);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", m_ClientID, Server()->ClientName(m_ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);

		bool WasModerator = m_Moderating;

		// Set this to false, otherwise PlayerModerating() will return true.
		m_Moderating = false;

		if (!GameServer()->PlayerModerating() && WasModerator)
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");
	}

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
	Controller->m_Teams.SetForceCharacterTeam(m_ClientID, 0);
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	if(Server()->IsSixup(m_ClientID))
		NewInput->m_PlayerFlags = (NewInput->m_PlayerFlags<<1)&(PLAYERFLAG_CHATTING|PLAYERFLAG_SCOREBOARD);

	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	AfkVoteTimer(NewInput);

	m_NumInputs++;

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnPredictedInput(NewInput);

	// Magic number when we can hope that client has successfully identified itself
	if(m_NumInputs == 20)
	{
		if(g_Config.m_SvClientSuggestion[0] != '\0' && m_ClientVersion <= VERSION_DDNET_OLD)
			GameServer()->SendBroadcast(g_Config.m_SvClientSuggestion, m_ClientID);
	}
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if(Server()->IsSixup(m_ClientID))
		NewInput->m_PlayerFlags = (NewInput->m_PlayerFlags<<1)&(PLAYERFLAG_CHATTING|PLAYERFLAG_SCOREBOARD);

	if (AfkTimer(NewInput->m_TargetX, NewInput->m_TargetY))
		return; // we must return if kicked, as player struct is already deleted
	AfkVoteTimer(NewInput);

	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
	// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
		return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter)
	{
		if(!m_Paused)
			m_pCharacter->OnDirectInput(NewInput);
		else
			m_pCharacter->ResetInput();
	}

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		m_Spawning = true;

	if(((!m_pCharacter && m_Team == TEAM_SPECTATORS) || m_Paused) && m_SpectatorID == SPEC_FREEVIEW)
		m_ViewPos = vec2(NewInput->m_TargetX, NewInput->m_TargetY);

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
	}
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::ThreadKillCharacter(int Weapon)
{
	m_KillMe = Weapon;
}

void CPlayer::KillCharacter(int Weapon)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);

		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn(bool WeakHook)
{
	if(m_Team != TEAM_SPECTATORS)
	{
		m_WeakHookSpawn = WeakHook;
		m_Spawning = true;
	}
}

CCharacter* CPlayer::ForceSpawn(vec2 Pos)
{
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, Pos);
	m_Team = 0;
	return m_pCharacter;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	// clamp the team
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	char aBuf[512];
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(m_ClientID), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, true);
	}

	if(Team == TEAM_SPECTATORS)
	{
		CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;
		Controller->m_Teams.SetForceCharacterTeam(m_ClientID, 0);
	}

	KillCharacter();

	m_Team = Team;
	m_LastSetTeam = Server()->Tick();
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	//GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(Server()->ClientIngame(i) && Server()->IsSixup(i))
		{
			CMsgPacker Msg(4 + 26); // NETMSGTYPE_SV_TEAM
			Msg.AddInt(m_ClientID);
			Msg.AddInt(m_Team);
			Msg.AddInt(!DoChatMsg); // m_Silent
			Msg.AddInt(m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay); // m_CooldownTick
			Server()->SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
		}
	}

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
				GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
		}
	}
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;

	if(!GameServer()->m_pController->CanSpawn(m_Team, g_Config.m_SvFastcap ? m_FastcapSpawnAt : 0, &SpawnPos))
		return;

	CGameControllerDDRace* Controller = (CGameControllerDDRace*)GameServer()->m_pController;

	m_WeakHookSpawn = false;
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));

	if(g_Config.m_SvTeam == 3)
	{
		int NewTeam = 0;
		for(; NewTeam < TEAM_SUPER; NewTeam++)
			if(Controller->m_Teams.Count(NewTeam) == 0)
				break;

		if(NewTeam == TEAM_SUPER)
			NewTeam = 0;

		Controller->m_Teams.SetForceCharacterTeam(GetCID(), NewTeam);
	}
}

bool CPlayer::AfkTimer(int NewTargetX, int NewTargetY)
{
	/*
		afk timer (x, y = mouse coordinates)
		Since a player has to move the mouse to play, this is a better method than checking
		the player's position in the game world, because it can easily be bypassed by just locking a key.
		Frozen players could be kicked as well, because they can't move.
		It also works for spectators.
		returns true if kicked
	*/

	if(Server()->GetAuthedState(m_ClientID))
		return false; // don't kick admins
	if(g_Config.m_SvMaxAfkTime == 0)
		return false; // 0 = disabled

	if(NewTargetX != m_LastTarget_x || NewTargetY != m_LastTarget_y)
	{
		m_LastPlaytime = time_get();
		m_LastTarget_x = NewTargetX;
		m_LastTarget_y = NewTargetY;
		m_Sent1stAfkWarning = 0; // afk timer's 1st warning after 50% of sv_max_afk_time
		m_Sent2ndAfkWarning = 0;

	}
	else
	{
		if(!m_Paused)
		{
			// not playing, check how long
			if(m_Sent1stAfkWarning == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.5))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime*0.5),
					g_Config.m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent1stAfkWarning = 1;
			}
			else if(m_Sent2ndAfkWarning == 0 && m_LastPlaytime < time_get()-time_freq()*(int)(g_Config.m_SvMaxAfkTime*0.9))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime*0.9),
					g_Config.m_SvMaxAfkTime
				);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent2ndAfkWarning = 1;
			}
			else if(m_LastPlaytime < time_get()-time_freq()*g_Config.m_SvMaxAfkTime)
			{
				m_pGameServer->Server()->Kick(m_ClientID, "Away from keyboard");
				return true;
			}
		}
	}
	return false;
}

void CPlayer::AfkVoteTimer(CNetObj_PlayerInput *NewTarget)
{
	if(g_Config.m_SvMaxAfkVoteTime == 0)
		return;

	if(mem_comp(NewTarget, &m_LastTarget, sizeof(CNetObj_PlayerInput)) != 0)
	{
		m_LastPlaytime = time_get();
		mem_copy(&m_LastTarget, NewTarget, sizeof(CNetObj_PlayerInput));
	}
	else if(m_LastPlaytime < time_get()-time_freq()*g_Config.m_SvMaxAfkVoteTime)
	{
		m_Afk = true;
		return;
	}

	m_Afk = false;
}

void CPlayer::ProcessPause()
{
	if(m_ForcePauseTime && m_ForcePauseTime < Server()->Tick())
	{
		m_ForcePauseTime = 0;
		Pause(PAUSE_NONE, true);
	}

	if(m_Paused == PAUSE_SPEC && !m_pCharacter->IsPaused() && m_pCharacter->IsGrounded() && m_pCharacter->m_Pos == m_pCharacter->m_PrevPos)
	{
		m_pCharacter->Pause(true);
		GameServer()->CreateDeath(m_pCharacter->m_Pos, m_ClientID, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
		GameServer()->CreateSound(m_pCharacter->m_Pos, SOUND_PLAYER_DIE, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
	}
}

int CPlayer::Pause(int State, bool Force)
{
	if(State < PAUSE_NONE || State > PAUSE_SPEC) // Invalid pause state passed
		return 0;

	if(!m_pCharacter)
		return 0;

	char aBuf[128];
	if(State != m_Paused)
	{
		// Get to wanted state
		switch(State){
		case PAUSE_PAUSED:
		case PAUSE_NONE:
			if(m_pCharacter->IsPaused()) // First condition might be unnecessary
			{
				if(!Force && m_LastPause && m_LastPause + g_Config.m_SvSpecFrequency * Server()->TickSpeed() > Server()->Tick())
				{
					GameServer()->SendChatTarget(m_ClientID, "Can't /spec that quickly.");
					return m_Paused; // Do not update state. Do not collect $200
				}
				m_pCharacter->Pause(false);
				GameServer()->CreatePlayerSpawn(m_pCharacter->m_Pos, m_pCharacter->Teams()->TeamMask(m_pCharacter->Team(), -1, m_ClientID));
			}
			// fall-thru
		case PAUSE_SPEC:
			if(g_Config.m_SvPauseMessages)
			{
				str_format(aBuf, sizeof(aBuf), (State > PAUSE_NONE) ? "'%s' speced" : "'%s' resumed", Server()->ClientName(m_ClientID));
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			}
			break;
		}

		// Update state
		m_Paused = State;
		m_LastPause = Server()->Tick();
	}

	return m_Paused;
}

int CPlayer::ForcePause(int Time)
{
	m_ForcePauseTime = Server()->Tick() + Server()->TickSpeed() * Time;

	if(g_Config.m_SvPauseMessages)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' was force-paused for %ds", Server()->ClientName(m_ClientID), Time);
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	return Pause(PAUSE_SPEC, true);
}

int CPlayer::IsPaused()
{
	return m_ForcePauseTime ? m_ForcePauseTime : -1 * m_Paused;
}

bool CPlayer::IsPlaying()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return true;
	return false;
}

void CPlayer::FindDuplicateSkins()
{
	if (m_TeeInfos.m_UseCustomColor == 0 && !m_StolenSkin) return;
	m_StolenSkin = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == m_ClientID) continue;
		if(GameServer()->m_apPlayers[i])
		{
			if(GameServer()->m_apPlayers[i]->m_StolenSkin) continue;
			if((GameServer()->m_apPlayers[i]->m_TeeInfos.m_UseCustomColor == m_TeeInfos.m_UseCustomColor) &&
			(GameServer()->m_apPlayers[i]->m_TeeInfos.m_ColorFeet == m_TeeInfos.m_ColorFeet) &&
			(GameServer()->m_apPlayers[i]->m_TeeInfos.m_ColorBody == m_TeeInfos.m_ColorBody) &&
			!str_comp(GameServer()->m_apPlayers[i]->m_TeeInfos.m_SkinName, m_TeeInfos.m_SkinName))
			{
				m_StolenSkin = 1;
				return;
			}
		}
	}
}

void CPlayer::SpectatePlayerName(const char *pName)
{
	if(!pName)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i != m_ClientID && Server()->ClientIngame(i) && !str_comp(pName, Server()->ClientName(i)))
		{
			m_SpectatorID = i;
			return;
		}
	}
}

struct StdSkin
{
	char m_SkinName[64];
	char m_apSkinPartNames[6][24];
	bool m_aUseCustomColors[6];
	int m_aSkinPartColors[6];
};

static StdSkin g_StdSkins[] = {
{"default",{"standard","","","standard","standard","standard"},{true,false,false,true,true,false},{1798004,0,0,1799582,1869630,0}},
{"bluekitty",{"kitty","whisker","","standard","standard","standard"},{true,true,false,true,true,false},{8681144,-8229413,0,7885547,7885547,0}},
{"bluestripe",{"standard","stripes","","standard","standard","standard"},{true,false,false,true,true,false},{10187898,0,0,750848,1944919,0}},
{"brownbear",{"bear","bear","hair","standard","standard","standard"},{true,true,false,true,true,false},{1082745,-15634776,0,1082745,1147174,0}},
{"cammo",{"standard","cammo2","","standard","standard","standard"},{true,true,false,true,true,false},{5334342,-11771603,0,750848,1944919,0}},
{"cammostripes",{"standard","cammostripes","","standard","standard","standard"},{true,true,false,true,true,false},{5334342,-14840320,0,750848,1944919,0}},
{"coala",{"koala","twinbelly","","standard","standard","standard"},{true,true,false,true,true,false},{184,-15397662,0,184,9765959,0}},
{"limekitty",{"kitty","whisker","","standard","standard","standard"},{true,true,false,true,true,false},{4612803,-12229920,0,3827951,3827951,0}},
{"pinky",{"standard","whisker","","standard","standard","standard"},{true,true,false,true,true,false},{15911355,-801066,0,15043034,15043034,0}},
{"redbopp",{"standard","donny","unibop","standard","standard","standard"},{true,true,true,true,true,false},{16177260,-16590390,16177260,16177260,7624169,0}},
{"redstripe",{"standard","stripe","","standard","standard","standard"},{true,false,false,true,true,false},{16307835,0,0,184,9765959,0}},
{"saddo",{"standard","saddo","","standard","standard","standard"},{true,true,false,true,true,false},{7171455,-9685436,0,3640746,5792119,0}},
{"toptri",{"standard","toptri","","standard","standard","standard"},{true,false,false,true,true,false},{6119331,0,0,3640746,5792119,0}},
{"twinbop",{"standard","duodonny","twinbopp","standard","standard","standard"},{true,true,true,true,true,false},{15310519,-1600806,15310519,15310519,37600,0}},
{"twintri",{"standard","twintri","","standard","standard","standard"},{true,true,false,true,true,false},{3447932,-14098717,0,185,9634888,0}},
{"warpaint",{"standard","warpaint","","standard","standard","standard"},{true,false,false,true,true,false},{1944919,0,0,750337,1944919,0}},
/* non-standard 0.6 skins that are part of vanilla 0.7 */
{"greensward",{"spiky","duodonny","","standard","standard","standard"},{true,true,false,true,true,true},{4959008,1616196797,0,5599232,5592988,1880670}},
{"greyfox",{"fox","cammostripes","","standard","standard","colorable"},{true,true,false,true,true,true},{1051203,538383807,0,10230803,2100280,1858113}},
{"monkey",{"monkey","monkey","hair","standard","standard","standard"},{true,true,true,true,true,false},{1421252,-15301582,2352795,1659536,1274287,0}},
{"pandabear",{"bear","panda1","hair","standard","standard","standard"},{true,true,false,true,true,true},{9834574,-6411543,0,1769630,1835070,41215}},
{"tiger",{"kitty","tiger1","","standard","standard","colorable"},{true,true,false,true,true,true},{1495659,-602669093,0,1487971,1495666,1900288}}};

void CPlayer::SkinToSixup()
{
	// reset to default skin
	for(int p = 0; p < 6; p++)
	{
		str_copy(m_TeeInfos.m_apSkinPartNames[p], g_StdSkins[0].m_apSkinPartNames[p], 24);
		m_TeeInfos.m_aUseCustomColors[p] = g_StdSkins[0].m_aUseCustomColors[p];
		m_TeeInfos.m_aSkinPartColors[p] = g_StdSkins[0].m_aSkinPartColors[p];
	}

	// check for std skin
	for(int s = 0; s < 16 + 5; s++)
	{
		if(!str_comp(m_TeeInfos.m_SkinName, g_StdSkins[s].m_SkinName))
		{
			for(int p = 0; p < 6; p++)
			{
				str_copy(m_TeeInfos.m_apSkinPartNames[p], g_StdSkins[s].m_apSkinPartNames[p], 24);
				m_TeeInfos.m_aUseCustomColors[p] = g_StdSkins[s].m_aUseCustomColors[p];
				m_TeeInfos.m_aSkinPartColors[p] = g_StdSkins[s].m_aSkinPartColors[p];
			}
			break;
		}
	}

	if(m_TeeInfos.m_UseCustomColor)
	{
		m_TeeInfos.m_aUseCustomColors[0] = true;
		m_TeeInfos.m_aUseCustomColors[1] = true;
		m_TeeInfos.m_aUseCustomColors[2] = true;
		m_TeeInfos.m_aUseCustomColors[3] = true;
		m_TeeInfos.m_aUseCustomColors[4] = true;
		m_TeeInfos.m_aSkinPartColors[0] = m_TeeInfos.m_ColorBody;
		m_TeeInfos.m_aSkinPartColors[1] = 0x22FFFFFF;
		m_TeeInfos.m_aSkinPartColors[2] = m_TeeInfos.m_ColorBody;
		m_TeeInfos.m_aSkinPartColors[3] = m_TeeInfos.m_ColorBody;
		m_TeeInfos.m_aSkinPartColors[4] = m_TeeInfos.m_ColorFeet;
	}
}

void CPlayer::SkinFromSixup()
{
	// reset to default skin
	str_copy(m_TeeInfos.m_SkinName, "default", sizeof(m_TeeInfos.m_SkinName));
	m_TeeInfos.m_UseCustomColor = false;
	m_TeeInfos.m_ColorBody = 0;
	m_TeeInfos.m_ColorFeet = 0;

	// check for std skin
	for(int s = 0; s < 16 + 5; s++)
	{
		bool match = true;
		for(int p = 0; p < 6; p++)
		{
			if(str_comp(m_TeeInfos.m_apSkinPartNames[p], g_StdSkins[s].m_apSkinPartNames[p])
					|| m_TeeInfos.m_aUseCustomColors[p] != g_StdSkins[s].m_aUseCustomColors[p]
					|| (m_TeeInfos.m_aUseCustomColors[p] && m_TeeInfos.m_aSkinPartColors[p] != g_StdSkins[s].m_aSkinPartColors[p]))
			{
				match = false;
				break;
			}
		}
		if(match)
		{
			str_copy(m_TeeInfos.m_SkinName, g_StdSkins[s].m_SkinName, sizeof(m_TeeInfos.m_SkinName));
			return;
		}
	}

	// find closest match
	int best_skin;
	int best_matches = -1;
	for(int s = 0; s < 16; s++)
	{
		int matches = 0;
		for(int p = 0; p < 3; p++)
			if(str_comp(m_TeeInfos.m_apSkinPartNames[p], g_StdSkins[s].m_apSkinPartNames[p]) == 0)
				matches++;

		if(matches > best_matches)
		{
			best_matches = matches;
			best_skin = s;
		}
	}

	str_copy(m_TeeInfos.m_SkinName, g_StdSkins[best_skin].m_SkinName, sizeof(m_TeeInfos.m_SkinName));
	m_TeeInfos.m_UseCustomColor = true;
	m_TeeInfos.m_ColorBody = m_TeeInfos.m_aUseCustomColors[0] ? m_TeeInfos.m_aSkinPartColors[0] : 255;
	m_TeeInfos.m_ColorFeet = m_TeeInfos.m_aUseCustomColors[4] ? m_TeeInfos.m_aSkinPartColors[4] : 255;
}
