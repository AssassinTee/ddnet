/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"
#include "character.h"

#include <engine/shared/config.h>

#include <generated/protocol.h>
#include <generated/server_data.h>

#include <game/mapitems.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/teamscore.h>

static constexpr int gs_PickupPhysSize = 14;

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType, int Layer, int Number, int Flags) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, vec2(0, 0), gs_PickupPhysSize)
{
	m_Core = vec2(0.0f, 0.0f);
	m_Type = Type;
	m_Subtype = SubType;

	m_Layer = Layer;
	m_Number = Number;
	m_Flags = Flags;
	std::fill_n(m_aSpawnTick, MAX_CLIENTS, -1);

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	m_MarkedForDestroy = true;
	std::fill_n(m_aSpawnTick, MAX_CLIENTS, -1);
}

void CPickup::Tick()
{
	Move();

	// wait for
	if(g_Config.m_SvHealthAndAmmo)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(!GameServer()->m_apPlayers[i] || !GameServer()->m_apPlayers[i]->GetCharacter())
			{
				m_aSpawnTick[i] = -1;
			}
			else if(m_aSpawnTick[i] > 0 && Server()->Tick() > m_aSpawnTick[i])
			{
				// respawn
				m_aSpawnTick[i] = -1;

				if(m_Type == POWERUP_WEAPON)
					GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, GameServer()->m_apPlayers[i]->GetCharacter()->TeamMask());
			}
		}
	}

	// Check if a player intersected us
	CEntity *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, GetProximityRadius() + ms_CollisionExtraSize, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; ++i)
	{
		auto *pChr = static_cast<CCharacter *>(apEnts[i]);

		if(pChr && pChr->IsAlive())
		{
			if(g_Config.m_SvHealthAndAmmo && m_aSpawnTick[pChr->GetPlayer()->GetCid()] != -1)
				continue;
			if(m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pChr->Team()])
				continue;

			int RespawnTime = -1;
			bool Sound = false;
			// player picked us up, is someone was hooking us, let them go
			switch(m_Type)
			{
			case POWERUP_HEALTH:
				if(g_Config.m_SvHealthAndAmmo)
				{
					if(pChr->IncreaseHealth(1))
					{
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->TeamMask());
						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
					}
				}
				else
				{
					if(pChr->Freeze())
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->TeamMask());
				}
				break;

			case POWERUP_ARMOR:
				if(g_Config.m_SvHealthAndAmmo)
				{
					if(pChr->IncreaseArmor(1))
					{
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
					}
				}
				else
				{
					if(pChr->Team() == TEAM_SUPER)
						continue;
					for(int j = WEAPON_SHOTGUN; j < NUM_WEAPONS; j++)
					{
						if(pChr->GetWeaponGot(j))
						{
							pChr->SetWeaponGot(j, false);
							pChr->SetWeaponAmmo(j, 0);
							Sound = true;
						}
					}
					pChr->SetNinjaActivationDir(vec2(0, 0));
					pChr->SetNinjaActivationTick(-500);
					pChr->SetNinjaCurrentMoveTime(0);
					if(Sound)
					{
						pChr->SetLastWeapon(WEAPON_GUN);
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
					}
					if(pChr->GetActiveWeapon() >= WEAPON_SHOTGUN)
						pChr->SetActiveWeapon(WEAPON_HAMMER);
				}
				break;

			case POWERUP_ARMOR_SHOTGUN:
				if(pChr->Team() == TEAM_SUPER)
					continue;
				if(pChr->GetWeaponGot(WEAPON_SHOTGUN))
				{
					pChr->SetWeaponGot(WEAPON_SHOTGUN, false);
					pChr->SetWeaponAmmo(WEAPON_SHOTGUN, 0);
					pChr->SetLastWeapon(WEAPON_GUN);
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
				}
				if(pChr->GetActiveWeapon() == WEAPON_SHOTGUN)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_ARMOR_GRENADE:
				if(pChr->Team() == TEAM_SUPER)
					continue;
				if(pChr->GetWeaponGot(WEAPON_GRENADE))
				{
					pChr->SetWeaponGot(WEAPON_GRENADE, false);
					pChr->SetWeaponAmmo(WEAPON_GRENADE, 0);
					pChr->SetLastWeapon(WEAPON_GUN);
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
				}
				if(pChr->GetActiveWeapon() == WEAPON_GRENADE)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_ARMOR_NINJA:
				if(pChr->Team() == TEAM_SUPER)
					continue;
				pChr->SetNinjaActivationDir(vec2(0, 0));
				pChr->SetNinjaActivationTick(-500);
				pChr->SetNinjaCurrentMoveTime(0);
				break;

			case POWERUP_ARMOR_LASER:
				if(pChr->Team() == TEAM_SUPER)
					continue;
				if(pChr->GetWeaponGot(WEAPON_LASER))
				{
					pChr->SetWeaponGot(WEAPON_LASER, false);
					pChr->SetWeaponAmmo(WEAPON_LASER, 0);
					pChr->SetLastWeapon(WEAPON_GUN);
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
				}
				if(pChr->GetActiveWeapon() == WEAPON_LASER)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_WEAPON:

				if(m_Subtype >= 0 && m_Subtype < NUM_WEAPONS && (!pChr->GetWeaponGot(m_Subtype) || pChr->GetWeaponAmmo(m_Subtype) != -1) && (!g_Config.m_SvHealthAndAmmo || pChr->GetWeaponAmmo(m_Subtype) != g_pData->m_Weapons.m_aId[m_Subtype].m_Maxammo))
				{
					pChr->GiveWeapon(m_Subtype);

					if(g_Config.m_SvHealthAndAmmo)
						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

					if(m_Subtype == WEAPON_GRENADE)
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, pChr->TeamMask());
					else if(m_Subtype == WEAPON_SHOTGUN)
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->TeamMask());
					else if(m_Subtype == WEAPON_LASER)
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->TeamMask());

					if(pChr->GetPlayer())
						GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCid(), m_Subtype);
				}
				break;

			case POWERUP_NINJA:
			{
				// activate ninja on target player
				pChr->GiveNinja();
				break;
			}
			default:
				break;
			};

			if(RespawnTime >= 0 && g_Config.m_SvHealthAndAmmo)
				m_aSpawnTick[pChr->GetPlayer()->GetCid()] = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
		}
	}
}

