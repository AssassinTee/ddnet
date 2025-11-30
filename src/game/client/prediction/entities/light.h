/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_LIGHT_H
#define GAME_CLIENT_PREDICTION_ENTITIES_LIGHT_H

#include <game/client/prediction/entity.h>

class CLaserData;

class CLight : public CEntity
{
	float m_Rotation;
	vec2 m_To;
	vec2 m_Core;
	int m_EvalTick;
	int m_Tick;

	int m_CurveLength;
	int m_LengthL;
	float m_AngularSpeed;
	int m_Speed;
	int m_Length;

	bool HitCharacter();
	void Move();
	void Step();

public:
	CLight(CGameWorld *pGameWorld, int Id, const CLaserData *pData);
	bool Match(const CLight *pLight) const;
	void Read(const CLaserData *pData);
	void Tick() override;
};

#endif // GAME_CLIENT_PREDICTION_ENTITIES_DOOR_H
