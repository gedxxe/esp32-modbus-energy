// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "meter_registry.h"
namespace fort {
enum MeterLabOperation:uint8_t { LAB_START,LAB_SAVE,LAB_REMOVE,LAB_SELECT };
struct MeterLabRequest {
  MeterLabOperation operation=LAB_START;DiscoveryConfig config{};uint32_t run=0;uint16_t meterId=0;uint8_t index=0,profile=UNKNOWN_PROFILE;bool verified=false;
  char name[32]={},model[40]={},notes[64]={};
};
struct MeterLabSnapshot {
  MeterDiscovery discovery{};MeterRegistry registry{};bool storageOK=true,ready=false;uint16_t activeId=0;char error[80]={};
};
} // namespace fort
