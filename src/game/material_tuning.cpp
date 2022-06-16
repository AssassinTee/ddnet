//
// Created by Marvin on 16.06.2022.
//
#include "material_tuning.h"
#include <base/math.h>
#include <base/system.h>
#include <game/generated/protocol.h>

const char *CTuneParams::ms_apNames[] =
	{
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) #ScriptName,
#include "tuning.h"
#undef MACRO_TUNING_PARAM
};

bool CTuneParams::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CTuneParams::Get(int Index, float *pValue) const
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CTuneParams::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, ms_apNames[i]) == 0)
			return Set(i, Value);
	return false;
}

bool CTuneParams::Get(const char *pName, float *pValue) const
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, ms_apNames[i]) == 0)
			return Get(i, pValue);

	return false;
}

int CMatDefault::GetSkidSound()
{
	return SOUND_PLAYER_SKID;
}

CMatIce::CMatIce()
{
	m_TuneParams.m_GroundFriction = 0.99f;
	m_TuneParams.m_GroundControlSpeed = 150.0f;
	m_TuneParams.m_GroundControlAccel = 20.0f / ms_TicksPerSecond;
}

float CMatIce::GetSkidThreshold()
{
	return 100.0f;
}

CMatSand::CMatSand()
{
	m_TuneParams.m_GroundFriction = 0.7f; //lower friction
	m_TuneParams.m_GroundControlSpeed = 12.0f; //TODO check this
	m_TuneParams.m_GroundControlAccel = 50.0f / ms_TicksPerSecond; //low acceleration
	//TODO add sand skid sound
}

float CMatSand::GetSkidThreshold()
{
	return 0.1f; //Always skid
}

CMatPenalty::CMatPenalty()
{
	m_TuneParams.m_GroundFriction = 0.4f; //high friction
	m_TuneParams.m_GroundControlSpeed = 10.0f; //low max speed
	m_TuneParams.m_GroundControlAccel = -10.0f / ms_TicksPerSecond; //penalty low accell!

	m_PenaltyControlAccel = 25.0f; //cutoff where the default m_GroundControlAccel starts to get less negative
	m_PenaltyMinAccel = 30.0f / ms_TicksPerSecond; //minimal allowed acceleration on penalty
	m_PenaltyScale = 0.51337; //scale of extra penalty for even higher speeds
}

float CMatPenalty::GetSkidThreshold()
{
	return 700.0f; //high friction, late skids
}

float CMatPenalty::GetDynamicGroundAccel(float Vel, int Dir)
{
	float TotalVelocity = abs(Vel);
	float Accel;
	//Player wants to slow down on penalty
	if((Vel > 0 && Dir < 0) || (Vel < 0 && Dir > 0))
		Accel = m_PenaltyMinAccel + logf(TotalVelocity + 1) * m_PenaltyScale;

	//Make the penalty bigger with bigger speeds
	else if(TotalVelocity > m_PenaltyControlAccel)
	{
		Accel = m_TuneParams.m_GroundControlAccel - logf(TotalVelocity - m_PenaltyControlAccel + 1) * m_PenaltyScale;
	}
	else
	{
		//allow for a minimal acceleration on low speeds
		Accel = mix(m_PenaltyMinAccel, (float)m_TuneParams.m_GroundControlAccel, TotalVelocity / m_PenaltyControlAccel);
	}
	return HandleDirection(Accel, Vel, Dir);
}

// SLIME

float IMatSlime::GetSkidThreshold()
{
	return -1.0f; //slime shouldn't skid
}

CMatSlime::CMatSlime()
{
	m_TuneParams.m_GroundElasticityX = 1.0f;
	m_TuneParams.m_GroundElasticityY = 1.0f;
}

CMatSlimeV::CMatSlimeV()
{
	m_TuneParams.m_GroundElasticityY = 1.0f;
}

CMatSlimeH::CMatSlimeH()
{
	m_TuneParams.m_GroundElasticityX = 1.0f;
}

CMatSlimeWeak::CMatSlimeWeak()
{
	m_TuneParams.m_GroundElasticityX = 0.5f;
	m_TuneParams.m_GroundElasticityY = 0.5f;
}

