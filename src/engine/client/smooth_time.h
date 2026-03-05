/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef ENGINE_CLIENT_SMOOTH_TIME_H
#define ENGINE_CLIENT_SMOOTH_TIME_H

#include <base/math.h>
#include <base/vmath.h>

#include <cmath>
#include <cstdint>

class CGraph;

/**
 * @brief Enhanced smooth time synchronization with Kalman filter
 *
 * This class provides superior time synchronization using:
 * 1. Kalman Filter - Optimal state estimation for time and drift rate
 * 2. Adaptive Jitter Buffer - Dynamic smoothing based on network variance
 * 3. RTT-informed thresholds - Scales based on actual network conditions
 * 4. Dual-mode operation - Fast response for good networks, robust for bad
 *
 * Kalman Filter State Model:
 * - State vector: [time_estimate, drift_rate]
 * - Time step: 1 tick (predicts next time and drift)
 * - Measurement: server timestamp with network noise
 * - Automatically balances trust between prediction and measurement
 *
 * Jitter Buffer Model:
 * - Tracks rolling statistics (mean, stddev) of recent samples
 * - Sets adaptive thresholds: spike = mean + 3*stddev
 * - Scales smoothing factor based on jitter variance
 * - Uses RTT to normalize thresholds
 *
 * Key Features:
 * - Handles jittery WAN connections gracefully
 * - Maintains low latency on clean connections
 * - Adapts to varying network conditions automatically
 * - Predicts drift rate for better accuracy
 * - Compatible with existing API
 *
 * @see https://en.wikipedia.org/wiki/Kalman_filter
 */
class CSmoothTime
{
public:
	/**
	 * @brief Direction of time adjustment needed
	 */
	enum EAdjustDirection
	{
		ADJUSTDIRECTION_DOWN = 0,
		ADJUSTDIRECTION_UP,
		NUM_ADJUSTDIRECTIONS
	};

	/**
	 * @brief Configuration constants
	 *
	 * These values are tuned for mixed network conditions.
	 * They control the Kalman filter behavior and jitter buffer.
	 */
	struct CTimeConfig
	{
	public:
		// === Kalman Filter Parameters ===
		/** Process noise variance (clock instability) - higher = trust measurements more */
		static constexpr float PROCESS_NOISE_VAR = 0.1f;

		/** Initial estimate error variance */
		static constexpr float INITIAL_ERROR_VAR = 1000.0f;

		/** Initial drift rate estimate variance */
		static constexpr float DRIFT_VAR = 0.01f;

		/** Minimum measurement noise (network jitter baseline) */
		static constexpr float MIN_MEASUREMENT_VAR = 1.0f;

		/** Maximum measurement noise (to prevent over-smoothing) */
		static constexpr float MAX_MEASUREMENT_VAR = 100.0f;

		/** Minimum Kalman gain (never trust measurement completely) */
		static constexpr float MIN_KALMAN_GAIN = 0.1f;

		/** Maximum Kalman gain (can fully trust stable measurements) */
		static constexpr float MAX_KALMAN_GAIN = 0.9f;

		// === Jitter Buffer Parameters ===
		/** Number of samples for rolling statistics */
		static constexpr int JITTER_HISTORY_SIZE = 32;

		/** Spike threshold multiplier (3σ rule for Gaussian) */
		static constexpr float SPIKE_STDDEV_MULTIPLIER = 3.0f;

		/** Minimum samples before adaptive thresholds kick in */
		static constexpr int WARMUP_SAMPLES = 10;

		/** Decay factor for rolling statistics (0-1) */
		static constexpr float ROLLING_DECAY = 0.95f;

		/** RTT-based threshold scaling factor */
		static constexpr float RTT_THRESHOLD_SCALE = 0.5f;

		// === Speed Control ===
		/** Base adjustment speed */
		static constexpr float BASE_ADJUST_SPEED = 0.5f;

		/** Minimum adjustment speed */
		static constexpr float MIN_ADJUST_SPEED = 2.0f;

		/** Maximum adjustment speed */
		static constexpr float MAX_ADJUST_SPEED = 30.0f;

