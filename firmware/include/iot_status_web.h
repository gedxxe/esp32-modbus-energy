// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <ArduinoJson.h>
#include <cstdint>

namespace fort {
namespace iot {struct Status;}
// Stateless serialization of the existing /api/status.iot contract.
// Keep HTTPS transport, ACK durability and sampling out of the HTTP layer.
void addIoTStatusJSON(JsonObject json,const iot::Status &status,uint32_t nowMs);
} // namespace fort
