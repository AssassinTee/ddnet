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

class CTuneParams
{
public:
	CTuneParams()
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

	static int Num()
	{
		return sizeof(CTuneParams) / sizeof(CTuneParam);
	}
	bool Set(int Index, float Value);
	bool Set(const char *pName, float Value);
	bool Get(int Index, float *pValue) const;
	bool Get(const char *pName, float *pValue) const;

protected:
	constexpr static const float ms_TicksPerSecond = 50.0f;
};

/* Materials ---------------------------------------------------------------------------- */

class CMatDefault
{
public:
	CTuneParams &GetTuning() { return m_TuneParams; }
	CMatDefault() = default;
	virtual int GetSkidSound();
	virtual float GetSkidThreshold() { return 500.0f; }
	virtual float GetDynamicGroundAccel(float Vel, int Dir) { return HandleDirection(m_TuneParams.m_GroundControlAccel, Vel, Dir); }
	virtual float GetDynamicAirAccel(float Vel, int Dir) { return HandleDirection(m_TuneParams.m_AirControlAccel, Vel, Dir); }
	virtual float HandleDirection(float Accel, float Vel, int Dir) { return Dir == 0 ? 0.0f : Dir * Accel; }

protected:
	CTuneParams m_TuneParams;
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
	float GetDynamicGroundAccel(float Vel, int Dir) override;
	float GetSkidThreshold() override;

private:
	float m_PenaltyControlAccel;
	float m_PenaltyMinAccel;
	float m_PenaltyScale;
};

// SLIME

class IMatSlime : virtual public CMatDefault
{
public:
	virtual float GetSkidThreshold() override;
	virtual int GetSkidSound() override { return -1; }

protected:
	IMatSlime() = default;
};

class CMatSlime : public IMatSlime
{
public:
	CMatSlime();
};

class CMatSlimeV : public IMatSlime
{
public:
	CMatSlimeV();
};

class CMatSlimeH : public IMatSlime
{
public:
	CMatSlimeH();
};

class CMatSlimeWeak : public IMatSlime
{
public:
	CMatSlimeWeak();
};

class CMatSlimeWeakV : public IMatSlime
{
public:
	CMatSlimeWeakV();
};

class CMatSlimeWeakH : public IMatSlime
{
public:
	CMatSlimeWeakH();
};

// BOOSTER

class IMatBooster : public CMatDefault
{
public:
	virtual float GetDynamicGroundAccel(float Vel, int Dir) override;
	virtual float HandleDirection(float Accel, float Vel, int Dir) override;

protected:
	enum BoosterDir
	{
		LEFT = -1,
		BIDIRECT,
		RIGHT,
	};

	IMatBooster() = default;
	float m_BoosterMinControlAccel;
	float m_BoosterMaxControlAccel;
	float m_BoosterHighSpeedAccel;
	BoosterDir m_BoosterDir;
};

class CMatBoosterBidirect : virtual public IMatBooster
{
public:
	CMatBoosterBidirect();
};

class CMatBoosterRight : public CMatBoosterBidirect
{
public:
	CMatBoosterRight();
};

class CMatBoosterLeft : public CMatBoosterBidirect
{
public:
	CMatBoosterLeft();
};

class CMatBoosterWeakBidirect : virtual public IMatBooster
{
public:
	CMatBoosterWeakBidirect();
};

class CMatBoosterWeakRight : public CMatBoosterWeakBidirect
{
public:
	CMatBoosterWeakRight();
};

class CMatBoosterWeakLeft : public CMatBoosterWeakBidirect
{
public:
	CMatBoosterWeakLeft();
};

#endif //GAME_MATERIAL_TUNING_H
