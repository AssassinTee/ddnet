//
// Created by Marvin on 16.06.2022.
//
#include "material_tuning.h"
#include "material.h"

#include <base/math.h>
#include <game/generated/protocol.h>

int CMatDefault::GetSkidSound()
{
	return SOUND_PLAYER_SKID;
}

CMatIce::CMatIce()
{
	m_GroundFriction = 0.99f;
	m_GroundControlSpeed = 150.0f;
	m_GroundControlAccel = 20.0f / ms_TicksPerSecond;
}

float CMatIce::GetSkidThreshold()
{
	return 100.0f;
}

CMatSand::CMatSand()
{
	m_GroundFriction = 0.7f; //lower friction
	m_GroundControlSpeed = 10.0f; //TODO check this
	m_GroundControlAccel = 50.0f / ms_TicksPerSecond; //low acceleration
	//TODO add sand skid sound
}

float CMatSand::GetSkidThreshold()
{
	return 100.0f;
}

CMatPenalty::CMatPenalty()
{
	m_GroundFriction = 0.3f; //high friction
	m_GroundControlSpeed = 5.0f; //low max speed
	m_GroundControlAccel = -20.0f / ms_TicksPerSecond; //penalty low accell!
	// m_VelrampStart = 0; //begin slow down on contact
	// m_VelrampRange = 200;
	// m_VelrampCurvature = 3.0f;
}

float CMatPenalty::GetSkidThreshold()
{
	return 700.0f; //high friction, late skids
}

float CMatPenalty::GetDynamicAcceleration(float Velocity)
{
	if(Velocity > 2 * abs((float)m_GroundControlAccel))
		return CMatDefault::GetDynamicAcceleration(Velocity);
	//allow for a minimal acceleration on low speed
	return mix(abs((float)m_GroundControlAccel), (float)m_GroundControlAccel, Velocity / (2 * abs((float)m_GroundControlAccel)));
}

float CMatSlimeBase::GetSkidThreshold()
{
	return -1.0f; //slime shouldn't skid
}

CMatSlime::CMatSlime()
{
	m_GroundElasticityX = 1.0f;
	m_GroundElasticityY = 1.0f;
}

CMatSlimeV::CMatSlimeV()
{
	m_GroundElasticityY = 1.0f;
}

CMatSlimeH::CMatSlimeH()
{
	m_GroundElasticityX = 1.0f;
}

CMatSlimeWeak::CMatSlimeWeak()
{
	m_GroundElasticityX = 0.5f;
	m_GroundElasticityY = 0.5f;
}

CMatSlimeWeakV::CMatSlimeWeakV()
{
	m_GroundElasticityY = 0.5f;
}

CMatSlimeWeakH::CMatSlimeWeakH()
{
	m_GroundElasticityX = 0.5f;
}
