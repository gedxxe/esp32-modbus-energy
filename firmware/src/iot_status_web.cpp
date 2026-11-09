// SPDX-License-Identifier: GPL-3.0-only
#include "iot_status_web.h"
#include "iot_uplink.h"

namespace fort {
void addIoTStatusJSON(JsonObject iotState,const iot::Status &uplink,uint32_t nowMs){
    iotState["configured"]=uplink.configured;iotState["enabled"]=uplink.enabled;
    iotState["localHeaderPresent"]=uplink.localHeaderPresent;
    iotState["clientRevision"]=uplink.clientRevision;
    iotState["workerPulseAgeMs"]=uplink.workerPulseMs?uint32_t(nowMs-uplink.workerPulseMs):0;
    iotState["workerPulseSeen"]=uplink.workerPulseMs!=0;
    iotState["phase"]=uplink.phase;iotState["archiveReady"]=uplink.archiveReady;
    iotState["cursorVerified"]=uplink.cursorVerified;
    iotState["accepted"]=uplink.accepted;iotState["duplicates"]=uplink.duplicates;
    iotState["expiredLiveRetries"]=uplink.expiredLiveRetries;
    iotState["backfillCursorKnown"]=uplink.backfillCursorKnown;
    iotState["backfillDaysBehind"]=uplink.backfillDaysBehind;
    iotState["backfillRemainingDays"]=uplink.backfillRemainingDays;
    iotState["backfillNearLimit"]=uplink.backfillNearLimit;
    iotState["backfillWindowExceeded"]=uplink.backfillWindowExceeded;
    iotState["failures"]=uplink.failures;iotState["lastHttp"]=uplink.httpStatus;
    iotState["heartbeatAttempts"]=uplink.heartbeatAttempts;
    iotState["heartbeatAccepted"]=uplink.heartbeatAccepted;
    iotState["heartbeatFailures"]=uplink.heartbeatFailures;
    iotState["lastHeartbeatUTC"]=uplink.lastHeartbeatUTC;
    iotState["heartbeatHttp"]=uplink.heartbeatHttp;
    iotState["heartbeatTransportError"]=uplink.heartbeatTransportError;
    iotState["heartbeatTlsError"]=uplink.heartbeatTlsError;
    iotState["heartbeatTlsFlags"]=uplink.heartbeatTlsFlags;
    iotState["heartbeatError"]=uplink.heartbeatError;
  iotState["lastManualRequestedUTC"]=uplink.lastManualRequestedUTC;
  iotState["lastManualServedUTC"]=uplink.lastManualServedUTC;
    iotState["archiveError"]=uplink.archiveError;
    iotState["transportError"]=uplink.transportError;
    iotState["socketErrno"]=uplink.socketErrno;
    iotState["tlsError"]=uplink.tlsError;iotState["tlsFlags"]=uplink.tlsFlags;
    iotState["freeHeapBeforePost"]=uplink.freeHeapBeforePost;
    iotState["internalFreeBeforePost"]=uplink.internalFreeBeforePost;
    iotState["internalLargestBeforePost"]=uplink.internalLargestBeforePost;
    iotState["psramFreeBeforePost"]=uplink.psramFreeBeforePost;
    iotState["psramLargestBeforePost"]=uplink.psramLargestBeforePost;
    iotState["tlsAllocatorPsram"]=uplink.tlsAllocatorPsram;
    iotState["probeAttempted"]=uplink.probeAttempted;
    iotState["probeHttp"]=uplink.probeHttp;
    iotState["probeTransportError"]=uplink.probeTransportError;
    iotState["probeTlsError"]=uplink.probeTlsError;iotState["probeTlsFlags"]=uplink.probeTlsFlags;
    iotState["freeHeapBeforeProbe"]=uplink.freeHeapBeforeProbe;
    iotState["lastAcceptedUTC"]=uplink.lastAcceptedUTC;
    iotState["lastAckDay"]=uplink.lastAckDay;
    iotState["lastAckSlot"]=uplink.lastAckDay?uplink.lastAckSlot:-1;
    iotState["error"]=uplink.error;

}
} // namespace fort
