// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <WebServer.h>
#include <ArduinoJson.h>
#include "lora_service.h"
namespace fort {
void setupLoRaWeb(WebServer &,bool (*authorized)(),void (*save)(const lora::Settings &,uint32_t));
void addLoRaJSON(JsonObject);
}
