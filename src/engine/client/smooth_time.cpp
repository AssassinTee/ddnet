/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "smooth_time.h"

#include "graph.h"

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <cmath>

/**
 * @file
 * @brief Implementation of enhanced CSmoothTime with Kalman filter
 *
 * =============================== KALMAN FILTER ===============================
 *
 * The Kalman filter is an optimal recursive estimator for linear systems
 * with Gaussian noise. It maintains a state estimate and uncertainty,
 * updating both as new measurements arrive.
 *
 * State Model:
 *   x[k] = F * x[k-1] + w[k]
 *   z[k] = H * x[k] + v[k]
 *
 * Where:
 *   - x = state vector [time, drift_rate]
 *   - F = state transition matrix (assumes constant drift)
 *   - H = measurement matrix (observes only time)
 *   - w = process noise (clock instability)
 *   - v = measurement noise (network jitter)
 *
 * Prediction Step:
 *   x_pred = F * x_est
 *   P_pred = F * P_est * F' + Q
 *
 * Update Step:
 *   K = P_pred * H' / (H * P_pred * H' + R)
 *   x_est = x_pred + K * (z - H * x_pred)
 *   P_est = (I - K * H) * P_pred
 *
 * =========================== JITTER BUFFER ===============================
 *
 * Maintains rolling statistics of network delay to adapt to conditions.
 *
 * Rolling Mean:
 *   mean[k] = Decay * mean[k-1] + (1-Decay) * sample
 *
 * Rolling Variance:
 *   var[k] = Decay * (var[k-1] + Decay * (sample - mean[k-1])^2)
 *
 * Adaptive Threshold:
 *   threshold = mean + N * Stddev
 *
 * ========================= RTT ADAPTATION ================================
 *
 * Uses RTT to scale spike thresholds:
 *   threshold_ms = max(DEFAULT_THRESHOLD, RTT * SCALE)
 *
 * Higher RTT = larger acceptable variance (more lenient)
 * Lower RTT = tighter thresholds (more sensitive)
 *
 * Kalman Filter (2D case):
 *
 * State: x = [time_estimate, drift_rate]'
 * Covariance: P = [[P_time, P_cross], [P_cross, P_drift]]
 *
 * Prediction:
 *   TimePred = time_est + drift_est
 *   PTimePred = P_time + 2*P_cross + P_drift + Q_time
 *   PCrossPred = P_cross + P_drift
 *   P_DriftPred = P_drift + Q_drift
 *
 * Update:
 *   Innovation = measurement - TimePred
 *   S = PTimePred + R
 *   K_time = PTimePred / S
 *   K_drift = (PCrossPred) / S
 *
 *   time_est = TimePred + K_time * Innovation
 *   drift_est = DriftPred + K_drift * Innovation
 *
 *   P_time_new = (1 - K_time) * PTimePred
 *   P_cross_new = (1 - K_time) * PCrossPred
 *   P_drift_new = P_DriftPred - K_drift * PCrossPred
 *
 * @see CSmoothTime for usage
 */

void CSmoothTime::Init(int64_t Target, int RTTms)
{
	// Initialize legacy state
	m_Snap = time_get();
	m_Current = Target;
	m_Target = Target;
	m_Margin = 0;
	m_SpikeCounter = 0;
	m_AdjustSpeed[ADJUSTDIRECTION_DOWN] = CTimeConfig::BASE_ADJUST_SPEED;
	m_AdjustSpeed[ADJUSTDIRECTION_UP] = CTimeConfig::BASE_ADJUST_SPEED;

	// Initialize Kalman filter
	InitKalmanFilter(Target);

	// Initialize jitter buffer
	m_JitterMean = 0.0f;
	m_JitterVariance = 0.0f;
	m_SampleCount = 0.0f;
	m_JitterWriteIndex = 0;

	// Initialize per-direction jitter
	for(int i = 0; i < NUM_ADJUSTDIRECTIONS; i++)
	{
		m_DirJitterMean[i] = 0.0f;
		m_DirJitterVar[i] = 0.0f;
		m_DirSampleCount[i] = 0.0f;
	}

	// Initialize RTT tracking
	m_RTTAverage = 0.0f;
	m_RTTCount = 0.0f;
	m_LastRTT = 0.0f;

	if(RTTms > 0)
	{
		UpdateRTT(static_cast<float>(RTTms));
	}

	m_FilterWarmup = true;
}

