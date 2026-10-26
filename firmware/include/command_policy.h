// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>

namespace fort {
// Core 0 queues values; the core 1 owner revalidates their mode at execution.
// A mode revision prevents an old command from applying in a later normal session.
enum CommandType : uint8_t {
  SET_TIME,SET_CONFIG,TOGGLE_MUTE,START_SCAN,CANCEL_SCAN,APPLY_SCAN,
  SET_LED,START_TRACE,STOP_TRACE
};
constexpr uint32_t COMMAND_MAX_AGE_MS=10000;
inline bool safeDuringMaintenance(CommandType type) {
  return type==TOGGLE_MUTE||type==SET_LED||type==START_TRACE||type==STOP_TRACE;
}
inline bool commandModeAllowed(CommandType type,bool diagnostic,bool scanRunning,bool scanFound) {
  if(safeDuringMaintenance(type))return true;
  if(diagnostic)return false;
  switch(type) {
    case SET_TIME:case SET_CONFIG:case START_SCAN:return !scanRunning;
    case CANCEL_SCAN:return scanRunning;
    case APPLY_SCAN:return !scanRunning&&scanFound;
    default:return false;
  }
}
inline bool commandContextValid(CommandType type,uint32_t submittedRevision,
                                uint32_t currentRevision,uint32_t queuedAt,uint32_t now,
                                bool diagnostic,bool scanRunning,bool scanFound) {
  if(!commandModeAllowed(type,diagnostic,scanRunning,scanFound))return false;
  if(safeDuringMaintenance(type))return true;
  return submittedRevision==currentRevision&&uint32_t(now-queuedAt)<=COMMAND_MAX_AGE_MS;
}
} // namespace fort
