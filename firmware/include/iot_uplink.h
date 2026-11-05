// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "checkpoint_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstdint>
namespace fort { namespace iot {
struct Status {
  bool configured=false,enabled=false,archiveReady=false,localHeaderPresent=false;
  uint8_t clientRevision=2;
  uint8_t phase=0; // 0 disabled, 1 idle, 2 uploading, 3 retry, 4 blocked
  uint32_t accepted=0,duplicates=0,failures=0,lastAcceptedUTC=0,lastAckDay=0;
  uint8_t lastAckSlot=0;
  int32_t httpStatus=0,transportError=0,socketErrno=0,tlsError=0,tlsFlags=0;
  uint32_t freeHeapBeforePost=0;
  char error[40]={};
};
bool start(SemaphoreHandle_t filesystemMutex,bool filesystemMounted);
void update(const CaptureDay &active,const CaptureDay &previous,
            bool captureDurable,bool stationOnline,bool clockValid,uint32_t utc,
            uint32_t liveCapturedUTC,bool telemetryValid,double telemetryKWh,uint8_t profile,
            bool currentValid,double currentA);
Status readStatus();
} }
