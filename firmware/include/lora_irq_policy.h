// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>

namespace fort { namespace lora {
// The IRQ is edge-notified through a FreeRTOS task notification; DIO1 level
// polling is a second path for an edge that predated ISR registration.
// Both paths are intentionally coalesced: one RadioLib status read services
// the latched IRQ causes on the radio task, never in interrupt context.
inline bool irqServiceDue(uint32_t notificationCount,bool dio1High){
  return notificationCount!=0||dio1High;
}
inline bool irqRecoveredFromLevel(uint32_t notificationCount,bool dio1High){
  return notificationCount==0&&dio1High;
}
} } // namespace fort::lora