		/** Speed multiplier on bad updates */
		static constexpr float BAD_ADJUST_MULTIPLIER = 2.0f;

		/** Speed multiplier on good updates */
		static constexpr float GOOD_ADJUST_MULTIPLIER = 0.95f;

		// === Adaptive Thresholds ===
		/** Default spike threshold when no history available */
		static constexpr int DEFAULT_SPIKE_THRESHOLD_MS = -50;

		/** Spike counter weight */
		static constexpr int SPIKE_WEIGHT = 5;

		/** Maximum spike counter */
		static constexpr int SPIKE_COUNTER_MAX = 50;

		/** Spike ignore threshold */
		static constexpr int SPIKE_IGNORE_THRESHOLD = 15;

		/** Spike counter decay */
		static constexpr float SPIKE_DECAY = 0.95f;

		// === RTT Handling ===
		/** RTT update decay factor */
		static constexpr float RTT_DECAY = 0.9f;

		/** Minimum RTT sample count for reliable average */
		static constexpr int RTT_MIN_SAMPLES = 5;

		/** Maximum RTT value (sanity check) */
		static constexpr int MAX_RTT_MS = 5000;
	};

private:
	// === Kalman Filter State ===

	/** Current time estimate */
	float m_TimeEstimate = 0.0f;

	/** Estimated clock drift rate (time units per tick) */
	float m_DriftRate = 0.0f;

	/** Estimate error variance for time */
	float m_TimeErrorVar = 0.0f;

	/** Estimate error variance for drift rate */
	float m_DriftErrorVar = 0.0f;

	/** Covariance between time and drift (cross-term) */
	float m_Covariance = 0.0f;

	/** Previous time estimate (for drift calculation) */
	float m_PreviousTime = 0.0f;

	// === Jitter Buffer State ===

	/** Rolling mean of TimeLeft values */
	float m_JitterMean = 0.0f;

	/** Rolling variance of TimeLeft values */
	float m_JitterVariance = 0.0f;

	/** Rolling sample count for statistics */
	float m_SampleCount = 0.0f;

	/** Buffer for recent TimeLeft values (circular) */
	float m_JitterBuffer[CTimeConfig::JITTER_HISTORY_SIZE] = {0.0f};

	/** Current write index in jitter buffer */
	int m_JitterWriteIndex = 0;

	// === RTT Tracking ===

	/** Running average RTT in milliseconds */
	float m_RTTAverage = 0.0f;

	/** RTT sample count */
	float m_RTTCount = 0.0f;

	/** Last measured RTT */
	float m_LastRTT = 0.0f;

	// === Legacy State (for compatibility) ===

	/** Timestamp of last snapshot */
	int64_t m_Snap = 0;

	/** Current interpolated time value */
	int64_t m_Current = 0;

	/** Target time from server */
	int64_t m_Target = 0;

	/** Additional margin */
	int64_t m_Margin = 0;

	/** Spike counter */
	int m_SpikeCounter = 0;

	/** Adjustment speeds (UP/DOWN) */
	vec2 m_AdjustSpeed = vec2(0.0f, 0.0f);

	/** Filter warmup status */
	bool m_FilterWarmup = true;

	/** Direction-specific warmup (per-direction jitter tracking) */
	vec2 m_DirJitterMean = vec2(0.0f, 0.0f);
	vec2 m_DirJitterVar = vec2(0.0f, 0.0f);
	vec2 m_DirSampleCount = vec2(0.0f, 0.0f);

public:
	// === Initialization ===

	/**
	 * @brief Initialize with target time and RTT estimate
	 *
	 * @param Target Initial target time
	 * @param RTTms Optional RTT estimate in ms (0 = auto-detect)
	 */
	void Init(int64_t Target, int RTTms = 0);

	/**
	 * @brief Initialize Kalman filter state
	 *
	 * Sets up the Kalman filter with initial values based on
	 * the target time and estimated RTT.
	 */
	void InitKalmanFilter(int64_t Target);