void CPickup::TickPaused()
{
	for(int &SpawnTick : m_aSpawnTick)
		if(SpawnTick != -1)
			++SpawnTick;
}

void CPickup::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || (SnappingClient >= 0 && m_aSpawnTick[SnappingClient] != -1 && g_Config.m_SvHealthAndAmmo))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	bool Sixup = Server()->IsSixup(SnappingClient);

	if(SnappingClientVersion < VERSION_DDNET_ENTITY_NETOBJS)
	{
		CCharacter *pChar = GameServer()->GetPlayerChar(SnappingClient);

		if(SnappingClient != SERVER_DEMO_CLIENT && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[SnappingClient]->IsPaused()) && GameServer()->m_apPlayers[SnappingClient]->SpectatorId() != SPEC_FREEVIEW)
			pChar = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->SpectatorId());

		int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;
		if(pChar && pChar->IsAlive() && m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pChar->Team()] && !Tick)
			return;
	}

	GameServer()->SnapPickup(CSnapContext(SnappingClientVersion, Sixup, SnappingClient), GetId(), m_Pos, m_Type, m_Subtype, m_Number, m_Flags);
}

void CPickup::Move()
{
	if(Server()->Tick() % (int)(Server()->TickSpeed() * 0.15f) == 0)
	{
		GameServer()->Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &m_Core);
		m_Pos += m_Core;
	}
}
