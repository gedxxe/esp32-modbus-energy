// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "checkpoint_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstdint>
namespace fort { namespace iot {
struct Status {
  bool configured=false,enabled=false,archiveReady=false,localHeaderPresent=false;
  bool cursorVerified=false; // HTTPS ACK loaded or proven absent on this boot
  uint32_t workerPulseMs=0; // monotonic loop liveness, copied via the one-slot status queue
  uint8_t clientRevision=3;
  uint8_t phase=0; // 0 disabled, 1 idle, 2 uploading, 3 retry, 4 blocked
  uint32_t accepted=0,duplicates=0,failures=0,lastAcceptedUTC=0,lastAckDay=0;
  uint32_t expiredLiveRetries=0; // Ephemeral unacknowledged snapshots replaced after TTL
  // Server presence and reading receipts are independent: heartbeat never
  // modifies accepted/duplicates, lastAcceptedUTC or the archive cursor.
  uint32_t heartbeatAttempts=0,heartbeatAccepted=0,heartbeatFailures=0,lastHeartbeatUTC=0;
  uint32_t lastManualRequestedUTC=0,lastManualServedUTC=0;
  int32_t heartbeatHttp=0,heartbeatTransportError=0,heartbeatTlsError=0,heartbeatTlsFlags=0;
  char heartbeatError[40]={};
  char archiveError[40]={};
  uint8_t lastAckSlot=0;
  int32_t httpStatus=0,transportError=0,socketErrno=0,tlsError=0,tlsFlags=0;
  uint32_t freeHeapBeforePost=0,freeHeapBeforeProbe=0;
  // Aggregate free heap is not the largest contiguous TLS allocation.
  uint32_t internalFreeBeforePost=0,internalLargestBeforePost=0;
  uint32_t psramFreeBeforePost=0,psramLargestBeforePost=0;
  bool tlsAllocatorPsram=false;
  bool probeAttempted=false;
  int32_t probeHttp=0,probeTransportError=0,probeTlsError=0,probeTlsFlags=0;
  char error[40]={};
};
bool start(SemaphoreHandle_t filesystemMutex,bool filesystemMounted);
void update(const CaptureDay &active,const CaptureDay &previous,
            bool captureDurable,bool stationOnline,bool clockValid,uint32_t utc,
            uint32_t liveCapturedUTC,bool telemetryValid,double telemetryKWh,uint8_t profile,
            bool currentValid,double currentA);
Status readStatus();
} }
