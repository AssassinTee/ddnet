//
// Created by Marvin on 05.06.2022.
//

#ifndef GAME_MATERIAL_H
#define GAME_MATERIAL_H

#include <base/system.h>
#include <functional>
#include <vector>

class CTuneParam
{
	int m_Value;

public:
	void Set(int v) { m_Value = v; }
	int Get() const { return m_Value; }
	CTuneParam &operator=(int v)
	{
		m_Value = (int)(v * 100.0f);
		return *this;
	}
	CTuneParam &operator=(float v)
	{
		m_Value = (int)(v * 100.0f);
		return *this;
	}
	operator float() const { return m_Value / 100.0f; }
};

/* Materials ---------------------------------------------------------------------------- */

class CMatDefault //Note: this was CTuneParams before
{
public:
	CMatDefault()
	{
		const float TicksPerSecond = 50.0f;
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) m_##Name.Set((int)(Value * 100.0f));
#include "tuning.h"
#undef MACRO_TUNING_PARAM
	}

	static const char *ms_apNames[];

#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) CTuneParam m_##Name;
#include "tuning.h"
#undef MACRO_TUNING_PARAM

	static int Num()
	{
		return sizeof(CMatDefault) / sizeof(CTuneParam);
	}
	bool Set(int Index, float Value);
	bool Set(const char *pName, float Value);
	bool Get(int Index, float *pValue) const;
	bool Get(const char *pName, float *pValue) const;
};

class CMatPlaceholder : public CMatDefault
{
public:
	CMatPlaceholder()
	{
		m_GroundFriction = 0.99f;
		m_Gravity = 0.1f;
	}
};

/* Material handling ---------------------------------------------------------------------------- */

class CMaterials
{
public:
	CMaterials() = default;
	CMatDefault &operator[](int Index) const;
	CMatDefault *Tuning() { return const_cast<CMatDefault *>(&ms_aMaterials[0]); }

	//multi material interactions (ground)
	float GetGroundControlSpeed(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight);
	float GetGroundControlAccel(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight);
	float GetGroundFriction(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight);
	float GetGroundJumpImpulse(bool GroundedLeft, bool GroundedRight, int MaterialLeft, int MaterialRight);

private:
	float HandleMaterialInteraction(bool GroundedLeft, bool GroundedRight, float ValueLeft, float ValueRight, const std::function<float(float, float)> &function);
	static const inline std::vector<CMatDefault> ms_aMaterials{
		CMatDefault(),
		CMatPlaceholder(),
	};
};

#endif //GAME_MATERIAL_H