	/**
	 * @brief Set adjustment speed for a direction
	 * @param Direction UP or DOWN direction
	 * @param Value Speed value
	 */
	void SetAdjustSpeed(EAdjustDirection Direction, float Value);

	// === Core API ===

	/**
	 * @brief Get current smoothed time value
	 * @param Now Current timestamp
	 * @return Smoothed time including margin
	 */
	int64_t Get(int64_t Now) const;

	/**
	 * @brief Update with spike detection, jitter analysis, and Kalman filter
	 * @param pGraph Graph for logging (nullptr if no logging)
	 * @param Target New target time from server
	 * @param TimeLeft Time difference in milliseconds
	 * @param AdjustDirection Which direction time is moving
	 * @param RTTms Optional RTT value in ms (-1 = don't update RTT)
	 */
	void Update(CGraph *pGraph, int64_t Target, int TimeLeft, EAdjustDirection AdjustDirection, int RTTms = -1);

	/**
	 * @brief Update with automatic RTT detection
	 *
	 * RTT is calculated as (LocalNow - ServerTime) * 2 assuming
	 * symmetric network path. This is the preferred method when
	 * server provides synchronized clocks.
	 *
	 * @param pGraph Graph for logging
	 * @param Target Server time
	 * @param LocalNow Local timestamp when update received
	 * @param AdjustDirection Direction
	 */
	void UpdateWithRTT(CGraph *pGraph, int64_t Target, int64_t LocalNow, EAdjustDirection AdjustDirection);

	/**
	 * @brief Set prediction margin
	 * @param Margin Margin value
	 */
	void UpdateMargin(int64_t Margin);

	// === Debug/Query Methods ===

	/** Get Kalman filter time estimate (for debugging) */
	float GetKalmanTimeEstimate() const { return m_TimeEstimate; }

	/** Get estimated drift rate (time units per tick) */
	float GetDriftRate() const { return m_DriftRate; }

	/** Get current jitter mean (ms) */
	float GetJitterMean() const { return m_JitterMean; }

	/** Get current jitter stddev (ms) */
	float GetJitterStddev() const { return std::sqrt(m_JitterVariance); }

	/** Get current RTT average (ms) */
	float GetRTTAverage() const { return m_RTTAverage; }

	/** Get spike counter value */
	int GetSpikeCounter() const { return m_SpikeCounter; }

	/** Get current adjustment speed */
	float GetAdjustSpeed(EAdjustDirection Direction) const;

	/** Check if filter is warm (using adaptive thresholds) */
	bool IsWarm() const { return !m_FilterWarmup; }

	/** Check if time is stable */
	bool IsStable() const { return m_SpikeCounter < CTimeConfig::SPIKE_IGNORE_THRESHOLD; }

	/** Get current measurement noise variance (Kalman trust metric) */
	float GetMeasurementNoiseVar() const { return m_JitterVariance * CTimeConfig::SPIKE_STDDEV_MULTIPLIER; }

private:
	// === Internal Helper Functions ===

	/** Update Kalman filter with new measurement */
	void UpdateKalman(float Measurementvalue, int64_t NowTime);

	/** Calculate current Kalman measurement noise variance */
	float CalculateMeasurementNoiseVar() const;

	/** Update legacy state with new target (internal use) */
	void UpdateInt(int64_t Target);

	/** Update jitter buffer with new sample */
	void UpdateJitterBuffer(float TimeLeft, EAdjustDirection DirectionIdx);

	/** Calculate adaptive spike threshold */
	float GetAdaptiveSpikeThreshold(EAdjustDirection DirectionIdx) const;

	/** Update RTT average */
	void UpdateRTT(float RttMs);

	/** Get current RTT-based threshold */
	float GetRTTThreshold() const;

	/** Convert milliseconds to time units */
	int64_t MsToTime(int64_t TimeMs) const;

	/** Convert time units to milliseconds */
	int64_t TimeToMs(int64_t TimeValue) const;

	/** Check if filter is warm enough */
	bool IsFilterWarm(int64_t NowTime) const;
};

#endif // ENGINE_CLIENT_SMOOTH_TIME_H
