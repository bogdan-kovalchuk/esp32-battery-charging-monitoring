#pragma once

#include <cstdint>

// Pure reporting state machine. It deliberately has no Arduino dependency so
// transition and millis()-rollover behaviour can be verified on the host.
enum class BatteryState : uint8_t { UNKNOWN, NORMAL, CRITICAL };
enum class ReportDecision : uint8_t { NONE, INFO, ALERT };

class ReportScheduler {
 public:
  explicit ReportScheduler(uint32_t sampleIntervalMs)
      : sampleIntervalMs_(sampleIntervalMs) {}

  bool sampleDue(uint32_t now) const { return due(now, nextSampleAt_); }

  void recordInvalidSample(uint32_t now, uint32_t retryMs) {
    nextSampleAt_ = now + retryMs;
  }

  ReportDecision recordSample(uint32_t now, bool critical) {
    nextSampleAt_ = now + sampleIntervalMs_;
    BatteryState measured =
        critical ? BatteryState::CRITICAL : BatteryState::NORMAL;

    // A transition is reported at the first sample that observes it. Reminder
    // cadence never suppresses a recovery or a renewed critical condition.
    if (measured != state_) {
      state_ = measured;
      return decisionFor(state_);
    }

    return due(now, nextReportAt_) ? decisionFor(state_)
                                   : ReportDecision::NONE;
  }

  void recordSendResult(uint32_t now, bool success, uint32_t repeatIntervalMs,
                        uint32_t retryIntervalMs) {
    nextReportAt_ = now + (success ? repeatIntervalMs : retryIntervalMs);
  }

  BatteryState state() const { return state_; }

 private:
  static bool due(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
  }

  static ReportDecision decisionFor(BatteryState state) {
    if (state == BatteryState::CRITICAL) return ReportDecision::ALERT;
    if (state == BatteryState::NORMAL) return ReportDecision::INFO;
    return ReportDecision::NONE;
  }

  uint32_t sampleIntervalMs_;
  uint32_t nextSampleAt_ = 0;
  uint32_t nextReportAt_ = 0;
  BatteryState state_ = BatteryState::UNKNOWN;
};
