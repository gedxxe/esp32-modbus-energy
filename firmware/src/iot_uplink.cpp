// SPDX-License-Identifier: GPL-3.0-only
// Development-only, opt-in HTTPS uplink. Existing CRC-verified captures are the
// durable source; the small dual-bank cursor is advanced only on server ACK.
#include "iot_uplink.h"
#include "iot_contract.h"
#include "iot_provision.h"
#include "iot_archive_path.h"
#include "iot_tls_diagnostics.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_system.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <mbedtls/ssl.h>
#include <sdkconfig.h>
#include <sys/time.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <strings.h>

// A positive build marker proves that this translation unit saw the private
// local header. Neither the marker nor /api/status exposes credentials.
#if defined(__has_include)
# if __has_include("../include/iot_credentials.local.h")
#  include "../include/iot_credentials.local.h"
#  define IOT_LOCAL_HEADER_PRESENT 1
# else
#  define IOT_LOCAL_HEADER_PRESENT 0
# endif
#else
# define IOT_LOCAL_HEADER_PRESENT 0
#endif
#ifndef IOT_DEV_ENABLED
#define IOT_DEV_ENABLED 0
#endif
#ifndef IOT_DEV_ENDPOINT
#define IOT_DEV_ENDPOINT ""
#endif
#ifndef IOT_DEV_DEVICE_ID
#define IOT_DEV_DEVICE_ID ""
#endif
#ifndef IOT_DEV_METER_ID
#define IOT_DEV_METER_ID ""
#endif
#ifndef IOT_DEV_METER_PROFILE
#define IOT_DEV_METER_PROFILE ""
#endif
#ifndef IOT_DEV_BEARER
#define IOT_DEV_BEARER ""
#endif
#if defined(IOT_EXPECT_LOCAL_PROVISION) && IOT_EXPECT_LOCAL_PROVISION && !IOT_LOCAL_HEADER_PRESENT
#error "Local IoT credential header was found by PlatformIO but not included by firmware/src/iot_uplink.cpp"
#endif
#if IOT_DEV_ENABLED && (!defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE) || !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
#error "IoT HTTPS requires CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y (TLS verification cannot be disabled)"
#endif
#if IOT_DEV_ENABLED && (!defined(CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS) || !CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS)
#error "IoT HTTPS requires CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y"
#endif
#if IOT_DEV_ENABLED && (!defined(CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC) || !CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC)
#error "IoT HTTPS needs CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y in the compiled ESP32-S3 N16R8 SDK"
#endif
#if IOT_DEV_ENABLED
static_assert(-MBEDTLS_ERR_SSL_ALLOC_FAILED==0x7F00,"Unexpected Mbed TLS allocation failure code");
#endif