CMatSlimeWeakV::CMatSlimeWeakV()
{
	m_TuneParams.m_GroundElasticityY = 0.5f;
}

CMatSlimeWeakH::CMatSlimeWeakH()
{
	m_TuneParams.m_GroundElasticityX = 0.5f;
}

// IBOOSTER

float IMatBooster::HandleDirection(float Accel, float Vel, int Dir)
{
	//player want's to change direction
	if(m_BoosterDir == BoosterDir::RIGHT)
		return Accel;
	else if(m_BoosterDir == BoosterDir::LEFT)
		return -Accel;

	if((Vel > 0 && Dir < 0) || (Vel < 0 && Dir > 0))
		return m_TuneParams.m_GroundControlAccel * Dir;
	if(Dir == 0)
	{
		if(Vel > 0)
			return Accel; //keep acclerating when the player doesn't press
		else if(Vel < 0)
			return -Accel;
		else
			return 0.0f; //landing perfectly on a bidirectional booster, you don't accel anywhere
	}
	return Accel * Dir;
}

float IMatBooster::GetDynamicGroundAccel(float Vel, int Dir)
{
	float TotalVelocity = abs(Vel);
	//Player wants to switch direction

	float Accel;
	if(TotalVelocity >= m_BoosterMaxControlAccel)
	{
		Accel = m_BoosterHighSpeedAccel;
	}
	else if(TotalVelocity <= m_BoosterMinControlAccel)
	{
		Accel = m_TuneParams.m_GroundControlAccel;
	}
	else
	{
		//mix them linearly for now
		Accel = mix((float)m_TuneParams.m_GroundControlAccel, m_BoosterHighSpeedAccel, (TotalVelocity - m_BoosterMinControlAccel) / (m_BoosterMaxControlAccel - m_BoosterMinControlAccel));
	}
	// don't gain over m_BoosterMaxControlAccel speed, but don't go under m_BoosterHighSpeedAccel, because the player might be faster
	if(TotalVelocity + Accel > m_BoosterMaxControlAccel)
		Accel = maximum(m_BoosterHighSpeedAccel, m_BoosterMaxControlAccel - TotalVelocity);
	return HandleDirection(Accel, Vel, Dir);
}

// BOOSTER

CMatBoosterBidirect::CMatBoosterBidirect()
{
	m_BoosterMinControlAccel = 50.0f; // at this speed value the acceleration starts to get mixed
	m_BoosterMaxControlAccel = 100.0f; // end of mixing
	m_BoosterHighSpeedAccel = 0.0f; // stop accelerating when max control speed is reached
	m_TuneParams.m_GroundControlSpeed = 150.0f; //max speed is at 150
	// m_TuneParams.m_VelrampStart = m_BoosterMaxControlAccel * 50; this is buggy, because this doesn't get send constantly
	m_TuneParams.m_GroundControlAccel = 200.0f / ms_TicksPerSecond; //default acceleration on low speeds
	m_BoosterDir = BoosterDir::BIDIRECT;
}

CMatBoosterLeft::CMatBoosterLeft()
{
	m_BoosterDir = BoosterDir::LEFT;
}

CMatBoosterRight::CMatBoosterRight()
{
	m_BoosterDir = BoosterDir::RIGHT;
}

CMatBoosterWeakBidirect::CMatBoosterWeakBidirect()
{
	m_BoosterMinControlAccel = 25.0f; // at this speed value the acceleration starts to get mixed
	m_BoosterMaxControlAccel = 50.0f; // end of mixing
	m_BoosterHighSpeedAccel = 0.0f; // stop accelerating when max control speed is reached
	m_TuneParams.m_GroundControlSpeed = 75.0f;
	// m_TuneParams.m_VelrampStart = m_BoosterMaxControlAccel * 50; this is buggy, because this doesn't get send constantly
	m_TuneParams.m_GroundControlAccel = 100.0f / ms_TicksPerSecond; //default acceleration on low speeds
	m_BoosterDir = BoosterDir::BIDIRECT;
}

CMatBoosterWeakLeft::CMatBoosterWeakLeft()
{
	m_BoosterDir = BoosterDir::LEFT;
}

CMatBoosterWeakRight::CMatBoosterWeakRight()
{
	m_BoosterDir = BoosterDir::RIGHT;
}