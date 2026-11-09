#!/usr/bin/env python3
"""Pin /api/status.iot serialization fields across main.cpp extraction.

Structural regression only: the firmware compiler and runtime/browser tests
must independently verify ArduinoJson and endpoint behavior.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
EXPECTED = ("configured", "enabled", "localHeaderPresent", "clientRevision", "workerPulseAgeMs", "workerPulseSeen", "phase", "archiveReady", "cursorVerified", "accepted", "duplicates", "expiredLiveRetries", "backfillCursorKnown", "backfillDaysBehind", "backfillRemainingDays", "backfillNearLimit", "backfillWindowExceeded", "failures", "lastHttp", "heartbeatAttempts", "heartbeatAccepted", "heartbeatFailures", "lastHeartbeatUTC", "heartbeatHttp", "heartbeatTransportError", "heartbeatTlsError", "heartbeatTlsFlags", "heartbeatError", "lastManualRequestedUTC", "lastManualServedUTC", "archiveError", "transportError", "socketErrno", "tlsError", "tlsFlags", "freeHeapBeforePost", "internalFreeBeforePost", "internalLargestBeforePost", "psramFreeBeforePost", "psramLargestBeforePost", "tlsAllocatorPsram", "probeAttempted", "probeHttp", "probeTransportError", "probeTlsError", "probeTlsFlags", "freeHeapBeforeProbe", "lastAcceptedUTC", "lastAckDay", "lastAckSlot", "error",)
source = (ROOT / "firmware/src/iot_status_web.cpp").read_text()
main = (ROOT / "firmware/src/main.cpp").read_text()
actual = tuple(re.findall(r'\biotState\["([^"]+)"\]\s*=', source))
assert actual == EXPECTED, (
    f"IoT status schema changed: expected {EXPECTED!r}, got {actual!r}"
)
assert main.count('addIoTStatusJSON(d["iot"].to<JsonObject>(),iot::readStatus(),millis())') == 1
assert 'uint32_t(nowMs-uplink.workerPulseMs)' in source
assert 'uplink.lastAckDay?uplink.lastAckSlot:-1' in source
assert 'boundedWrite(data,size,requestStarted_,5000' in (
    ROOT / "firmware/src/bounded_web_server.cpp"
).read_text()
assert 'WebServer::handleClient();' in (
    ROOT / "firmware/src/bounded_web_server.cpp"
).read_text()

# Post-PR#29 composition: main.cpp must preserve both independent JSON
# adapters. In particular the IRQ counters from the merged LoRa fix must not
# disappear when the IoT status serializer is moved out of main.cpp.
assert main.count('addLoRaJSON(d["lora"].to<JsonObject>())') == 1
assert main.index('addLoRaJSON(d["lora"].to<JsonObject>())') < main.index(
    'addIoTStatusJSON(d["iot"].to<JsonObject>(),iot::readStatus(),millis())'
)
lora_json = (ROOT / "firmware/src/lora_web.cpp").read_text()
lora_task = (ROOT / "firmware/src/lora_service.cpp").read_text()
for field in ("irqNotifications", "irqPinFallbacks"):
    assert f'd["{field}"]=v.{field}' in lora_json, f"Lost PR#29 LoRa field: {field}"
assert "vTaskNotifyGiveFromISR(target,&higherPriorityWoken)" in lora_task
assert "ulTaskNotifyTake(pdTRUE,0)" in lora_task

# Owner explicitly freezes the installed dependency versions.
platform = (ROOT / "firmware/platformio.ini").read_text()
requirements = (ROOT / "requirements-dev.txt").read_text()
assert "releases/download/55.03.311/platform-espressif32.zip" in platform
assert "pioarduino==6.1.19" in requirements
print(f"PASS: {len(EXPECTED)} IoT fields, PR#29 LoRa ISR/JSON integration, unchanged HTTP bounds and pinned framework")
