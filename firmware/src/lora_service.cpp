// SPDX-License-Identifier: GPL-3.0-only
#include "lora_service.h"
#include "lora_irq_policy.h"
#include "board.h"
#include "version.h"
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <mbedtls/md.h>
namespace fort {
namespace {
using namespace lora;
struct Update {uint32_t revision=0;Settings settings{};bool stored=false;};
QueueHandle_t settingsQ=nullptr,controlQ=nullptr,telemetryQ=nullptr,viewQ=nullptr,queryQ=nullptr,replyQ=nullptr;
TaskHandle_t worker=nullptr;
// ISR->task communication uses FreeRTOS notifications, not a volatile
// cross-context flag. No SPI/RadioLib call may execute inside this ISR.
void IRAM_ATTR radioIRQ(){
  TaskHandle_t target=worker;
  if(!target)return;
  BaseType_t higherPriorityWoken=pdFALSE;
  vTaskNotifyGiveFromISR(target,&higherPriorityWoken);
  if(higherPriorityWoken==pdTRUE)portYIELD_FROM_ISR();
}
SPIClass radioSPI(FSPI);
Module radioModule(LORA_NSS,LORA_DIO1,LORA_RESET,LORA_BUSY,radioSPI,SPISettings(2000000,MSBFIRST,SPI_MODE0));
SX1262 radio=&radioModule;
LoRaView view;
LoRaTelemetry latest;
DutyGate duty;
Frame flight;bool inFlight=false,transmitting=false,awaiting=false,forceTelemetry=false,cadChecking=false;
uint8_t attempts=0;uint32_t txAt=0,ackAt=0,lastCycle=0,lastPublish=0,lastInit=0,sequence=0,lastDown=0;
uint64_t session=0;uint32_t backoffAt=0,backoffMs=0,firstTxAt=0,cadAt=0;uint8_t cadStreak=0;
LoRaArchiveReply archive;bool archiveActive=false,queryActive=false,liveSinceFragment=false;uint16_t archiveOffset=0;
uint32_t queryAt=0,queryRequest=0,telemetryRequest=0,latestAt=0;
uint8_t wire[MAX_PACKET]={};size_t wireSize=0;
uint64_t freshSession(){uint64_t n=(uint64_t(esp_random())<<32)|esp_random();return n?n:1;}
bool tag(const uint8_t *p,size_t n,uint8_t out[32]){
  const mbedtls_md_info_t *md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  return md&&mbedtls_md_hmac(md,view.settings.key,KEY_SIZE,p,n,out)==0;
}
bool receiveMode(){int16_t code=radio.startReceive();if(code!=RADIOLIB_ERR_NONE){view.code=code;view.available=false;view.phase=LORA_ERROR;return false;}return true;}
void clearFlight(){inFlight=transmitting=awaiting=cadChecking=false;wireSize=0;attempts=cadStreak=0;}
void rotateSession(){session=freshSession();sequence=lastDown=0;telemetryRequest=0;forceTelemetry=false;view.paired=false;archiveActive=queryActive=liveSinceFragment=false;clearFlight();}
bool makeFrame(uint8_t type,const uint8_t *payload,uint8_t length){
  if(sequence==UINT32_MAX){rotateSession();return false;}
  flight=Frame{};flight.type=type;flight.source=view.settings.nodeId;flight.destination=view.settings.gatewayId;flight.network=view.settings.networkId;flight.session=session;flight.sequence=++sequence;flight.length=length;
  if(length)memcpy(flight.payload,payload,length);
  size_t n=encodeFrame(flight,wire);uint8_t mac[32]={};if(!n||!tag(wire,n,mac)){view.code=-10001;return false;}
  memcpy(wire+n,mac,TAG_SIZE);wireSize=n+TAG_SIZE;inFlight=true;attempts=cadStreak=0;cadChecking=false;backoffAt=millis();
  uint32_t airMs=uint32_t((radio.getTimeOnAir(wireSize)+999)/1000);
  backoffMs=esp_random()%contentionWindowMs(airMs,type==HELLO)+17*(view.settings.nodeId%23);return true;
}
void initializeRadio(){
  lastInit=millis();view.available=false;view.lastRxMs=0;view.rssi=view.snr=0;view.phase=LORA_STARTING;view.code=0;rotateSession();
  if(!view.settings.enabled){view.phase=LORA_DISABLED;return;}
  if(!valid(view.settings)){view.code=-10002;view.phase=LORA_ERROR;return;}
  radioSPI.begin(LORA_SCK,LORA_MISO,LORA_MOSI,LORA_NSS);
  // Ra-01SH V1.1 uses a crystal, not a DIO3-powered TCXO. DIO2 drives its internal RF switch.
  ConfigLoRa_t config;config.frequency=view.settings.frequencyHz/1000000.0f;config.bandwidth=view.settings.bandwidthKHz;config.spreadingFactor=view.settings.sf;config.codingRate=view.settings.cr;config.syncWord=view.settings.syncWord;config.power=view.settings.powerDbm;config.preambleLength=view.settings.preamble;radio.tcxoVoltage=0;
  // Bound every BUSY wait and avoid ten nested 1-second reset probes in begin().
  radioModule.spiConfig.timeout=100;radio.resetOnStartup=false;
  radio.reset(false);vTaskDelay(pdMS_TO_TICKS(10));
  pinMode(LORA_BUSY,INPUT);
  if(digitalRead(LORA_BUSY)==HIGH){view.code=RADIOLIB_ERR_SPI_CMD_TIMEOUT;view.phase=LORA_ERROR;return;}
  int16_t code=radio.begin(config);
  if(code==RADIOLIB_ERR_NONE)code=radio.setCurrentLimit(140.0);
  if(code==RADIOLIB_ERR_NONE)code=radio.setDio2AsRfSwitch(true);
  if(code==RADIOLIB_ERR_NONE)code=radio.setCRC(2);
  if(code!=RADIOLIB_ERR_NONE){view.code=code;view.phase=LORA_ERROR;Serial.printf("[LoRa] SX1262 init failed code=%d\n",code);return;}
  // Discard notifications from the prior session before arming DIO1 again.
  // Level polling below handles an IRQ already asserted during re-arming.
  ulTaskNotifyTake(pdTRUE,0);
  radio.setDio1Action(radioIRQ);view.available=true;
  if(receiveMode()){lastCycle=millis()-view.settings.intervalMs;view.phase=LORA_LISTENING;Serial.printf("[LoRa] SX1262 ready node=%u gateway=%u frequency=%lu Hz\n",view.settings.nodeId,view.settings.gatewayId,(unsigned long)view.settings.frequencyHz);}
}
void hello(){uint8_t p[24]={};p[0]=3;p[1]=0x07;strncpy(reinterpret_cast<char*>(p+2),FIRMWARE_VERSION,21);makeFrame(HELLO,p,24);}
void fillTelemetry(uint8_t *p,uint32_t request){
  memset(p,0,32);put32(p,request);put32(p+4,latest.utc);
  uint64_t total=uint64_t(latest.ageMs)+uint32_t(millis()-latestAt);uint32_t age=total>UINT32_MAX?UINT32_MAX:uint32_t(total);
  put32(p+8,age);p[12]=age<=15000?latest.flags:uint8_t(latest.flags&~1);p[13]=latest.profile;put16(p+14,latest.meterId);put32(p+16,latest.day);put32(p+20,latest.revision);
  uint64_t bits=0;if(p[12]&1)memcpy(&bits,&latest.kwh,8);put64(p+24,bits);
}
void telemetry(){uint8_t p[32];fillTelemetry(p,telemetryRequest);telemetryRequest=0;makeFrame(TELEMETRY,p,sizeof(p));}
bool refreshUnsentTelemetry(){
  if(flight.type!=TELEMETRY||attempts)return true;
  fillTelemetry(flight.payload,get32(flight.payload));size_t n=encodeFrame(flight,wire);uint8_t mac[32]={};
  if(!n||!tag(wire,n,mac))return false;memcpy(wire+n,mac,TAG_SIZE);wireSize=n+TAG_SIZE;return true;
}
void nextArchiveChunk(){uint8_t p[MAX_PAYLOAD]={};constexpr uint16_t chunk=MAX_PAYLOAD-12;
  uint16_t count=uint16_t(CAPTURE_RECORD_SIZE-archiveOffset);if(count>chunk)count=chunk;
  put32(p,archive.request);put32(p+4,archive.day);put16(p+8,archiveOffset);put16(p+10,CAPTURE_RECORD_SIZE);memcpy(p+12,archive.bytes+archiveOffset,count);makeFrame(CHECKPOINT,p,uint8_t(12+count));}
void acceptedACK(){
  ++view.acks;if(flight.type==HELLO)view.paired=true;
  if(flight.type==CHECKPOINT){archiveOffset=uint16_t(archiveOffset+flight.length-12);liveSinceFragment=false;if(archiveOffset>=CAPTURE_RECORD_SIZE)archiveActive=false;}
  clearFlight();view.code=0;
}
void receivedPacket(){
  size_t n=radio.getPacketLength();if(n<HEADER_SIZE+TAG_SIZE||n>MAX_PACKET){++view.rejected;receiveMode();return;}
  uint8_t input[MAX_PACKET],mac[32]={};int16_t code=radio.readData(input,n);
  if(code!=RADIOLIB_ERR_NONE){if(code==RADIOLIB_ERR_CRC_MISMATCH)++view.crcErrors;else ++view.rejected;receiveMode();return;}
  view.rssi=radio.getRSSI();view.snr=radio.getSNR();
  bool signedOK=tag(input,n-TAG_SIZE,mac);uint8_t difference=0;for(unsigned i=0;i<TAG_SIZE;++i)difference|=mac[i]^input[n-TAG_SIZE+i];
  if(!signedOK||difference){++view.authErrors;++view.rejected;receiveMode();return;}
  Frame f;if(!decodeFrame(input,n-TAG_SIZE,f)){++view.rejected;receiveMode();return;}
  if(!addressed(f,view.settings,session)){++view.addressErrors;++view.rejected;receiveMode();return;}
  ++view.rx;view.lastRxSequence=f.sequence;view.lastRxMs=millis();
  Serial.printf("[LoRa] rx type=%u seq=%lu rssi=%.1f snr=%.1f\n",f.type,(unsigned long)f.sequence,view.rssi,view.snr);
  if(f.type==POLL&&lastDown&&int32_t(f.sequence-lastDown)<=0){++view.replayErrors;++view.rejected;receiveMode();return;}
  if(inFlight&&acceptsOutstandingACK(f,flight,transmitting,attempts)){acceptedACK();}
  else if(f.type==POLL&&view.paired&&f.length==5&&(!lastDown||int32_t(f.sequence-lastDown)>0)){
    uint32_t day=get32(f.payload+1);
    if(f.payload[0]==0&&!forceTelemetry&&!telemetryRequest){lastDown=f.sequence;telemetryRequest=f.sequence;forceTelemetry=true;}
    else if(f.payload[0]==1&&!queryActive&&!archiveActive){LoRaArchiveQuery request;request.day=day;request.request=f.sequence;request.session=session;
      if(xQueueSend(queryQ,&request,0)==pdTRUE){lastDown=f.sequence;queryActive=true;queryAt=millis();queryRequest=f.sequence;}else ++view.drops;
    }else ++view.rejected;
  }else ++view.rejected;
  receiveMode();
}
// Nonblocking SX1262 CAD and TX are owned only by the radio FreeRTOS task.
void sendAfterCad(uint32_t now){
  if(!inFlight)return; // No stale frame after a local re-pair or config edit.
  if(flight.type==TELEMETRY&&(flight.payload[12]&1)&&attempts&&
     uint64_t(get32(flight.payload+8))+uint32_t(now-firstTxAt)>15000){
    ++view.drops;view.paired=false;clearFlight();lastCycle=now;receiveMode();return;
  }
  if(!refreshUnsentTelemetry()){view.code=-10001;view.available=false;view.phase=LORA_ERROR;clearFlight();return;}
  view.lastAirMs=uint32_t((radio.getTimeOnAir(wireSize)+999)/1000);
  if(!view.lastAirMs)view.lastAirMs=1;
  duty.transmitted(now,view.lastAirMs);
  int16_t code=radio.startTransmit(wire,wireSize);
  if(code==RADIOLIB_ERR_NONE){
    ++view.tx;view.lastTxSequence=flight.sequence;
    Serial.printf("[LoRa] tx type=%u seq=%lu attempt=%u airtime=%lu ms\n",flight.type,
      (unsigned long)flight.sequence,unsigned(attempts+1),(unsigned long)view.lastAirMs);
    if(!attempts)firstTxAt=now;
    ++attempts;transmitting=true;txAt=now;cadStreak=0;
  }else{view.code=code;++view.drops;clearFlight();receiveMode();}
}
void postponeBusyChannel(uint32_t now){
  cadChecking=false;
  if(cadStreak<255)++cadStreak;
  backoffAt=now;
  backoffMs=cadBackoffMs(esp_random(),cadStreak,view.lastAirMs);
  ++view.cadDeferred;
  receiveMode(); // Keep listening for a late matching ACK or inbound POLL.
}
void run(void *){
  esp_task_wdt_add(nullptr);
  for(;;){
    esp_task_wdt_reset();uint32_t now=millis();Update update;
    if(xQueueReceive(settingsQ,&update,0)==pdTRUE){
      if(view.available){radio.standby();if(inFlight)++view.drops;}
      view.revision=update.revision;view.settings=update.settings;view.configured=true;view.stored=update.stored;initializeRadio();
    }
    now=millis();
    LoRaTelemetry incoming;if(xQueueReceive(telemetryQ,&incoming,0)==pdTRUE){latest=incoming;latestAt=now;}
    if(view.settings.enabled&&view.configured){
      if(!view.available){if(uint32_t(now-lastInit)>=30000)initializeRadio();}
      else {
        // Notifications cannot be lost between an ISR and a read/clear of a
        // volatile flag. A HIGH DIO1 also covers IRQs asserted before the
        // callback was armed or delivered while scheduling the task.
        const uint32_t notifications=ulTaskNotifyTake(pdTRUE,0);
        const bool pinHigh=digitalRead(LORA_DIO1)==HIGH;
        view.irqNotifications+=notifications;
        if(irqRecoveredFromLevel(notifications,pinHigh))++view.irqPinFallbacks;
        if(irqServiceDue(notifications,pinHigh)){uint32_t irq=radio.getIrqFlags();
          if(cadChecking&&(irq&(RADIOLIB_SX126X_IRQ_CAD_DONE|RADIOLIB_SX126X_IRQ_CAD_DETECTED))){
            cadChecking=false;int16_t scan=radio.getChannelScanResult();
            if(scan==RADIOLIB_CHANNEL_FREE){++view.cadClear;sendAfterCad(now);}
            else{
              if(scan==RADIOLIB_LORA_DETECTED)++view.cadBusy;
              else {++view.cadErrors;view.code=scan;}
              postponeBusyChannel(now);
            }
          }
          else if(transmitting&&(irq&RADIOLIB_SX126X_IRQ_TX_DONE)){int16_t code=radio.finishTransmit();transmitting=false;awaiting=code==RADIOLIB_ERR_NONE;ackAt=now;if(code!=RADIOLIB_ERR_NONE){view.code=code;++view.drops;clearFlight();}receiveMode();}
          else if(!transmitting&&!cadChecking&&(irq&RADIOLIB_SX126X_IRQ_RX_DONE))receivedPacket();
          else if(!transmitting){if(irq&RADIOLIB_SX126X_IRQ_CRC_ERR)++view.crcErrors;if(cadChecking){++view.cadErrors;postponeBusyChannel(now);}else receiveMode();}
        }
        if(cadChecking&&uint32_t(now-cadAt)>1500){radio.standby();++view.cadErrors;view.code=RADIOLIB_ERR_SPI_CMD_TIMEOUT;postponeBusyChannel(now);}
        if(transmitting&&uint32_t(now-txAt)>view.lastAirMs+2000){radio.standby();transmitting=false;awaiting=true;ackAt=now-view.settings.ackMs;view.code=RADIOLIB_ERR_TX_TIMEOUT;receiveMode();}
        if(awaiting&&uint32_t(now-ackAt)>=view.settings.ackMs){++view.timeouts;awaiting=false;backoffAt=now;backoffMs=250+esp_random()%(1000UL<<attempts);
          if(attempts>view.settings.retries){++view.drops;view.paired=false;archiveActive=false;clearFlight();lastCycle=now;}
        }
        uint8_t action=0;if(!transmitting&&!cadChecking&&xQueueReceive(controlQ,&action,0)==pdTRUE){
          if(action==1){if(inFlight)++view.drops;rotateSession();lastCycle=now-view.settings.intervalMs;}
          else if(action==2)forceTelemetry=true;
        }
        LoRaArchiveReply reply;if(xQueueReceive(replyQ,&reply,0)==pdTRUE&&queryActive&&reply.request==queryRequest&&reply.session==session){queryActive=false;
          if(reply.found){archive=reply;archiveOffset=0;archiveActive=true;liveSinceFragment=false;}else ++view.drops;
        }
        if(queryActive&&uint32_t(now-queryAt)>30000){queryActive=false;++view.drops;}
        if(!inFlight){
          uint8_t next=nextUplink(view.paired,archiveActive,forceTelemetry,uint32_t(now-lastCycle)>=view.settings.intervalMs,liveSinceFragment);
          if(next==CHECKPOINT)nextArchiveChunk();
          else if(next==HELLO||next==TELEMETRY){lastCycle=now;forceTelemetry=false;if(next==HELLO)hello();else{telemetry();if(archiveActive)liveSinceFragment=true;}}
        }
        // Do not resend an old live value as fresh after a long ACK/duty delay.
        if(inFlight&&flight.type==TELEMETRY&&(flight.payload[12]&1)&&attempts&&!transmitting&&!cadChecking&&uint64_t(get32(flight.payload+8))+uint32_t(now-firstTxAt)>15000){++view.drops;view.paired=false;clearFlight();lastCycle=now;}
        if(view.available&&inFlight&&!transmitting&&!awaiting&&!cadChecking&&!duty.remaining(now)&&uint32_t(now-backoffAt)>=backoffMs){
          // All node uplinks (HELLO, TELEMETRY, CHECKPOINT) sense the channel.
          // CAD is nonblocking and never runs concurrently with RX/TX.
          if(!refreshUnsentTelemetry()){view.code=-10001;view.available=false;view.phase=LORA_ERROR;clearFlight();continue;}
          int16_t code=radio.startChannelScan();
          if(code==RADIOLIB_ERR_NONE){cadChecking=true;cadAt=now;}
          else{++view.cadErrors;view.code=code;postponeBusyChannel(now);}
        }
        if(view.available){view.waitMs=duty.remaining(now);uint32_t jitter=uint32_t(now-backoffAt)<backoffMs?backoffMs-uint32_t(now-backoffAt):0;if(jitter>view.waitMs)view.waitMs=jitter;view.phase=cadChecking?LORA_CHANNEL_CHECK:transmitting?LORA_TRANSMITTING:awaiting?LORA_WAIT_ACK:inFlight&&view.waitMs?LORA_COOLDOWN:LORA_LISTENING;}
      }
    }
    if(uint32_t(now-lastPublish)>=250){lastPublish=now;xQueueOverwrite(viewQ,&view);}
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
} // namespace
bool initLoRaService(){
  settingsQ=xQueueCreate(2,sizeof(Update));controlQ=xQueueCreate(4,sizeof(uint8_t));telemetryQ=xQueueCreate(1,sizeof(LoRaTelemetry));viewQ=xQueueCreate(1,sizeof(LoRaView));queryQ=xQueueCreate(1,sizeof(LoRaArchiveQuery));replyQ=xQueueCreate(1,sizeof(LoRaArchiveReply));
  if(!settingsQ||!controlQ||!telemetryQ||!viewQ||!queryQ||!replyQ)return false;
  xQueueOverwrite(viewQ,&view);return xTaskCreatePinnedToCore(run,"sx1262-link",8192,nullptr,1,&worker,0)==pdPASS;
}
bool canQueueLoRaSettings(){return worker&&settingsQ&&uxQueueSpacesAvailable(settingsQ)>0;}
bool updateLoRaSettings(const lora::Settings &s,bool stored,uint32_t revision){Update u;u.revision=revision;u.settings=s;u.stored=stored;return settingsQ&&xQueueSend(settingsQ,&u,0)==pdTRUE;}
bool controlLoRa(uint8_t action){LoRaView current=readLoRaView();return current.available&&current.settings.enabled&&controlQ&&(action==1||action==2)&&xQueueSend(controlQ,&action,0)==pdTRUE;}
void publishLoRaTelemetry(const LoRaTelemetry &s){if(telemetryQ)xQueueOverwrite(telemetryQ,&s);}
bool takeLoRaArchiveQuery(LoRaArchiveQuery &q){return queryQ&&xQueueReceive(queryQ,&q,0)==pdTRUE;}
void replyLoRaArchive(const LoRaArchiveReply &r){if(replyQ)xQueueOverwrite(replyQ,&r);}
LoRaView readLoRaView(){LoRaView copy;if(viewQ)xQueuePeek(viewQ,&copy,0);return copy;}
TaskHandle_t loRaTaskHandle(){return worker;}
} // namespace fort
