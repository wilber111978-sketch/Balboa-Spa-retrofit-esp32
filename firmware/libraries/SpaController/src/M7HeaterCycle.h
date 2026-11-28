#pragma once

#include "M7Qualification.h"

namespace m7 {

enum class Shutdown { None, Qualification, UnexpectedHeat };

struct Outputs {
    bool contactor = false;
    bool ssr = false;
    Shutdown fault = Shutdown::None;
};

// Logical outputs for one thermostat call. The caller owns the physical
// output gate, independent cutoff, fault latching and pump interlocks.
class HeaterCycle {
public:
    constexpr HeaterCycle(Limits limits, uint32_t proofMs, uint32_t contactorSettleMs,
                          float proofRiseC)
        : qualification_(limits), maxSampleGapMs_(limits.maxSampleGapMs), proofMs_(proofMs),
          contactorSettleMs_(contactorSettleMs), proofRiseC_(proofRiseC) {}

    constexpr Phase phase() const { return active_ ? qualification_.phase() : Phase::Idle; }
    constexpr Failure qualificationFailure() const { return qualification_.failure(); }
    constexpr uint8_t retries() const { return retries_; }
    static constexpr uint8_t maxRetries = 2;

    constexpr Outputs update(Sample sample, bool demand)
    {
        if (!demand || !sample.interlocksOk) {
            active_ = false;
            retries_ = 0;
            return {};
        }
        if (!active_) {
            if (!(proofMs_ > 0 && proofMs_ < circulationMs &&
                  maxSampleGapMs_ < circulationMs - proofMs_ &&
                  contactorSettleMs_ > 0 && proofRiseC_ > 0)) {
                return {false, false, Shutdown::Qualification};
            }
            fault_ = Shutdown::None;
            active_ = true;
            started_ = sample.timeMs;
            proofStarted_ = false;
            normalContactorStarted_ = false;
            qualification_.start(sample.timeMs);
        }
        if (fault_ != Shutdown::None) return {false, false, fault_};

        const Phase previous = qualification_.phase();
        qualification_.update(sample);
        const Phase current = qualification_.phase();
        if (current == Phase::Failed) {
            // V0.9.3: a late/invalid reading during the test restarts it (heater already off)
            // rather than latching; the third in one heating call is a fault.
            if (qualification_.failure() == Failure::InvalidSample && previous != Phase::Passed &&
                retries_ < maxRetries) {
                ++retries_;
                active_ = false;
                return {};
            }
            return {false, false, Shutdown::Qualification};
        }

        if (current == Phase::Circulating || (previous == Phase::Circulating && current == Phase::Pulse)) {
            if (sample.timeMs - started_ < circulationMs - proofMs_ - maxSampleGapMs_) return {};
            const float difference = absoluteDifference(sample);
            if (!proofStarted_) {
                if (current == Phase::Pulse) return {false, false, Shutdown::Qualification};
                proofStarted_ = true;
                proofAt_ = sample.timeMs;
                proofBaselineDifference_ = difference;
            }
            if (difference - proofBaselineDifference_ >= proofRiseC_) {
                fault_ = Shutdown::UnexpectedHeat;
                return {false, false, fault_};
            }
            if (current == Phase::Circulating) return {true, false};
            if (sample.timeMs - proofAt_ < proofMs_)
                return {false, false, Shutdown::Qualification};
        }

        if (current == Phase::Pulse) return {true, true};
        if (current == Phase::Observing) return {};
        if (current == Phase::Passed) {
            if (!normalContactorStarted_) {
                normalContactorStarted_ = true;
                normalContactorAt_ = sample.timeMs;
                normalBaselineDifference_ = absoluteDifference(sample);
            }
            const uint32_t settleMs = proofMs_ > contactorSettleMs_ ? proofMs_ : contactorSettleMs_;
            if (sample.timeMs - normalContactorAt_ <= settleMs &&
                absoluteDifference(sample) - normalBaselineDifference_ >= proofRiseC_) {
                fault_ = Shutdown::UnexpectedHeat;
                return {false, false, fault_};
            }
            return {true, sample.timeMs - normalContactorAt_ >= settleMs && qualification_.sampleOk()};
        }
        return {};
    }

private:
    static constexpr float absoluteDifference(Sample sample)
    {
        return sample.aC >= sample.bC ? sample.aC - sample.bC : sample.bC - sample.aC;
    }

    Qualification qualification_;
    uint32_t maxSampleGapMs_;
    uint32_t proofMs_;
    uint32_t contactorSettleMs_;
    float proofRiseC_;
    bool active_ = false;
    uint8_t retries_ = 0;
    bool proofStarted_ = false;
    bool normalContactorStarted_ = false;
    uint32_t started_ = 0;
    uint32_t proofAt_ = 0;
    uint32_t normalContactorAt_ = 0;
    float proofBaselineDifference_ = 0;
    float normalBaselineDifference_ = 0;
    Shutdown fault_ = Shutdown::None;
};

} // namespace m7
