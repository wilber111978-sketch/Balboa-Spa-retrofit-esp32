#pragma once

#include <float.h>
#include <stdint.h>

// Qualification model for recorded or simulated inputs. No GPIO or relay access.
namespace m7 {

constexpr uint32_t circulationMs = 180000; // Owner's preheat circulation target.
constexpr uint32_t observationMs = 90000;   // Heater-off observation in the M7 guide.

enum class Phase { Idle, Circulating, Pulse, Observing, Passed, Failed };
enum class Failure { None, Configuration, Interlock, InvalidSample, BaselineMismatch,
                     Overtemperature, ExcessRise, ExcessDifference, NoResponse, DidNotRecover };

struct Limits {
    uint32_t pulseMs;
    uint32_t maxSampleGapMs;
    float baselineDifferenceC;
    float minPeakRiseC;
    float maxPeakRiseC;
    float maxDifferenceC;
    float recoveryRiseC;
    float maxTemperatureC;
    uint32_t recoveryHoldMs;
    float maxRecoveryCreepC;
};

struct Sample {
    uint32_t timeMs;
    float aC;
    float bC;
    bool valid;
    bool interlocksOk; // Simulated/replayed demand, LOW, cutoff and fault state.
};

class Qualification {
public:
    constexpr explicit Qualification(Limits limits) : limits_(limits) {}

    constexpr Phase phase() const { return phase_; }
    constexpr Failure failure() const { return failure_; }

    constexpr void start(uint32_t now)
    {
        phase_ = Phase::Circulating;
        failure_ = Failure::None;
        phaseStarted_ = lastSample_ = now;
        peakRiseC_ = 0;
        recoveryInRange_ = false;
        if (!(finite(limits_.baselineDifferenceC) && finite(limits_.minPeakRiseC) &&
              finite(limits_.maxPeakRiseC) && finite(limits_.maxDifferenceC) &&
              finite(limits_.recoveryRiseC) && finite(limits_.maxTemperatureC) &&
              limits_.pulseMs > 0 && limits_.maxSampleGapMs > 0 &&
              limits_.maxSampleGapMs < limits_.pulseMs &&
              limits_.baselineDifferenceC >= 0 &&
              limits_.minPeakRiseC > 0 && limits_.minPeakRiseC <= limits_.maxPeakRiseC &&
              limits_.maxDifferenceC >= limits_.baselineDifferenceC &&
              limits_.recoveryRiseC >= 0 && limits_.recoveryRiseC < limits_.maxPeakRiseC &&
              limits_.maxTemperatureC > 0 &&
              limits_.recoveryHoldMs >= limits_.maxSampleGapMs &&
              limits_.recoveryHoldMs < observationMs &&
              finite(limits_.maxRecoveryCreepC) && limits_.maxRecoveryCreepC >= 0)) {
            fail(Failure::Configuration);
        }
    }

    constexpr void update(Sample sample)
    {
        if (phase_ == Phase::Idle || phase_ == Phase::Failed) return;
        if (!sample.interlocksOk) return fail(Failure::Interlock);
        if (!sample.valid || !finite(sample.aC) || !finite(sample.bC) ||
            sample.timeMs - lastSample_ > limits_.maxSampleGapMs) {
            return fail(Failure::InvalidSample);
        }
        lastSample_ = sample.timeMs;
        if (sample.aC >= limits_.maxTemperatureC || sample.bC >= limits_.maxTemperatureC)
            return fail(Failure::Overtemperature);
        const float differenceC = absolute(sample.aC - sample.bC);
        const uint32_t elapsedMs = sample.timeMs - phaseStarted_;

        if (phase_ == Phase::Circulating) {
            if (elapsedMs < circulationMs) return;
            if (differenceC > limits_.baselineDifferenceC) return fail(Failure::BaselineMismatch);
            baselineAC_ = sample.aC;
            baselineBC_ = sample.bC;
            transition(Phase::Pulse, sample.timeMs);
            return;
        }
        if (differenceC > limits_.maxDifferenceC) return fail(Failure::ExcessDifference);
        if (phase_ == Phase::Passed) return;

        const float aRiseC = sample.aC - baselineAC_;
        const float bRiseC = sample.bC - baselineBC_;
        const float riseC = aRiseC > bRiseC ? aRiseC : bRiseC;
        if (riseC > limits_.maxPeakRiseC) return fail(Failure::ExcessRise);
        if (riseC > peakRiseC_) peakRiseC_ = riseC;

        if (phase_ == Phase::Pulse && elapsedMs >= limits_.pulseMs) {
            transition(Phase::Observing, sample.timeMs);
        } else if (phase_ == Phase::Observing) {
            updateRecovery(sample, riseC, differenceC);
            if (elapsedMs >= observationMs) {
                if (peakRiseC_ < limits_.minPeakRiseC) return fail(Failure::NoResponse);
                if (!recoveryInRange_ || sample.timeMs - recoveryStarted_ < limits_.recoveryHoldMs)
                    return fail(Failure::DidNotRecover);
                transition(Phase::Passed, sample.timeMs);
            }
        }
    }

private:
    static constexpr bool finite(float value) { return value >= -FLT_MAX && value <= FLT_MAX; }
    static constexpr float absolute(float value) { return value < 0 ? -value : value; }

    constexpr void updateRecovery(Sample sample, float riseC, float differenceC)
    {
        if (riseC > limits_.recoveryRiseC || differenceC > limits_.baselineDifferenceC) {
            recoveryInRange_ = false;
            return;
        }
        if (!recoveryInRange_ || sample.aC - recoveryMinAC_ > limits_.maxRecoveryCreepC ||
            sample.bC - recoveryMinBC_ > limits_.maxRecoveryCreepC) {
            recoveryInRange_ = true;
            recoveryStarted_ = sample.timeMs;
            recoveryMinAC_ = sample.aC;
            recoveryMinBC_ = sample.bC;
            return;
        }
        if (sample.aC < recoveryMinAC_) recoveryMinAC_ = sample.aC;
        if (sample.bC < recoveryMinBC_) recoveryMinBC_ = sample.bC;
    }

    constexpr void transition(Phase phase, uint32_t now)
    {
        phase_ = phase;
        phaseStarted_ = now;
    }

    constexpr void fail(Failure reason)
    {
        phase_ = Phase::Failed;
        failure_ = reason;
    }

    Limits limits_;
    Phase phase_ = Phase::Idle;
    Failure failure_ = Failure::None;
    uint32_t phaseStarted_ = 0;
    uint32_t lastSample_ = 0;
    float baselineAC_ = 0;
    float baselineBC_ = 0;
    float peakRiseC_ = 0;
    bool recoveryInRange_ = false;
    uint32_t recoveryStarted_ = 0;
    float recoveryMinAC_ = 0;
    float recoveryMinBC_ = 0;
};

} // namespace m7
