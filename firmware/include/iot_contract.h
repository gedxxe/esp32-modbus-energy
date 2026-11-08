// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Wire-format adapter for Midea Energy Record's IOT schema v1.
// No UART, RTC, flash, Wi-Fi or Arduino dependencies: safe for native tests.
#include "checkpoint_log.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace fort { namespace iot {

struct Binding {
  char device[65]={}, meter[65]={}, profile[65]={};
};
struct Event {
  char id[64]={};
  char json[640]={};
  size_t size=0;
  uint32_t day=0;
  uint8_t index=0;
};
inline bool identifier(const char *s){
  if(!s||!s[0])return false;
  size_t n=0;
  for(;s[n]&&n<=64;++n){
    const char c=s[n];
    if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
    if(n==0&&!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')))return false;
  }
  return n>=1&&n<=64&&!s[n];
}
inline bool valid(const Binding &b){return identifier(b.device)&&identifier(b.meter)&&identifier(b.profile);}

// UTC epoch to strict YYYY-MM-DDTHH:MM:SSZ, independently of the ESP32 system clock.
inline bool utcText(uint32_t epoch,char (&out)[21]){
  if(epoch<946684800UL||epoch>4102444799UL)return false; // website accepts 2000..2099
  int64_t z=epoch/86400;
  unsigned seconds=epoch%86400;
  z+=719468;
  const int64_t era=(z>=0?z:z-146096)/146097;
  const unsigned doe=unsigned(z-era*146097);
  const unsigned yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
  int year=int(yoe)+int(era*400);
  const unsigned doy=doe-(365*yoe+yoe/4-yoe/100);
  const unsigned mp=(5*doy+2)/153;
  const unsigned day=doy-(153*mp+2)/5+1;
  const unsigned month=mp+(mp<10?3:-9);
  year+=(month<=2);
  int n=snprintf(out,sizeof(out),"%04d-%02u-%02uT%02u:%02u:%02uZ",
    year,month,day,seconds/3600,(seconds/60)%60,seconds%60);
  return n==20;
}
inline bool primaryText(double energy,char (&out)[40]){
  if(!std::isfinite(energy)||energy<0||energy>1000000000000.0)return false;
  int n=snprintf(out,sizeof(out),"%.6f",energy==0.0?0.0:energy); // normalize IEEE-754 negative zero
  return n>0&&n<int(sizeof(out));
}
// Sample-age admission remains independent of the HTTPS task and UART.
// A single currentA channel is present only if the selected profile defines it.
inline bool freshCurrent(bool liveEnergyValid,uint8_t profile,double currentA,
                         uint32_t sampledAtMs,uint32_t nowMs){
  return liveEnergyValid&&profiles::valid(profile)&&
    profiles::available(profiles::get(profile).current)&&sampledAtMs&&
    uint32_t(nowMs-sampledAtMs)<=15000&&std::isfinite(currentA)&&currentA>=0;
}
// Immutable capture -> stable event ID + canonical field set expected by website.
// This routine does not invent readings or modify a capture.
inline bool checkpoint(const CaptureDay &day,unsigned index,const Binding &binding,Event &out){
  out=Event{};
  if(!valid(binding)||index>=CAPTURE_COUNT||!validCaptureDayNumber(day.number))return false;
  const MeterCapture &p=day.points[index];
  if(p.status!=CAPTURE_RECORDED||!validCaptureSource(p.source)||
     !profiles::valid(p.source.profile)||strcmp(profiles::get(p.source.profile).key,binding.profile))return false;
  const uint32_t scheduled=captureTarget(day.number,index);
  if(p.startedUTC<scheduled||p.capturedUTC<p.startedUTC||
     uint64_t(p.capturedUTC)>uint64_t(scheduled)+CAPTURE_WINDOW||
     p.capturedUTC-p.startedUTC>2)return false;
  char start[21],end[21],target[21],number[40];
  if(!utcText(scheduled,target)||!utcText(p.startedUTC,start)||!utcText(p.capturedUTC,end)||
     !primaryText(p.energy,number))return false;
  int idlen=snprintf(out.id,sizeof(out.id),"cp_%lu_%02u",(unsigned long)day.number,index);
  if(idlen<=0||idlen>=int(sizeof(out.id)))return false;
  int n=snprintf(out.json,sizeof(out.json),
    "{\"schema_version\":1,\"event_id\":\"%s\",\"device_id\":\"%s\","
    "\"meter_id\":\"%s\",\"meter_profile\":\"%s\",\"event_type\":\"checkpoint\","
    "\"captured_at\":\"%s\",\"quality\":\"valid\",\"readings\":{\"active_energy_kwh\":\"%s\"},"
    "\"scheduled_at\":\"%s\",\"started_at\":\"%s\"}",
    out.id,binding.device,binding.meter,binding.profile,end,number,target,start);
  if(n<=0||n>=int(sizeof(out.json)))return false;
  out.size=size_t(n);out.day=day.number;out.index=uint8_t(index);return true;
}
// The source has one named current-A channel, not three measured phases.
// Snapshot inputs are validated once: immutable JSON stays frozen across retries.
inline bool telemetry(uint32_t captured,double energy,const Binding &binding,
                      uint32_t boot,uint32_t sequence,Event &out,
                      bool currentAvailable=false,double currentA=0){
  out=Event{};
  if(!valid(binding)||!boot||!sequence)return false;
  char timestamp[21],number[40],currentNumber[40]={};
  if(!utcText(captured,timestamp)||!primaryText(energy,number))return false;
  const bool useCurrent=currentAvailable&&primaryText(currentA,currentNumber);
  char extra[80]={};
  if(useCurrent){
    int length=snprintf(extra,sizeof(extra),",\"current_a\":\"%s\"",currentNumber);
    if(length<=0||length>=int(sizeof(extra)))return false;
  }
  int idlen=snprintf(out.id,sizeof(out.id),"live_%08lx_%08lx",
    (unsigned long)boot,(unsigned long)sequence);
  if(idlen<=0||idlen>=int(sizeof(out.id)))return false;
  int n=snprintf(out.json,sizeof(out.json),
    "{\"schema_version\":1,\"event_id\":\"%s\",\"device_id\":\"%s\","
    "\"meter_id\":\"%s\",\"meter_profile\":\"%s\",\"event_type\":\"telemetry\","
    "\"captured_at\":\"%s\",\"quality\":\"valid\",\"readings\":{\"active_energy_kwh\":\"%s\"%s}}",
    out.id,binding.device,binding.meter,binding.profile,timestamp,number,extra);
  if(n<=0||n>=int(sizeof(out.json)))return false;
  out.size=size_t(n);return true;
}
// Heartbeat v1 is an authenticated contact signal, never a measurement.
// Its body deliberately has no event ID, sampled timestamp or readings.
inline bool heartbeat(const Binding &binding,Event &out){
  out=Event{};
  if(!valid(binding))return false;
  const int n=snprintf(out.json,sizeof(out.json),
    "{\"schema_version\":1,\"event_type\":\"heartbeat\",\"device_id\":\"%s\","
    "\"meter_id\":\"%s\",\"meter_profile\":\"%s\"}",
    binding.device,binding.meter,binding.profile);
  if(n<=0||n>=int(sizeof(out.json)))return false;
  out.size=size_t(n);return true;
}
// Before intentionally moving the RTC backward, any enabled cloud uploader
// must first verify its persisted ACK cursor. An unreadable old ACK must not
// become an implicit "never uploaded" cursor at day zero.
inline bool rtcRepairAckSafe(bool uplinkEnabled,bool cursorVerified){
  return !uplinkEnabled||cursorVerified;
}
// Missing cursor files are normal for a fresh device. Existing corrupt
// cursor files are NOT the same thing: do not silently restart history at zero.
inline bool cursorHistoryUnverifiable(bool existsA,bool existsB,bool validA,bool validB){
  return (existsA||existsB)&&!validA&&!validB;
}
// Backfill never moves a verified ACK cursor forward just to catch up.
// A historic gap may block archive delivery, but not fresh telemetry/presence.
inline bool backfillWindowExceeded(uint32_t latest,uint32_t acknowledged,uint32_t days){
  return acknowledged&&latest>acknowledged&&latest-acknowledged>days;
}
// Treat an HTTP error as a permanent device fault only when the HTTPS
// application returned its exact documented rejection code. Reverse proxies,
// captive portals and rolling deploys may emit the same numeric status with a
// different body; those are retryable (with bounded backoff), never accepted.
// The website's ingest contract is in iot-contract.js and server/iot.js.
inline bool confirmedIngestRejection(int http,const char *error){
  if(!error)return false;
  switch(http){
    case 400:return !strcmp(error,"INVALID_IOT_EVENT")||!strcmp(error,"INVALID_INPUT");
    case 401:return !strcmp(error,"DEVICE_UNAUTHORIZED");
    case 403:return !strcmp(error,"DEVICE_BINDING_MISMATCH");
    case 409:return !strcmp(error,"EVENT_CONFLICT");
    case 413:return !strcmp(error,"BODY_TOO_LARGE");
    case 415:return !strcmp(error,"INVALID_CONTENT_TYPE");
    default:return false;
  }
}
// A reading timeout must not suppress the independently scheduled heartbeat.
// The IoT worker uses ESP-IDF's 64-bit monotonic time, independent of DS3231,
// to prevent stale HTTP backoff deadlines from reactivating after months uptime.
inline bool retryDelayPending(uint64_t now,uint64_t deadline){
  return deadline&&now<deadline;
}
// Live telemetry is a transient snapshot, unlike immutable history checkpoints.
// Expire an unsent/retried snapshot after one telemetry period, independently
// of wall-clock edits and the 32-bit millis() rollover. A replacement sample
// gets a fresh event ID; durable checkpoint event IDs are never discarded.
inline bool liveRetryExpired(uint64_t now,uint64_t createdAt,uint64_t maxAgeMs){
  return maxAgeMs&&now>=createdAt&&now-createdAt>=maxAgeMs;
}
// After a short burst of historical sends, allow due heartbeat/telemetry.
// Both still respect the caller's global HTTP spacing and quiet-window guard.
constexpr uint8_t BACKFILL_BURST_LIMIT=3;
inline bool yieldBackfill(uint8_t posts,bool liveDue,bool heartbeatDue){
  return posts>=BACKFILL_BURST_LIMIT&&(liveDue||heartbeatDue);
}
constexpr uint32_t TELEMETRY_PERIOD_MS=600000UL; // 10-minute accepted-reading cadence
constexpr uint32_t MANUAL_REQUEST_TTL_SECONDS=120UL;
// Command is only acted on after a later valid Modbus sample, not a cached
// local snapshot. Prevent stale/replayed cloud requests from causing uploads.
inline bool manualRefreshDue(uint32_t requestUTC,uint32_t servedUTC,
                             uint32_t sampleUTC,uint32_t nowUTC){
  return requestUTC>=946684800UL&&requestUTC>servedUTC&&
    nowUTC>=requestUTC&&nowUTC-requestUTC<=MANUAL_REQUEST_TTL_SECONDS&&
    sampleUTC>requestUTC&&sampleUTC<=nowUTC+5UL;
}
constexpr uint32_t HEARTBEAT_PERIOD_MS=60000UL;
inline bool heartbeatDue(uint32_t now,uint32_t lastAttempt){
  return !lastAttempt||uint32_t(now-lastAttempt)>=HEARTBEAT_PERIOD_MS;
}
// Caller must parse a complete JSON object and reject malformed/overflow
// responses. This pure identity rule is tested by native regressions.
inline bool heartbeatAckFields(int http,const char *type,const char *device,
                               const Binding &binding){
  return http==200&&type&&device&&valid(binding)&&
         strcmp(type,"heartbeat")==0&&strcmp(device,binding.device)==0;
}

inline uint32_t bindingFingerprint(const Binding &b){
  uint32_t h=2166136261UL;
  for(const char *s:{b.device,b.meter,b.profile}){
    for(;*s;++s){h^=uint8_t(*s);h*=16777619UL;}
    h^=0xff;h*=16777619UL;
  }
  return h;
}
inline bool later(uint32_t day,unsigned index,uint32_t ackDay,unsigned ackIndex){
  return day>ackDay||(day==ackDay&&index>ackIndex);
}
} } // namespace fort::iot