void CSmoothTime::InitKalmanFilter(int64_t Target)
{
	// Initialize Kalman filter state
	m_TimeEstimate = static_cast<float>(Target);
	m_DriftRate = 0.0f;
	m_TimeErrorVar = CTimeConfig::INITIAL_ERROR_VAR;
	m_DriftErrorVar = CTimeConfig::DRIFT_VAR;
	m_Covariance = 0.0f;
	m_PreviousTime = static_cast<float>(Target);
	m_FilterWarmup = true;
}

void CSmoothTime::SetAdjustSpeed(EAdjustDirection Direction, float Value)
{
	m_AdjustSpeed[static_cast<int>(Direction)] = Value;
}

int64_t CSmoothTime::Get(int64_t Now) const
{
	// Use Kalman filter prediction for better accuracy
	int64_t Result;

	if(!m_FilterWarmup)
	{
		// Calculate predicted time using Kalman filter
		int64_t SnapTime = m_Snap;
		int64_t TimeUnitsNow = Now - SnapTime;

		// Predict: time = estimate + drift_rate * elapsed_ticks
		float PredictedTime = m_TimeEstimate + m_DriftRate * static_cast<float>(TimeUnitsNow);
		Result = static_cast<int64_t>(PredictedTime);
	}
	else
	{
		// Fallback to legacy interpolation during warmup
		int64_t ProjectedCurrent = m_Current + (Now - m_Snap);
		int64_t ProjectedTarget = m_Target + (Now - m_Snap);

		float AdjustmentSpeed = m_AdjustSpeed[ADJUSTDIRECTION_DOWN];
		if(ProjectedTarget > ProjectedCurrent)
		{
			AdjustmentSpeed = m_AdjustSpeed[ADJUSTDIRECTION_UP];
		}

		float ElapsedSeconds = static_cast<float>(Now - m_Snap) / static_cast<float>(time_freq());
		float Alpha = ElapsedSeconds * AdjustmentSpeed;
		if(Alpha > 1.0f)
		{
			Alpha = 1.0f;
		}

		Result = ProjectedCurrent + static_cast<int64_t>((ProjectedTarget - ProjectedCurrent) * Alpha);
	}

	return Result + m_Margin;
}

void CSmoothTime::UpdateWithRTT(CGraph *pGraph, int64_t Target, int64_t LocalNow, EAdjustDirection AdjustDirection)
{
	// Calculate RTT: assume symmetric network, RTT = 2 * (LocalNow - ServerTime)
	// This assumes clocks are roughly synchronized

	// For simplicity, we use the time Difference as an RTT estimate
	// In practice, you'd want proper clock synchronization
	int64_t TimeDiff = LocalNow - Target;
	int RTTms = static_cast<int>(TimeToMs(TimeDiff * 2));

	Update(pGraph, Target, static_cast<int>(TimeToMs(LocalNow - Target)), AdjustDirection, RTTms);
}