namespace fort { namespace iot {
namespace {
inline uint64_t uptimeMs(){return uint64_t(esp_timer_get_time()/1000LL);}
constexpr char endpoint[]=IOT_DEV_ENDPOINT;
constexpr char token[]=IOT_DEV_BEARER;
constexpr char deviceId[]=IOT_DEV_DEVICE_ID;
constexpr char meterId[]=IOT_DEV_METER_ID;
constexpr char meterProfile[]=IOT_DEV_METER_PROFILE;
// The 60-second authenticated heartbeat remains independent of telemetry.
constexpr uint32_t MIN_SEND_PERIOD_MS=2100; // well below website's 120/min quota
constexpr uint32_t BACKFILL_DAYS=30;
constexpr size_t CURSOR_SIZE=20;
constexpr char ACK_A[]="/iot_ack_a.bin",ACK_B[]="/iot_ack_b.bin";
struct Input {
  CaptureDay active{},previous{};
  bool durable=false,station=false,clock=false,live=false,currentValid=false;
  uint32_t utc=0,liveCapturedUTC=0;
  double liveEnergy=0,liveCurrentA=0;
  uint8_t profile=0;
};
struct Cursor {
  uint32_t day=0,sequence=0;
  uint8_t index=255,bank=0;
};
struct Response {
  char body[256]={};
  size_t used=0;
  uint32_t retrySeconds=0;
  bool overflow=false;
};
struct PostResult {
  int http=0;
  int32_t transportError=0,socketErrno=0,tlsError=0,tlsFlags=0;
};
QueueHandle_t incoming=nullptr,statusQ=nullptr;
SemaphoreHandle_t filesystem=nullptr;
bool mounted=false,ready=false;
Binding binding{};
Status status{};
Cursor ack{};
uint32_t fingerprint=0;
uint32_t bootId=0,liveSequence=0,lastSend=0,lastLive=0;
// 64-bit monotonic deadlines must not revive after millis() wraps or a long
// offline outage; DS3231 UTC remains authoritative only for captures/TLS time.
uint64_t notBefore=0,serverRateLimitUntil=0,heartbeatNotBefore=0;
uint32_t pendingManualUTC=0,outstandingManualUTC=0,servedManualUTC=0;
uint32_t lastHeartbeatAttempt=0;
uint8_t consecutiveFailures=0,heartbeatConsecutiveFailures=0;
Event outstandingLive{};
bool hasOutstandingLive=false;

void report(const char *reason,uint8_t phase,int http=0){
  status.phase=phase;status.httpStatus=http;
  snprintf(status.error,sizeof(status.error),"%s",reason?reason:"");
  if(statusQ)xQueueOverwrite(statusQ,&status);
}
void reportAck(){
  status.lastAckDay=ack.day;status.lastAckSlot=ack.index;
  if(statusQ)xQueueOverwrite(statusQ,&status);
}
// Pure validator shared with native regressions. No secret values are logged.
const char *provisionIssue(){
  return validateProvision(IOT_DEV_ENABLED!=0,binding,token,endpoint,
                           IOT_LOCAL_HEADER_PRESENT!=0);
}
void put32(uint8_t *p,uint32_t v){
  for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));
}
uint32_t get32(const uint8_t *p){
  return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
}
void cursorBytes(const Cursor &c,uint8_t out[CURSOR_SIZE]){
  memset(out,0,CURSOR_SIZE);memcpy(out,"IACK",4);
  out[4]=1;out[5]=c.index;
  put32(out+6,c.day);put32(out+10,c.sequence);put32(out+14,fingerprint);
  const uint16_t crc=crc16(out,CURSOR_SIZE-2);
  out[18]=uint8_t(crc);out[19]=uint8_t(crc>>8);
}
bool decodeCursor(const uint8_t data[CURSOR_SIZE],Cursor &out){
  if(memcmp(data,"IACK",4)||data[4]!=1||data[5]>=CAPTURE_COUNT||
     get32(data+14)!=fingerprint||
     crc16(data,CURSOR_SIZE-2)!=(uint16_t(data[18])|(uint16_t(data[19])<<8)))return false;
  out.index=data[5];out.day=get32(data+6);out.sequence=get32(data+10);
  return validCaptureDayNumber(out.day)&&out.sequence!=0;
}
bool readCursorFile(const char *path,Cursor &candidate){
  File f=LittleFS.open(path,"r");
  if(!f)return false;
  uint8_t bytes[CURSOR_SIZE];
  bool ok=f.size()==CURSOR_SIZE&&f.read(bytes,sizeof(bytes))==sizeof(bytes);
  f.close();return ok&&decodeCursor(bytes,candidate);
}
enum class CursorLoad : uint8_t { Ready, Busy, Corrupt };
CursorLoad loadCursor(){
  if(xSemaphoreTake(filesystem,pdMS_TO_TICKS(200))!=pdTRUE)return CursorLoad::Busy;
  const bool existsA=LittleFS.exists(ACK_A),existsB=LittleFS.exists(ACK_B);
  Cursor a{},b{};
  const bool first=readCursorFile(ACK_A,a),second=readCursorFile(ACK_B,b);
  xSemaphoreGive(filesystem);
  if(cursorHistoryUnverifiable(existsA,existsB,first,second))
    return CursorLoad::Corrupt; // Keep both files for controlled recovery.
  ack=Cursor{};
  if(first&&(!second||a.sequence>=b.sequence)){ack=a;ack.bank=0;}
  else if(second){ack=b;ack.bank=1;}
  reportAck();return CursorLoad::Ready;
}
bool saveCursor(uint32_t day,uint8_t index){
  Cursor next=ack;next.day=day;next.index=index;next.sequence=ack.sequence+1;
  if(!next.sequence)next.sequence=1;
  next.bank=ack.bank==0?1:0;
  uint8_t bytes[CURSOR_SIZE],verified[CURSOR_SIZE];
  cursorBytes(next,bytes);
  const char *path=next.bank==0?ACK_A:ACK_B;
  if(xSemaphoreTake(filesystem,pdMS_TO_TICKS(200))!=pdTRUE)return false;
  File file=LittleFS.open(path,"w");
  bool ok=bool(file);
  if(ok){ok=file.write(bytes,sizeof(bytes))==sizeof(bytes);file.flush();file.close();}
  if(ok){
    File readback=LittleFS.open(path,"r");
    ok=readback&&readback.size()==CURSOR_SIZE&&readback.read(verified,sizeof(verified))==sizeof(verified);
    if(readback)readback.close();
    Cursor parsed;
    ok=ok&&!memcmp(bytes,verified,sizeof(bytes))&&decodeCursor(verified,parsed);
  }
  xSemaphoreGive(filesystem);
  if(!ok)return false;
  ack=next;reportAck();return true;
}
enum ArchiveRead:uint8_t { ARCHIVE_ABSENT, ARCHIVE_OK, ARCHIVE_RETRY, ARCHIVE_INVALID };
ArchiveRead readArchive(uint32_t day,CaptureDay &out){
  if(!mounted||xSemaphoreTake(filesystem,pdMS_TO_TICKS(100))!=pdTRUE)return ARCHIVE_RETRY;
  bool found=false,present=false;
  for(bool backup:{false,true}){
    char path[40]={};
    if(!archivePath(day,backup,path,sizeof(path))){
      xSemaphoreGive(filesystem);return ARCHIVE_INVALID;
    }
    File f=LittleFS.open(path,"r");
    if(!f){
      if(LittleFS.exists(path))present=true; // unreadable existing archive
      continue;
    }
    present=true;
    const size_t bytesCount=f.size();
    if(captureRecordSize(bytesCount)){
      uint8_t bytes[CAPTURE_RECORD_SIZE];
      CaptureDay candidate{};
      if(f.read(bytes,bytesCount)==bytesCount&&decodeCaptureDay(bytes,bytesCount,candidate)&&
         candidate.number==day&&(!found||newerCaptureDay(candidate,out))){
        out=candidate;found=true;
      }
    }
    f.close();
  }
  xSemaphoreGive(filesystem);
  return found?ARCHIVE_OK:present?ARCHIVE_INVALID:ARCHIVE_ABSENT;
}
bool receiptMatches(const Response &response,const Event &event){
  if(response.overflow||!response.used)return false;
  JsonDocument doc;
  if(deserializeJson(doc,response.body, response.used))return false;
  const char *id=doc["event_id"] | "";
  return strcmp(id,event.id)==0;
}
// The website heartbeat ACK is separate from event_id receipt semantics.
bool heartbeatReceiptMatches(const Response &response,int http){
  if(response.overflow||!response.used||http!=200)return false;
  JsonDocument doc;
  if(deserializeJson(doc,response.body,response.used)||!doc.is<JsonObject>())return false;
  const char *kind=doc["event_type"] | "";
  const char *device=doc["device_id"] | "";
  return heartbeatAckFields(http,kind,device,binding);
}
esp_err_t onHttp(esp_http_client_event_t *event){
  Response *r=static_cast<Response*>(event->user_data);
  if(!r)return ESP_OK;
  if(event->event_id==HTTP_EVENT_ON_DATA&&event->data_len>0){
    const size_t n=size_t(event->data_len);
    if(r->used+n>=sizeof(r->body))r->overflow=true;
    else{memcpy(r->body+r->used,event->data,n);r->used+=n;r->body[r->used]=0;}
  }
  if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&
     !strcasecmp(event->header_key,"Retry-After")){
    char *end=nullptr;unsigned long seconds=strtoul(event->header_value,&end,10);
    if(end!=event->header_value&&!*end)r->retrySeconds=uint32_t(std::min(seconds,900UL));
  }
  return ESP_OK;
}
// The no-data probe uses GET on the same HTTPS origin with NO bearer and NO
// payload. A 405 response proves only HTTPS routing, not device authorization.
PostResult request(const Event *event,Response &response){
  PostResult result{};
  if(event){
    status.freeHeapBeforePost=esp_get_free_heap_size();
    const uint32_t internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    const uint32_t external=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
    status.internalFreeBeforePost=heap_caps_get_free_size(internal);
    status.internalLargestBeforePost=heap_caps_get_largest_free_block(internal);
    status.psramFreeBeforePost=heap_caps_get_free_size(external);
    status.psramLargestBeforePost=heap_caps_get_largest_free_block(external);
  }else status.freeHeapBeforeProbe=esp_get_free_heap_size();
  esp_http_client_config_t cfg={};
  cfg.url=endpoint;cfg.method=event?HTTP_METHOD_POST:HTTP_METHOD_GET;
  cfg.crt_bundle_attach=esp_crt_bundle_attach;
  cfg.skip_cert_common_name_check=false;
  cfg.timeout_ms=6000;cfg.disable_auto_redirect=true;
  cfg.event_handler=onHttp;cfg.user_data=&response;
  esp_http_client_handle_t client=esp_http_client_init(&cfg);
  if(!client){result.transportError=ESP_ERR_NO_MEM;return result;}
  char auth[64]={};
  if(event){
    const int written=snprintf(auth,sizeof(auth),"Bearer %s",token);
    esp_err_t prepared=written>0&&written<int(sizeof(auth))?ESP_OK:ESP_ERR_INVALID_SIZE;
    if(prepared==ESP_OK)prepared=esp_http_client_set_header(client,"Content-Type","application/json");
    if(prepared==ESP_OK)prepared=esp_http_client_set_header(client,"Authorization",auth);
    if(prepared==ESP_OK)prepared=esp_http_client_set_header(client,"X-Device-ID",binding.device);
    if(prepared==ESP_OK)prepared=esp_http_client_set_post_field(client,event->json,int(event->size));
    if(prepared!=ESP_OK){
      result.transportError=prepared;
      esp_http_client_cleanup(client);
      memset(auth,0,sizeof(auth));
      return result;
    }
  }
  const esp_err_t transport=esp_http_client_perform(client);
  if(transport==ESP_OK)result.http=esp_http_client_get_status_code(client);
  else{
    result.transportError=transport;
    result.socketErrno=esp_http_client_get_errno(client);
    int tlsError=0,tlsFlags=0;
    esp_http_client_get_and_clear_last_tls_error(client,&tlsError,&tlsFlags);
    result.tlsError=tlsError;
    result.tlsFlags=tlsFlags;
  }
  esp_http_client_cleanup(client);
  memset(auth,0,sizeof(auth));
  return result;
}
PostResult post(const Event &event,Response &response){return request(&event,response);}
void backoff(const char *reason,uint32_t retrySeconds=0){
  ++status.failures;
  if(consecutiveFailures<7)++consecutiveFailures;
  uint32_t delayMs=5000UL<<std::min(unsigned(consecutiveFailures-1),6U);
  if(delayMs>300000)delayMs=300000;
  if(retrySeconds)delayMs=std::max(delayMs,std::min(retrySeconds,900UL)*1000UL);
  delayMs+=esp_random()%1500UL;
  notBefore=uptimeMs()+delayMs;
  if(reason&&!strcmp(reason,"http_rate_limited"))
    serverRateLimitUntil=notBefore;
  report(reason,3,status.httpStatus);
}
bool serverError(const Response &response,const char *code){
  if(response.overflow||!response.used)return false;
  JsonDocument doc;
  if(deserializeJson(doc,response.body,response.used))return false;
  const char *error=doc["error"] | "";
  return strcmp(error,code)==0;
}
// Independent heartbeat retry state: failed contact must never postpone a
// freshly captured reading or mutate the durable checkpoint cursor.
void heartbeatBackoff(const char *reason,uint32_t retrySeconds=0){
  ++status.heartbeatFailures;
  if(heartbeatConsecutiveFailures<7)++heartbeatConsecutiveFailures;
  uint32_t delayMs=5000UL<<std::min(unsigned(heartbeatConsecutiveFailures-1),6U);
  if(delayMs>300000UL)delayMs=300000UL;
  if(retrySeconds)delayMs=std::max(delayMs,std::min(retrySeconds,900UL)*1000UL);
  delayMs+=esp_random()%1500UL;
  heartbeatNotBefore=uptimeMs()+delayMs;
  if(reason&&!strcmp(reason,"heartbeat_rate_limited"))
    serverRateLimitUntil=heartbeatNotBefore;
  snprintf(status.heartbeatError,sizeof(status.heartbeatError),"%s",reason);
  report(reason,3,status.heartbeatHttp);
}
bool transmitHeartbeat(uint32_t utc){
  Event contact{};
  if(!heartbeat(binding,contact)){
    snprintf(status.heartbeatError,sizeof(status.heartbeatError),"%s","heartbeat_payload_invalid");
    report("heartbeat_payload_invalid",4);
    return false;
  }
  ++status.heartbeatAttempts;
  lastHeartbeatAttempt=millis();
  lastSend=lastHeartbeatAttempt; // same global minimum request spacing as readings
  report("heartbeat_uploading",2);
  Response response{};
  // request() uses the same verified CA bundle, no redirects, bounded timeout,
  // X-Device-ID and bearer headers as an ordinary reading POST.
  const PostResult result=post(contact,response);
  const int code=result.http;
  status.httpStatus=code;
  status.transportError=result.transportError;
  status.socketErrno=result.socketErrno;
  status.tlsError=result.tlsError;
  status.tlsFlags=result.tlsFlags;
  status.heartbeatHttp=code;
  status.heartbeatTransportError=result.transportError;
  status.heartbeatTlsError=result.tlsError;
  status.heartbeatTlsFlags=result.tlsFlags;
  if(heartbeatReceiptMatches(response,code)){
    ++status.heartbeatAccepted;
    status.lastHeartbeatUTC=utc;
    heartbeatConsecutiveFailures=0;
    heartbeatNotBefore=0;
    status.heartbeatError[0]=0;
    // Optional command is carried by the existing authenticated heartbeat ACK.
    // Older website versions omit it; never trust unmatched/overflowed replies.
    JsonDocument reply;
    if(!deserializeJson(reply,response.body,response.used)){
      const uint32_t requested=reply["refresh_after_utc"] | 0UL;
      if(requested>servedManualUTC&&requested>=946684800UL&&
         utc>=requested&&utc-requested<=MANUAL_REQUEST_TTL_SECONDS){
        pendingManualUTC=requested;
        status.lastManualRequestedUTC=requested;
      }
    }
    report(mounted?"heartbeat_accepted":"heartbeat_accepted_storage_unavailable",1,200);
    return true;
  }
  // A 200 without the exact heartbeat and registered device ID is NOT contact.
  if(code==200){
    ++status.heartbeatFailures;
    snprintf(status.heartbeatError,sizeof(status.heartbeatError),"%s","heartbeat_ack_mismatch");
    report("heartbeat_ack_mismatch",4,code);
    return false;
  }
  if(code==400||code==401||code==403||code==409||code==413||code==415){
    ++status.heartbeatFailures;
    const char *reason="heartbeat_binding_or_payload_invalid";
    if(code==401)reason=serverError(response,"DEVICE_UNAUTHORIZED")?
      "heartbeat_device_unauthorized":"heartbeat_401_upstream_protection";
    else if(code==403)reason=serverError(response,"DEVICE_BINDING_MISMATCH")?
      "heartbeat_binding_mismatch":"heartbeat_403_upstream_protection";
    else if(code==415)reason="heartbeat_content_type_invalid";
    snprintf(status.heartbeatError,sizeof(status.heartbeatError),"%s",reason);
    report(reason,4,code);
    return false;
  }
  if(code==301||code==302||code==303||code==307||code==308||code==404){
    ++status.heartbeatFailures;
    const char *reason=code==404?"heartbeat_endpoint_not_found":"heartbeat_redirect_blocked";
    snprintf(status.heartbeatError,sizeof(status.heartbeatError),"%s",reason);
    report(reason,4,code);
    return false;
  }
  if(code==503&&serverError(response,"IOT_NOT_CONFIGURED")){
    ++status.heartbeatFailures;
    snprintf(status.heartbeatError,sizeof(status.heartbeatError),"%s","heartbeat_cloud_disabled");
    report("heartbeat_cloud_disabled",4,code);
    return false;
  }
  if(code==429){
    heartbeatBackoff("heartbeat_rate_limited",response.retrySeconds);
    return false;
  }
  if(!code){
    const char *reason=result.tlsFlags?"heartbeat_tls_certificate_rejected":
      tlsAllocationFailure(result.tlsError)?"heartbeat_tls_out_of_memory":
      result.tlsError?"heartbeat_tls_handshake_failed":
      result.transportError==ESP_ERR_NO_MEM?"heartbeat_http_no_memory":
      result.transportError?"heartbeat_transport_failed":"heartbeat_no_response";
    heartbeatBackoff(reason);
    return false;
  }
  heartbeatBackoff(code>=500?"heartbeat_http_server_error":"heartbeat_unexpected_http_status");
  return false;
}
bool transmit(const Event &event,bool checkpointEvent){
  Response response{};
  report("uploading",2);
  lastSend=millis();
  const PostResult result=post(event,response);
  const int code=result.http;
  status.httpStatus=code;
  status.transportError=result.transportError;
  status.socketErrno=result.socketErrno;
  status.tlsError=result.tlsError;
  status.tlsFlags=result.tlsFlags;
  if((code==200||code==201)&&receiptMatches(response,event)){
    if(checkpointEvent&&!saveCursor(event.day,event.index)){
      // A server receipt is not durable until its local ACK cursor is saved.
      // Retry the SAME immutable event ID (server deduplicates it), rather
      // than losing all future live telemetry after one transient FS failure.
      snprintf(status.archiveError,sizeof(status.archiveError),"%s","ack_storage_failed");
      backoff("ack_storage_failed");return false;
    }
    if(checkpointEvent)status.archiveError[0]=0;
    if(code==200)++status.duplicates;else ++status.accepted;
    status.lastAcceptedUTC=uint32_t(time(nullptr));
    consecutiveFailures=0;notBefore=0;
    report("accepted",1,code);return true;
  }
  if(code==400||code==401||code==403||code==409||code==413||code==415){
    const char *reason="review_binding_or_payload";
    if(code==401)reason=serverError(response,"DEVICE_UNAUTHORIZED")?
      "cloud_device_unauthorized":"http_401_upstream_protection";
    else if(code==403)reason=serverError(response,"DEVICE_BINDING_MISMATCH")?
      "cloud_binding_mismatch":"http_403_upstream_protection";
    else if(code==409)reason="event_id_conflict";
    else if(code==415)reason="invalid_content_type";
    report(reason,4,code);
    return false;
  }
  if(code==200||code==201){
    report("receipt_id_mismatch",4,code);return false;
  }
  // Never follow a redirect: doing so risks sending the device bearer
  // to a different host. A 404 usually means an invalid deployment/path.
  if(code==301||code==302||code==303||code==307||code==308||code==404){
    report(code==404?"endpoint_not_found":"endpoint_redirect_blocked",4,code);
    return false;
  }
  if(code==503&&serverError(response,"IOT_NOT_CONFIGURED")){
    report("cloud_iot_disabled",4,code);
    return false;
  }
  if(code==429){backoff("http_rate_limited",response.retrySeconds);return false;}
  if(!code){
    const char *cause=result.tlsFlags?"tls_certificate_rejected":
      tlsAllocationFailure(result.tlsError)?"tls_out_of_memory":
      result.tlsError?"tls_handshake_failed":
      result.transportError==ESP_ERR_NO_MEM?"http_no_memory":
      result.transportError?"http_transport_failed":"http_no_response";
    backoff(cause);return false;
  }
  backoff(code>=500?"http_server_error":"http_unexpected_status");
  return false;
}
enum class ArchiveAttempt : uint8_t { None, Sent, Paused };
ArchiveAttempt uploadNextCheckpoint(const Input &input){
  if(!input.active.number||!input.durable)return ArchiveAttempt::None;
  const uint32_t latest=input.active.number;
  if(backfillWindowExceeded(latest,ack.day,BACKFILL_DAYS)){
    // Retention may no longer contain the oldest unacknowledged day.
    // Never advance the cursor or fabricate acknowledgements; the operator
    // must export/reconcile the historical gap, while new traffic continues.
    snprintf(status.archiveError,sizeof(status.archiveError),"%s","archive_window_exceeded");
    report("archive_window_exceeded",3);
    return ArchiveAttempt::Paused;
  }
  status.archiveError[0]=0;
  const uint32_t earliest=ack.day?ack.day:(latest>BACKFILL_DAYS?latest-BACKFILL_DAYS:1);
  for(uint32_t day=earliest;day<=latest;++day){
    CaptureDay record{};
    bool found=false;
    if(day==input.active.number){record=input.active;found=true;}
    else if(day==input.previous.number&&input.previous.number){record=input.previous;found=true;}
    else{
      const ArchiveRead result=readArchive(day,record);
      if(result==ARCHIVE_RETRY){
        notBefore=uptimeMs()+2000UL;
        report("archive_read_retry",3);
        return ArchiveAttempt::Paused; // Never skip a busy archive.
      }
      if(result==ARCHIVE_INVALID){
        snprintf(status.archiveError,sizeof(status.archiveError),"%s","archive_corrupt_needs_review");
        report("archive_corrupt_needs_review",3);
        return ArchiveAttempt::Paused; // Preserve cursor and require review.
      }
      found=result==ARCHIVE_OK;
    }
    if(!found)continue;
    for(unsigned i=0;i<CAPTURE_COUNT;++i){
      if(!later(day,i,ack.day,ack.index))continue;
      const auto &capture=record.points[i];
      if(capture.status!=CAPTURE_RECORDED)continue;
      Event event{};
      if(!checkpoint(record,i,binding,event)){
        // Ignore nonmatching historical meter profiles, never reinterpret them.
        continue;
      }
      transmit(event,true);
      return ArchiveAttempt::Sent; // at most one HTTP request per cycle
    }
  }
  return ArchiveAttempt::None;
}
void worker(void *){
  Input input{};
  // A failed LittleFS mount must not suppress authenticated presence. Archive
  // transfer stays disabled until a normal boot successfully mounts storage.
  // Acquisition can own fsMutex during boot archive recovery. A transient
  // lock contention must not permanently delete the HTTPS worker task.
  bool archiveCursorReady=!mounted;
  uint64_t cursorRetryAt=0,nextArchiveCheck=0;
  uint32_t lastRead=0,observedDay=0,observedRevision=0;
  uint8_t backfillBurst=0;
  for(;;){
    Input next{};
    if(xQueueReceive(incoming,&next,pdMS_TO_TICKS(1000))==pdTRUE)input=next;
    if(mounted&&!archiveCursorReady&&
       !retryDelayPending(uptimeMs(),cursorRetryAt)){
      const CursorLoad result=loadCursor();
      if(result==CursorLoad::Ready){
        archiveCursorReady=true;
        cursorRetryAt=0;
        if(!strcmp(status.archiveError,"archive_lock_busy")||
           !strcmp(status.archiveError,"archive_cursor_corrupt"))
          status.archiveError[0]=0;
        report("waiting_for_wifi_rtc",1);
      }else{
        const char *reason=result==CursorLoad::Corrupt?
          "archive_cursor_corrupt":"archive_lock_busy";
        // Corrupt cursor bytes remain intact for offline inspection. Retrying
        // may recover a transient read failure without ever resetting ACK.
        cursorRetryAt=uptimeMs()+(result==CursorLoad::Corrupt?30000ULL:2000ULL);
        snprintf(status.archiveError,sizeof(status.archiveError),"%s",reason);
        report(reason,3);
      }
    }
    if(!input.station||!input.clock||!input.utc){
      if(status.phase!=4)report("waiting_for_wifi_rtc",1);
      continue;
    }
    if(status.phase==4)continue;
    uint32_t now=millis();
    const uint64_t monotonic=uptimeMs();
    if(retryDelayPending(monotonic,serverRateLimitUntil)||
       uint32_t(now-lastSend)<MIN_SEND_PERIOD_MS)continue;
    const bool readingPaused=retryDelayPending(monotonic,notBefore);
    // Keep HTTPS handshakes off the five-second Modbus checkpoint window.
    if(checkpointWindowProtected(input.utc)){
      if(status.phase!=1||strcmp(status.error,"checkpoint_window_hold"))
        report("checkpoint_window_hold",1);
      continue;
    }
    // The DS3231 is authoritative. Set only the TCP/TLS system clock, never RTC hardware.
    if(uint32_t(now-lastRead)>60000||!lastRead){
      time_t systemUTC=time(nullptr);
      if(systemUTC<time_t(input.utc-5)||systemUTC>time_t(input.utc+5)){
        timeval tv{};tv.tv_sec=input.utc;
        if(settimeofday(&tv,nullptr)!=0){backoff("tls_clock_invalid");continue;}
      }
      lastRead=now;
    }
    // A new captured checkpoint should be offered immediately; with no records,
    // avoid repeatedly opening 31 days of LittleFS archives every loop tick.
    if(input.active.number!=observedDay||input.active.revision!=observedRevision){
      observedDay=input.active.number;observedRevision=input.active.revision;
      nextArchiveCheck=0;
    }
    // Due real-time traffic must not wait behind hundreds of historic
    // checkpoint uploads after a long Wi-Fi outage.
    const bool requested=manualRefreshDue(pendingManualUTC,servedManualUTC,
                                         input.liveCapturedUTC,input.utc);
    const bool periodic=!lastLive||uint32_t(now-lastLive)>=TELEMETRY_PERIOD_MS;
    const bool liveDue=!readingPaused&&mounted&&input.live&&profiles::valid(input.profile)&&
      !strcmp(profiles::get(input.profile).key,binding.profile)&&(periodic||requested);
    const bool heartbeatReady=heartbeatDue(now,lastHeartbeatAttempt)&&
      !retryDelayPending(monotonic,heartbeatNotBefore);
    if(mounted&&!readingPaused&&archiveCursorReady&&
       !retryDelayPending(monotonic,nextArchiveCheck)&&
       !yieldBackfill(backfillBurst,hasOutstandingLive||liveDue,heartbeatReady)){
      const ArchiveAttempt attempt=uploadNextCheckpoint(input);
      if(attempt==ArchiveAttempt::Sent){
        if(backfillBurst<BACKFILL_BURST_LIMIT)++backfillBurst;
        nextArchiveCheck=0;
        continue;
      }
      // Do not spin on a missing, invalid or temporarily busy old archive.
      // Its failure stays visible in archiveError; new data still flows.
      nextArchiveCheck=monotonic+15000UL;
    }
    if(status.phase==4)continue;
    // Presence uses the same authenticated HTTPS route as readings.
    if(mounted&&!hasOutstandingLive&&liveDue){
      ++liveSequence;
      if(!liveSequence)++liveSequence;
      if(telemetry(input.liveCapturedUTC,input.liveEnergy,binding,bootId,liveSequence,outstandingLive,
                   input.currentValid,input.liveCurrentA)){
        hasOutstandingLive=true;
        outstandingManualUTC=requested?pendingManualUTC:0;
      }
    }
    // A queued reading always has priority; heartbeat cannot starve backlog.
    // Both use the same request spacing, verified TLS and quiet-window guard.
    if(hasOutstandingLive&&!readingPaused){
      backfillBurst=0;
      if(transmit(outstandingLive,false)){
        hasOutstandingLive=false;lastLive=millis();
        if(outstandingManualUTC){
          servedManualUTC=outstandingManualUTC;
          status.lastManualServedUTC=servedManualUTC;
          if(pendingManualUTC==servedManualUTC)pendingManualUTC=0;
        }
        outstandingManualUTC=0;
      }
      continue;
    }
    if(heartbeatReady){
      backfillBurst=0;
      transmitHeartbeat(input.utc);
      continue;
    }
    if(status.phase==1){
      // Keep a failed heartbeat visible until the next genuine signed ACK.
      const char *reason=status.heartbeatError[0]?status.heartbeatError:
        !status.heartbeatAccepted?"waiting_for_heartbeat":
        !mounted?"heartbeat_accepted_storage_unavailable":
        !input.live?"heartbeat_accepted_waiting_for_meter":"heartbeat_accepted";
      if(strcmp(status.error,reason))report(reason,1);
    }
  }
}
} // namespace

