// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "lora_protocol.h"
#include "checkpoint_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace fort {
struct LoRaTelemetry {uint32_t utc=0,ageMs=0,day=0,revision=0;double kwh=0;uint16_t meterId=0;uint8_t profile=0,flags=0;};
struct LoRaArchiveQuery {uint32_t day=0,request=0;uint64_t session=0;};
struct LoRaArchiveReply {uint32_t day=0,request=0;uint64_t session=0;bool found=false;uint8_t bytes[CAPTURE_RECORD_SIZE]={};};
struct LoRaView {uint32_t revision=0;lora::Settings settings{};bool configured=false,stored=false,available=false,paired=false;uint8_t phase=0;int16_t code=0;uint32_t authErrors=0,addressErrors=0,replayErrors=0,lastTxSequence=0,lastRxSequence=0;uint32_t tx=0,rx=0,acks=0,timeouts=0,rejected=0,crcErrors=0,drops=0,lastRxMs=0,waitMs=0,lastAirMs=0,cadBusy=0,cadClear=0,cadErrors=0,cadDeferred=0;float rssi=0,snr=0;};
enum LoRaPhase:uint8_t {LORA_DISABLED,LORA_STARTING,LORA_LISTENING,LORA_TRANSMITTING,LORA_WAIT_ACK,LORA_COOLDOWN,LORA_ERROR,LORA_CHANNEL_CHECK};
bool initLoRaService();
bool canQueueLoRaSettings();
bool updateLoRaSettings(const lora::Settings &,bool stored,uint32_t revision);
bool controlLoRa(uint8_t action); // 1: pair again, 2: request current telemetry.
void publishLoRaTelemetry(const LoRaTelemetry &);
bool takeLoRaArchiveQuery(LoRaArchiveQuery &);
void replyLoRaArchive(const LoRaArchiveReply &);
LoRaView readLoRaView();
TaskHandle_t loRaTaskHandle();
} // namespace fort