void CSmoothTime::Update(CGraph *pGraph, int64_t Target, int TimeLeft, EAdjustDirection AdjustDirection, int RTTms)
{
	bool UpdateTimer = true;

	// === Update RTT if provided ===
	if(RTTms >= 0)
	{
		UpdateRTT(static_cast<float>(RTTms));
	}

	// === Kalman Filter Update ===
	if(!m_FilterWarmup)
	{
		// Convert TimeLeft to time units and apply as measurement correction
		// TimeLeft < 0 means server is behind, need to increase estimate
		// TimeLeft > 0 means server is ahead, need to decrease estimate
		float MeasurementCorrection = static_cast<float>(TimeLeft) / static_cast<float>(time_freq());

		UpdateKalman(MeasurementCorrection, time_get());
	}

	// === Update Jitter Buffer ===
	UpdateJitterBuffer(static_cast<float>(TimeLeft), AdjustDirection);

	// === Adaptive Threshold Calculation ===
	float AdaptiveThreshold = GetAdaptiveSpikeThreshold(AdjustDirection);
	bool IsSpike = false;

	if(TimeLeft < 0 && TimeLeft < AdaptiveThreshold)
	{
		IsSpike = true;
		m_SpikeCounter += CTimeConfig::SPIKE_WEIGHT;
		if(m_SpikeCounter > CTimeConfig::SPIKE_COUNTER_MAX)
		{
			m_SpikeCounter = CTimeConfig::SPIKE_COUNTER_MAX;
		}
	}

	// Spike Decision
	if(IsSpike && m_SpikeCounter < CTimeConfig::SPIKE_IGNORE_THRESHOLD)
	{
		UpdateTimer = false;

		if(pGraph)
		{
			pGraph->Add(static_cast<float>(TimeLeft), ColorRGBA(1.0f, 1.0f, 0.0f, 0.75f));
		}
	}
	else
	{
		if(pGraph)
		{
			// Color based on severity
			if(IsSpike)
			{
				pGraph->Add(static_cast<float>(TimeLeft), ColorRGBA(1.0f, 0.0f, 0.0f, 0.75f));

				// Increase Speed for confirmed spikes
				float &Speed = m_AdjustSpeed[static_cast<int>(AdjustDirection)];
				if(Speed < CTimeConfig::MAX_ADJUST_SPEED)
				{
					Speed *= CTimeConfig::BAD_ADJUST_MULTIPLIER;
				}
			}
			else
			{
				pGraph->Add(static_cast<float>(TimeLeft), ColorRGBA(0.0f, 1.0f, 0.0f, 0.75f));

				// Decay Speed on good updates
				float &Speed = m_AdjustSpeed[static_cast<int>(AdjustDirection)];
				Speed *= CTimeConfig::GOOD_ADJUST_MULTIPLIER;

				if(Speed < CTimeConfig::MIN_ADJUST_SPEED)
				{
					Speed = CTimeConfig::MIN_ADJUST_SPEED;
				}

				// Decay spike counter
				if(m_SpikeCounter > 0)
				{
					m_SpikeCounter--;
				}
			}
		}
		else if(IsSpike)
		{
			// Speed increase without logging
			float &Speed = m_AdjustSpeed[static_cast<int>(AdjustDirection)];
			if(Speed < CTimeConfig::MAX_ADJUST_SPEED)
			{
				Speed *= CTimeConfig::BAD_ADJUST_MULTIPLIER;
			}
		}
		else if(m_SpikeCounter > 0)
		{
			// Decay without logging
			m_SpikeCounter = static_cast<int>(m_SpikeCounter * CTimeConfig::SPIKE_DECAY);
		}
	}

	// === Legacy State Update ===
	if(UpdateTimer)
	{
		UpdateInt(Target);
	}

	// === Check Filter Warmup ===
	if(IsFilterWarm(time_get()))
	{
		m_FilterWarmup = false;
	}
}

void CSmoothTime::UpdateMargin(int64_t Margin)
{
	m_Margin = Margin;
}

float CSmoothTime::GetAdjustSpeed(EAdjustDirection Direction) const
{
	return m_AdjustSpeed[static_cast<int>(Direction)];
}

