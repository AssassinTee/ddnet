//
// Created by Marvin on 05.06.2022.
//

#include "material.h"
#include <base/math.h>

CMaterials::CMaterials()
{
	m_apMaterials = std::vector<CMatDefault *>(
		{
			&m_MatDefault,
			&m_MatIce,
			&m_MatSand,
			&m_MatPenalty,
			&m_MatSlime,
			&m_MatSlimeV,
			&m_MatSlimeH,
			&m_MatSlimeWeak,
			&m_MatSlimeWeakV,
			&m_MatSlimeWeakH,
			&m_MatBoosterBidirect,
			&m_MatBoosterRight,
			&m_MatBoosterLeft,
			&m_MatBoosterWeakBidirect,
			&m_MatBoosterWeakRight,
			&m_MatBoosterWeakLeft,
		});
}

CMaterials::~CMaterials()
{
	m_apMaterials.clear();
}

CMatDefault &CMaterials::operator[](int Index)
{
	switch(Index)
	{
	case MAT_ICE: return *m_apMaterials[1];
	case MAT_SAND: return *m_apMaterials[2];
	case MAT_PENALTY_GRAS: return *m_apMaterials[3];
	case MAT_SLIME: return *m_apMaterials[4];
	case MAT_SLIME_V: return *m_apMaterials[5];
	case MAT_SLIME_H: return *m_apMaterials[6];
	case MAT_SLIME_WEAK: return *m_apMaterials[7];
	case MAT_SLIME_WEAK_V: return *m_apMaterials[8];
	case MAT_SLIME_WEAK_H: return *m_apMaterials[9];
	case MAT_BOOSTER_BIDIRECT: return *m_apMaterials[10];
	case MAT_BOOSTER_RIGHT: return *m_apMaterials[11];
	case MAT_BOOSTER_LEFT: return *m_apMaterials[12];
	case MAT_BOOSTER_WEAK_BIDIRECT: return *m_apMaterials[13];
	case MAT_BOOSTER_WEAK_RIGHT: return *m_apMaterials[14];
	case MAT_BOOSTER_WEAK_LEFT: return *m_apMaterials[15];
	}
	return *m_apMaterials[MAT_DEFAULT];
}

float CMaterials::GetGroundControlSpeed(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight)
{
	return HandleMaterialInteraction(GroundedLeft, GroundedRight, GetTuning(MaterialLeft).m_GroundControlSpeed, GetTuning(MaterialRight).m_GroundControlSpeed, [](float a, float b) { return minimum(a, b); });
}

float CMaterials::GetGroundControlAccel(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight, float Vel, int Dir)
{
	return HandleMaterialInteraction(GroundedLeft, GroundedRight, At(MaterialLeft).GetDynamicGroundAccel(Vel, Dir), At(MaterialRight).GetDynamicGroundAccel(Vel, Dir), [](float a, float b) { return maximum(a, b); });
}

float CMaterials::GetGroundFriction(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight)
{
	// Note: Higher friction VALUE means LOWER physical friction
	return HandleMaterialInteraction(GroundedLeft, GroundedRight, GetTuning(MaterialLeft).m_GroundFriction, GetTuning(MaterialRight).m_GroundFriction, [](float a, float b) { return minimum(a, b); });
}

float CMaterials::GetGroundJumpImpulse(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight)
{
	// you sticked to something with one leg, average it?
	return HandleMaterialInteraction(GroundedLeft, GroundedRight, GetTuning(MaterialLeft).m_GroundJumpImpulse, GetTuning(MaterialRight).m_GroundJumpImpulse, [](float a, float b) { return (a + b) / 2; });
}

float CMaterials::GetElasticityX(bool GroundedTop, bool GroundedBottom, int MaterialTop, int MaterialBottom, float Offset)
{
	return HandleMaterialInteraction(GroundedTop, GroundedBottom, GetTuning(MaterialTop).m_GroundElasticityX, GetTuning(MaterialBottom).m_GroundElasticityX, [Offset](float a, float b) { return Offset > 0.5 ? a : b; });
}

float CMaterials::GetElasticityY(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight, float Offset)
{
	return HandleMaterialInteraction(GroundedLeft, GroundedRight, GetTuning(MaterialLeft).m_GroundElasticityY, GetTuning(MaterialRight).m_GroundElasticityY, [Offset](float a, float b) { return Offset > 0.5 ? a : b; });
}

float CMaterials::HandleMaterialInteraction(bool GroundedLeft, bool GroundedRight, float ValueLeft, float ValueRight, const std::function<float(float, float)> &function)
{
	if(GroundedLeft && !GroundedRight) //standing on the left edge
		return ValueLeft;
	else if(GroundedRight && !GroundedLeft) //standing on the right edge
		return ValueRight;
	else
		return function(ValueLeft, ValueRight); //standing on two materials
}
