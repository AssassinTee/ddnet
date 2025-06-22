#ifndef GAME_SERVER_GAMEMODES_UNIQUE_H
#define GAME_SERVER_GAMEMODES_UNIQUE_H

#include <game/server/gamecontroller.h>

class CGameControllerUnique : public IGameController
{
public:
	CGameControllerUnique(class CGameContext *pGameServer);
	~CGameControllerUnique();

	CScore *Score();

	void HandleCharacterTiles(class CCharacter *pChr, int MapIndex, float FractionOfTick) override;
	void SetArmorProgress(CCharacter *pCharacter, int Progress) override;

	void OnPlayerConnect(class CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;

	void OnReset() override;

	void Tick() override;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true) override;
	bool IsUniqueRace() const override { return true; }
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
};
#endif // GAME_SERVER_GAMEMODES_UNIQUE_H
