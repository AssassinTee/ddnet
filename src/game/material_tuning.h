//
// Created by Marvin on 16.06.2022.
//

#ifndef GAME_MATERIAL_TUNING_H
#define GAME_MATERIAL_TUNING_H

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
		const float TicksPerSecond = ms_TicksPerSecond;
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) m_##Name.Set((int)(Value * 100.0f));
#include "tuning.h"
#undef MACRO_TUNING_PARAM
	}

	static const char *ms_apNames[];

#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) CTuneParam m_##Name;
#include "tuning.h"
#undef MACRO_TUNING_PARAM

	// client side material values
	virtual int GetSkidSound();
	virtual float GetSkidThreshold() { return 500.0f; }
	virtual float GetDynamicAcceleration(float Velocity) { return m_GroundControlAccel; }

	static int Num()
	{
		return sizeof(CMatDefault) / sizeof(CTuneParam);
	}
	bool Set(int Index, float Value);
	bool Set(const char *pName, float Value);
	bool Get(int Index, float *pValue) const;
	bool Get(const char *pName, float *pValue) const;

protected:
	constexpr static const float ms_TicksPerSecond = 50.0f;
};

class CMatIce : public CMatDefault
{
public:
	CMatIce();
	int GetSkidSound() override { return -1; }
	float GetSkidThreshold() override;
};

class CMatSand : public CMatDefault
{
public:
	CMatSand();
	float GetSkidThreshold() override;
};

class CMatPenalty : public CMatDefault
{
public:
	CMatPenalty();
	float GetDynamicAcceleration(float Velocity) override;
	float GetSkidThreshold() override;
};

class CMatSlimeBase : public CMatDefault
{
public:
	CMatSlimeBase() = default;
	virtual float GetSkidThreshold() override;
	virtual int GetSkidSound() override { return -1; }
};

class CMatSlime : public CMatSlimeBase
{
public:
	CMatSlime();
};

class CMatSlimeV : public CMatSlimeBase
{
public:
	CMatSlimeV();
};

class CMatSlimeH : public CMatSlimeBase
{
public:
	CMatSlimeH();
};

class CMatSlimeWeak : public CMatSlimeBase
{
public:
	CMatSlimeWeak();
};

class CMatSlimeWeakV : public CMatSlimeBase
{
public:
	CMatSlimeWeakV();
};

class CMatSlimeWeakH : public CMatSlimeBase
{
public:
	CMatSlimeWeakH();
};

#endif //GAME_MATERIAL_TUNING_H