bool start(SemaphoreHandle_t filesystemMutex,bool filesystemMounted){
  binding=Binding{};
  snprintf(binding.device,sizeof(binding.device),"%s",deviceId);
  snprintf(binding.meter,sizeof(binding.meter),"%s",meterId);
  snprintf(binding.profile,sizeof(binding.profile),"%s",meterProfile);
  status=Status{};status.localHeaderPresent=IOT_LOCAL_HEADER_PRESENT!=0;
#if defined(CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC) && CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC
  status.tlsAllocatorPsram=true;
#endif
  const char *issue=provisionIssue();status.configured=issue==nullptr;
  if(!IOT_DEV_ENABLED){status.phase=0;return true;}
  if(issue){report(issue,4);return false;}
  filesystem=filesystemMutex;mounted=filesystemMounted;
  if(!filesystem){report("filesystem_mutex_unavailable",4);return false;}
  fingerprint=bindingFingerprint(binding);
  bootId=esp_random();if(!bootId)bootId=1;
  incoming=xQueueCreate(1,sizeof(Input));statusQ=xQueueCreate(1,sizeof(Status));
  if(!incoming||!statusQ){report("queue_allocation_failed",4);return false;}
  status.enabled=true;status.archiveReady=mounted;
  if(!mounted){
    snprintf(status.archiveError,sizeof(status.archiveError),"%s","filesystem_unavailable");
    report("filesystem_unavailable_heartbeat_only",1);
  }else report("waiting_for_wifi_rtc",1);
  TaskHandle_t handle=nullptr;
  if(xTaskCreatePinnedToCore(worker,"iot-https-uplink",12288,nullptr,1,&handle,0)!=pdPASS){
    status.enabled=false;report("task_allocation_failed",4);return false;
  }
  ready=true;return true;
}
void update(const CaptureDay &active,const CaptureDay &previous,
            bool captureDurable,bool stationOnline,bool clockValid,uint32_t utc,
            uint32_t liveCapturedUTC,bool telemetryValid,double telemetryKWh,uint8_t profile,
            bool currentValid,double currentA){
  if(!ready)return;
  // One core-0 producer. Keep two capture-day copies off loopTask's 16 KiB stack.
  static Input input{};
  input.active=active;input.previous=previous;input.durable=captureDurable;
  input.station=stationOnline;input.clock=clockValid;input.utc=utc;
  input.liveCapturedUTC=liveCapturedUTC;
  input.live=telemetryValid;input.liveEnergy=telemetryKWh;input.profile=profile;
  input.currentValid=currentValid;input.liveCurrentA=currentA;
  xQueueOverwrite(incoming,&input);
}
Status readStatus(){
  // The worker owns mutable state. Web/loop consumers read only the queued copy.
  Status out{};
  if(statusQ)xQueuePeek(statusQ,&out,0);
  else out=status; // startup/disabled state, before a worker exists
  return out;
}
} } // namespace fort::iot