void CSmoothTime::UpdateKalman(float MeasurementValue, int64_t NowTime)
{
	// ========================================
	// PREDICTION STEP
	// ========================================

	// State transition: x = [time, drift_rate]
	// F = [[1, 1], [0, 1]] (constant velocity model)

	// Predicted state
	float TimePred = m_TimeEstimate + m_DriftRate;
	float DriftPred = m_DriftRate;

	// Predicted covariance
	// P_pred = F * P * F' + Q
	// Where Q = [[Q_time, 0], [0, Q_drift]] (process noise)
	float PTimePred = m_TimeErrorVar + 2.0f * m_Covariance + m_DriftErrorVar + CTimeConfig::PROCESS_NOISE_VAR;
	float PCrossPred = m_Covariance + m_DriftErrorVar;
	float PDriftPred = m_DriftErrorVar + CTimeConfig::DRIFT_VAR;

	// ========================================
	// UPDATE STEP
	// ========================================

	// Measurement model: z = H * x + v, where H = [1, 0]
	// Only observes time, not drift rate

	// Measurement noise (adapted from jitter buffer)
	float MeasurementNoiseVar = CalculateMeasurementNoiseVar();

	// Innovation (measurement residual)
	float Innovation = MeasurementValue - TimePred;

	// Innovation covariance
	// S = H * P_pred * H' + R = PTimePred + R
	float InnovationVar = PTimePred + MeasurementNoiseVar;

	// Kalman gain
	// K = P_pred * H' / S
	float KalmanGainTime = PTimePred / InnovationVar;
	float KalmanGainDrift = PCrossPred / InnovationVar;

	// Clip gains for stability
	if(KalmanGainTime < CTimeConfig::MIN_KALMAN_GAIN)
	{
		KalmanGainTime = CTimeConfig::MIN_KALMAN_GAIN;
	}
	if(KalmanGainTime > CTimeConfig::MAX_KALMAN_GAIN)
	{
		KalmanGainTime = CTimeConfig::MAX_KALMAN_GAIN;
	}
	if(KalmanGainDrift < 0.0f)
	{
		KalmanGainDrift = 0.0f;
	}
	if(KalmanGainDrift > 0.1f)
	{
		KalmanGainDrift = 0.1f;
	}

	// Update state
	m_TimeEstimate = TimePred + KalmanGainTime * Innovation;
	m_DriftRate = DriftPred + KalmanGainDrift * Innovation;

	// Update covariance
	// P_new = (I - K * H) * P_pred
	// I - K*H = [[1 - K_time, 0], [-K_drift, 1]]
	m_TimeErrorVar = (1.0f - KalmanGainTime) * PTimePred;
	m_Covariance = (1.0f - KalmanGainTime) * PCrossPred - KalmanGainDrift * PTimePred;
	m_DriftErrorVar = PDriftPred - KalmanGainDrift * PCrossPred;

	// Ensure covariance matrix remains positive definite
	if(m_TimeErrorVar < 0.01f)
	{
		m_TimeErrorVar = 0.01f;
	}
	if(m_DriftErrorVar < 0.001f)
	{
		m_DriftErrorVar = 0.001f;
	}

	// Store previous time for drift calculation reference
	m_PreviousTime = m_TimeEstimate;
}

float CSmoothTime::CalculateMeasurementNoiseVar() const
{
	// Use jitter variance as measurement noise estimate
	// Scale based on RTT for adaptive behavior

	float BaseNoise = m_JitterVariance;

	if(m_RTTAverage > 0.0f)
	{
		// Scale noise with RTT: higher RTT = more variance expected
		float RttScale = 1.0f + m_RTTAverage * 0.01f;
		BaseNoise *= RttScale;
	}

	// Clamp to configured bounds
	if(BaseNoise < CTimeConfig::MIN_MEASUREMENT_VAR)
	{
		BaseNoise = CTimeConfig::MIN_MEASUREMENT_VAR;
	}
	if(BaseNoise > CTimeConfig::MAX_MEASUREMENT_VAR)
	{
		BaseNoise = CTimeConfig::MAX_MEASUREMENT_VAR;
	}

	return BaseNoise;
}

void CSmoothTime::UpdateJitterBuffer(float TimeLeft, EAdjustDirection DirectionIdx)
{
	// Update global jitter statistics
	float Decay = CTimeConfig::ROLLING_DECAY;

	if(m_SampleCount < 1.0f)
	{
		// First sample
		m_JitterMean = TimeLeft;
		m_JitterVariance = 0.0f;
		m_SampleCount = 1.0f;
	}
	else
	{
		// Rolling mean
		float OldMean = m_JitterMean;
		m_JitterMean = Decay * OldMean + (1.0f - Decay) * TimeLeft;

		// Rolling variance (Welford's algorithm adapted for Decay)
		float Diff = TimeLeft - OldMean;
		m_JitterVariance = Decay * (m_JitterVariance + (1.0f - Decay) * Diff * Diff);

		// Increment sample count with Decay
		m_SampleCount = Decay * m_SampleCount + 1.0f;

		// Circular buffer update
		m_JitterBuffer[m_JitterWriteIndex] = TimeLeft;
		m_JitterWriteIndex = (m_JitterWriteIndex + 1) % CTimeConfig::JITTER_HISTORY_SIZE;
	}

	// Update per-direction statistics (more granular adaptation)
	if(m_DirSampleCount[DirectionIdx] < 1.0f)
	{
		m_DirJitterMean[DirectionIdx] = TimeLeft;
		m_DirJitterVar[DirectionIdx] = 0.0f;
		m_DirSampleCount[DirectionIdx] = 1.0f;
	}
	else
	{
		float OldMean = m_DirJitterMean[DirectionIdx];
		m_DirJitterMean[DirectionIdx] = Decay * OldMean + (1.0f - Decay) * TimeLeft;

		float Diff = TimeLeft - OldMean;
		m_DirJitterVar[DirectionIdx] = Decay * (m_DirJitterVar[DirectionIdx] + (1.0f - Decay) * Diff * Diff);

		m_DirSampleCount[DirectionIdx] = Decay * m_DirSampleCount[DirectionIdx] + 1.0f;
	}

	// Check if filter can be considered warm
	if(m_SampleCount >= CTimeConfig::WARMUP_SAMPLES)
	{
		m_FilterWarmup = false;
	}
}

