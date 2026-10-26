// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>

namespace fort {
constexpr uint32_t CLOCK_MIN_UTC=1704067200UL; // 2024-01-01T00:00:00Z
constexpr uint32_t CLOCK_MAX_UTC=4102444799UL; // 2099-12-31T23:59:59Z
constexpr uint32_t CLOCK_COMMAND_MAX_AGE_MS=10000;

// Bound both UTC and the displayed/local calendar without overflowing uint32_t.
inline bool validClockEpoch(uint32_t utc,uint32_t offset) {
  return utc>=CLOCK_MIN_UTC&&utc<=CLOCK_MAX_UTC&&
         uint64_t(utc)+offset<=CLOCK_MAX_UTC;
}
// A saved day remains protected when the RTC is absent, invalid or newly fitted.
// Call this before changing the tracker day as well as before explicit RTC writes.
inline bool protectsRecordedDay(uint32_t utc,uint32_t offset,uint32_t retainedDay) {
  return validClockEpoch(utc,offset)&&
         (uint64_t(utc)+offset)/86400>=retainedDay;
}

enum ClockGuardResult : uint8_t {
  CLOCK_ACCEPTED,CLOCK_INVALID_EPOCH,CLOCK_BACKWARD_DAY,CLOCK_STALE_REQUEST
};
inline ClockGuardResult resolveQueuedClockTime(uint32_t requestedUTC,uint32_t queuedAt,
                                               uint32_t now,uint32_t offset,
                                               uint32_t retainedDay,uint32_t &effectiveUTC) {
  // Unsigned age remains valid across millis() rollover. No old phone timestamp
  // may be applied after a long scan, maintenance session or stalled command queue.
  uint32_t age=now-queuedAt;
  if(age>CLOCK_COMMAND_MAX_AGE_MS)return CLOCK_STALE_REQUEST;
  if(!validClockEpoch(requestedUTC,offset))return CLOCK_INVALID_EPOCH;
  uint64_t adjusted=uint64_t(requestedUTC)+age/1000;
  if(adjusted>CLOCK_MAX_UTC||!validClockEpoch(uint32_t(adjusted),offset))return CLOCK_INVALID_EPOCH;
  if(!protectsRecordedDay(uint32_t(adjusted),offset,retainedDay))return CLOCK_BACKWARD_DAY;
  effectiveUTC=uint32_t(adjusted);
  return CLOCK_ACCEPTED;
}
} // namespace fort
