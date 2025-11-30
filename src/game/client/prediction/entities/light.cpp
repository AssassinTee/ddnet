/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "light.h"

#include "character.h"

#include <game/client/laser_data.h>
#include <game/collision.h>
#include <game/mapitems.h>

CLight::CLight(CGameWorld *pGameWorld, int Id, const CLaserData *pData) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LIGHT)
{
	m_Id = Id;
	//m_Active = false;

	Read(pData);
	
  // TODO I don't know these values
  m_LengthL = 0;
	m_AngularSpeed = 0;
	m_Speed = 0;
  m_CurveLength = 0;
	m_Length = distance(m_Core, m_To);

	m_Number = pData->m_SwitchNumber;
	m_Layer = m_Number > 0 ? LAYER_SWITCH : LAYER_GAME;
}

bool CLight::HitCharacter()
{
	std::vector<CCharacter *> vpHitCharacters = GameWorld()->IntersectedCharacters(m_Pos, m_To, 0.0f, nullptr);
	if(vpHitCharacters.empty())
		return false;
	for(auto *pChar : vpHitCharacters)
	{
		if(m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pChar->Team()])
			continue;
		pChar->Freeze();
	}
	return true;
}

void CLight::Tick()
{
	if(GameWorld()->GameTick() % (int)(GameWorld()->GameTickSpeed() * 0.15f) == 0)
	{
		m_EvalTick = GameWorld()->GameTick();
		Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &m_Core);
		m_Pos += m_Core;
		Step();
	}

	HitCharacter();
}

void CLight::Move()
{
	if(m_Speed != 0)
	{
		if((m_CurveLength >= m_Length && m_Speed > 0) || (m_CurveLength <= 0 && m_Speed < 0))
			m_Speed = -m_Speed;
		m_CurveLength += m_Speed * m_Tick + m_LengthL;
		m_LengthL = 0;
		if(m_CurveLength > m_Length)
		{
			m_LengthL = m_CurveLength - m_Length;
			m_CurveLength = m_Length;
		}
		else if(m_CurveLength < 0)
		{
			m_LengthL = 0 + m_CurveLength;
			m_CurveLength = 0;
		}
	}

	m_Rotation += m_AngularSpeed * m_Tick;
	if(m_Rotation > pi * 2)
		m_Rotation -= pi * 2;
	else if(m_Rotation < 0)
		m_Rotation += pi * 2;
}

void CLight::Step()
{
	Move();
	const vec2 Direction = vec2(std::sin(m_Rotation), std::cos(m_Rotation));
	const vec2 NextPosition = m_Pos + normalize(Direction) * m_CurveLength;
	Collision()->IntersectNoLaser(m_Pos, NextPosition, &m_To, nullptr);
}

void CLight::Read(const CLaserData *pData)
{
	// it's flipped in the laser object
	m_Pos = pData->m_To;
	m_To = pData->m_From;
}

bool CLight::Match(const CLight *pLight) const
{
	return pLight->m_Pos == m_Pos && pLight->m_To == m_To && pLight->m_Number == m_Number;
}