float CSmoothTime::GetAdaptiveSpikeThreshold(EAdjustDirection DirectionIdx) const
{
	// Use per-direction jitter statistics if available
	float DirStddev = sqrtf(m_DirJitterVar[DirectionIdx]);
	float GlobalStddev = sqrtf(m_JitterVariance);

	// Use direction-specific if we have enough samples
	float Stddev = m_DirSampleCount[DirectionIdx] >= 5.0f ? DirStddev : GlobalStddev;

	// Calculate adaptive threshold using 3σ rule
	float AdaptiveThreshold = Stddev * CTimeConfig::SPIKE_STDDEV_MULTIPLIER;

	// Apply RTT-based scaling
	AdaptiveThreshold = fmaxf(AdaptiveThreshold, GetRTTThreshold());

	// Apply minimum threshold (default if no statistics)
	if(m_SampleCount < CTimeConfig::WARMUP_SAMPLES)
	{
		AdaptiveThreshold = fmaxf(AdaptiveThreshold, static_cast<float>(CTimeConfig::DEFAULT_SPIKE_THRESHOLD_MS));
	}

	return -AdaptiveThreshold; // Negative because we check TimeLeft < threshold
}

void CSmoothTime::UpdateRTT(float RttMs)
{
	if(m_RTTCount < 1.0f)
	{
		// First RTT sample
		m_RTTAverage = RttMs;
		m_RTTCount = 1.0f;
		m_LastRTT = RttMs;
	}
	else
	{
		// Sanity check
		if(RttMs < 0.0f || RttMs > CTimeConfig::MAX_RTT_MS)
		{
			return; // Ignore invalid samples
		}

		// Running average with Decay
		float Decay = CTimeConfig::RTT_DECAY;
		m_RTTAverage = Decay * m_RTTAverage + (1.0f - Decay) * RttMs;
		m_RTTCount = Decay * m_RTTCount + 1.0f;
		m_LastRTT = RttMs;
	}
}

float CSmoothTime::GetRTTThreshold() const
{
	// RTT-based minimum threshold
	if(m_RTTCount >= CTimeConfig::RTT_MIN_SAMPLES)
	{
		return m_RTTAverage * CTimeConfig::RTT_THRESHOLD_SCALE;
	}

	// Default if not enough samples
	return static_cast<float>(CTimeConfig::DEFAULT_SPIKE_THRESHOLD_MS);
}

int64_t CSmoothTime::MsToTime(int64_t TimeMs) const
{
	return TimeMs * time_freq() / 1000;
}

int64_t CSmoothTime::TimeToMs(int64_t TimeValue) const
{
	return TimeValue * 1000 / time_freq();
}

bool CSmoothTime::IsFilterWarm(int64_t NowTime) const
{
	// Filter is warm if we have enough samples AND time has passed
	// This prevents premature adaptation
	return m_SampleCount >= CTimeConfig::WARMUP_SAMPLES &&
	       (NowTime - m_Snap) > time_freq() / 10; // At least 100ms since init
}

void CSmoothTime::UpdateInt(int64_t Target)
{
	// Get current timestamp
	int64_t Now = time_get();

	// Calculate smoothed time at this moment
	m_Current = Get(Now) - m_Margin;

	// Reset snapshot point
	m_Snap = Now;

	// Update target for next interpolation
	m_Target = Target;
}
