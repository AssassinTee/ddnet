//
// Created by Marvin on 05.06.2022.
//

#ifndef GAME_MATERIAL_H
#define GAME_MATERIAL_H

#include "mapitems.h"
#include "material_tuning.h"
#include <base/system.h>
#include <functional>
#include <vector>


class CMaterials
{
public:
	~CMaterials();

	CMaterials(CMaterials const &) = delete;
	CMaterials &operator=(CMaterials const &) = delete;

	//TODO split this into Get and Set
	CMatDefault &At(int Index) { return (*this)[Index]; }
	CMatDefault *Tuning() { return m_apMaterials[MAT_DEFAULT]; }

	//multi material interactions (ground)
	float GetGroundControlSpeed(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight);
	float GetGroundControlAccel(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight);
	float GetGroundFriction(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight);
	float GetGroundJumpImpulse(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight);
	float GetElasticityX(bool GroundedTop, bool GroundedBottom, int MaterialTop, int MaterialBottom, float Offset);
	float GetElasticityY(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight, float Offset);

	static CMaterials *GetInstance()
	{
		static CMaterials instance;
		return &instance;
	}

private:
	CMaterials();
	CMatDefault &operator[](int Index);
	float HandleMaterialInteraction(bool GroundedLeft, bool GroundedRight, float ValueLeft, float ValueRight, const std::function<float(float, float)> &function);
	const std::vector<CMatDefault *> m_apMaterials;
};

#endif //GAME_MATERIAL_H
