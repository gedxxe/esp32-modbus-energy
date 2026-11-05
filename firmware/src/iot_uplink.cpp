// SPDX-License-Identifier: GPL-3.0-only
// Development-only, opt-in HTTPS uplink. Existing CRC-verified captures are the
// durable source; the small dual-bank cursor is advanced only on server ACK.
#include "iot_uplink.h"
#include "iot_contract.h"
#include "iot_archive_path.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_system.h>
#include <sys/time.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <strings.h>

#if defined(__has_include)
# if __has_include("iot_credentials.local.h")
#  include "iot_credentials.local.h"
# endif
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

namespace fort { namespace iot {
namespace {
constexpr char endpoint[]=IOT_DEV_ENDPOINT;
constexpr char token[]=IOT_DEV_BEARER;
constexpr char deviceId[]=IOT_DEV_DEVICE_ID;
constexpr char meterId[]=IOT_DEV_METER_ID;
constexpr char meterProfile[]=IOT_DEV_METER_PROFILE;
constexpr uint32_t MIN_TELEMETRY_PERIOD_MS=30000;
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
QueueHandle_t incoming=nullptr,statusQ=nullptr;
SemaphoreHandle_t filesystem=nullptr;
bool mounted=false,ready=false;
Binding binding{};
Status status{};
Cursor ack{};
uint32_t fingerprint=0;
uint32_t bootId=0,liveSequence=0,lastSend=0,lastLive=0,notBefore=0;
uint8_t consecutiveFailures=0;
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
bool validProvision(){
  if(!IOT_DEV_ENABLED||!valid(binding))return false;
  const size_t len=strlen(token);
  if(len!=43)return false;
  for(size_t i=0;i<len;++i){
    char c=token[i];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
  }
  const char *prefix="https://";
  if(strncmp(endpoint,prefix,8))return false;
  const char *path=strstr(endpoint+8,"/api/iot-ingest");
  if(!path||strcmp(path,"/api/iot-ingest")||path==endpoint+8)return false;
  if(strchr(endpoint+8,'@')||strchr(endpoint+8,'#')||strchr(endpoint+8,'?'))return false;
  return strlen(endpoint)<256;
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
bool loadCursor(){
  if(xSemaphoreTake(filesystem,pdMS_TO_TICKS(200))!=pdTRUE)return false;
  Cursor a{},b{};bool first=readCursorFile(ACK_A,a),second=readCursorFile(ACK_B,b);
  xSemaphoreGive(filesystem);
  if(first&&(!second||a.sequence>=b.sequence)){ack=a;ack.bank=0;}
  else if(second){ack=b;ack.bank=1;}
  reportAck();return true;
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
int post(const Event &event,Response &response){
  esp_http_client_config_t cfg={};
  cfg.url=endpoint;cfg.method=HTTP_METHOD_POST;
  cfg.crt_bundle_attach=esp_crt_bundle_attach;
  cfg.skip_cert_common_name_check=false;
  cfg.timeout_ms=6000;cfg.disable_auto_redirect=true;
  cfg.event_handler=onHttp;cfg.user_data=&response;
  esp_http_client_handle_t client=esp_http_client_init(&cfg);
  if(!client)return 0;
  char auth[64]={};
  snprintf(auth,sizeof(auth),"Bearer %s",token);
  esp_http_client_set_header(client,"Content-Type","application/json");
  esp_http_client_set_header(client,"Authorization",auth);
  esp_http_client_set_header(client,"X-Device-ID",binding.device);
  esp_http_client_set_post_field(client,event.json,int(event.size));
  const esp_err_t result=esp_http_client_perform(client);
  const int http=result==ESP_OK?esp_http_client_get_status_code(client):0;
  esp_http_client_cleanup(client);
  memset(auth,0,sizeof(auth));
  return http;
}
void backoff(uint32_t retrySeconds=0){
  ++status.failures;
  if(consecutiveFailures<7)++consecutiveFailures;
  uint32_t delayMs=5000UL<<std::min(unsigned(consecutiveFailures-1),6U);
  if(delayMs>300000)delayMs=300000;
  if(retrySeconds)delayMs=std::max(delayMs,std::min(retrySeconds,900UL)*1000UL);
  delayMs+=esp_random()%1500UL;
  notBefore=millis()+delayMs;
  report("retry_scheduled",3,status.httpStatus);
}
bool transmit(const Event &event,bool checkpointEvent){
  Response response{};
  report("uploading",2);
  lastSend=millis();
  const int code=post(event,response);
  status.httpStatus=code;
  if((code==200||code==201)&&receiptMatches(response,event)){
    if(checkpointEvent&&!saveCursor(event.day,event.index)){
      report("ack_storage_failed",4,code);return false;
    }
    if(code==200)++status.duplicates;else ++status.accepted;
    status.lastAcceptedUTC=uint32_t(time(nullptr));
    consecutiveFailures=0;notBefore=0;
    report("accepted",1,code);return true;
  }
  if(code==400||code==401||code==403||code==409||code==413||code==415){
    report(code==401?"repair_credentials":"review_binding_or_payload",4,code);
    return false;
  }
  if(code==200||code==201){
    report("receipt_id_mismatch",4,code);return false;
  }
  report("network_or_service_failure",3,code);
  backoff(code==429?response.retrySeconds:0);
  return false;
}
bool uploadNextCheckpoint(const Input &input){
  if(!input.active.number||!input.durable)return false;
  const uint32_t latest=input.active.number;
  if(ack.day&&latest>ack.day+BACKFILL_DAYS){
    report("archive_window_exceeded",4);return false;
  }
  const uint32_t earliest=ack.day?ack.day:(latest>BACKFILL_DAYS?latest-BACKFILL_DAYS:1);
  for(uint32_t day=earliest;day<=latest;++day){
    CaptureDay record{};
    bool found=false;
    if(day==input.active.number){record=input.active;found=true;}
    else if(day==input.previous.number&&input.previous.number){record=input.previous;found=true;}
    else{
      const ArchiveRead result=readArchive(day,record);
      if(result==ARCHIVE_RETRY){
        notBefore=millis()+2000UL;
        report("archive_read_retry",3);
        return true; // Do not advance past a file busy with acquisition.
      }
      if(result==ARCHIVE_INVALID){
        report("archive_corrupt_needs_review",4);
        return true; // Fail closed: no silent loss of a damaged archived day.
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
      return true; // at most one HTTP request per service cycle
    }
  }
  return false;
}
void worker(void *){
  Input input{};
  if(!loadCursor()){report("archive_lock_unavailable",4);vTaskDelete(nullptr);return;}
  uint32_t lastRead=0;
  for(;;){
    Input next{};
    if(xQueueReceive(incoming,&next,pdMS_TO_TICKS(1000))==pdTRUE)input=next;
    if(!input.station||!input.clock||!mounted||!input.utc){
      if(status.phase!=4)report("waiting_for_wifi_rtc",1);
      continue;
    }
    if(status.phase==4)continue;
    uint32_t now=millis();
    if(int32_t(now-notBefore)<0||uint32_t(now-lastSend)<MIN_SEND_PERIOD_MS)continue;
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
        if(settimeofday(&tv,nullptr)!=0){report("tls_clock_invalid",3);backoff();continue;}
      }
      lastRead=now;
    }
    if(uploadNextCheckpoint(input))continue;
    if(status.phase==4)continue;
    if(!hasOutstandingLive&&input.live&&
       profiles::valid(input.profile)&&!strcmp(profiles::get(input.profile).key,binding.profile)&&
       uint32_t(now-lastLive)>=MIN_TELEMETRY_PERIOD_MS){
      ++liveSequence;
      if(!liveSequence)++liveSequence;
      if(telemetry(input.liveCapturedUTC,input.liveEnergy,binding,bootId,liveSequence,outstandingLive,
                   input.currentValid,input.liveCurrentA))
        hasOutstandingLive=true;
    }
    if(hasOutstandingLive){
      if(transmit(outstandingLive,false)){hasOutstandingLive=false;lastLive=millis();}
    }
  }
}
} // namespace

bool start(SemaphoreHandle_t filesystemMutex,bool filesystemMounted){
  binding=Binding{};
  snprintf(binding.device,sizeof(binding.device),"%s",deviceId);
  snprintf(binding.meter,sizeof(binding.meter),"%s",meterId);
  snprintf(binding.profile,sizeof(binding.profile),"%s",meterProfile);
  status=Status{};status.configured=validProvision();
  if(!IOT_DEV_ENABLED){status.phase=0;return true;}
  if(!status.configured){report("invalid_local_provisioning",4);return false;}
  filesystem=filesystemMutex;mounted=filesystemMounted;
  if(!filesystem||!mounted){report("filesystem_unavailable",4);return false;}
  fingerprint=bindingFingerprint(binding);
  bootId=esp_random();if(!bootId)bootId=1;
  incoming=xQueueCreate(1,sizeof(Input));statusQ=xQueueCreate(1,sizeof(Status));
  if(!incoming||!statusQ){report("queue_allocation_failed",4);return false;}
  status.enabled=true;status.archiveReady=true;report("waiting_for_wifi_rtc",1);
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
