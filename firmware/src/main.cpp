#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <driver/rmt.h>
#include "board.h"
#include "energy.h"
#include "debounce.h"
#include "web_ui.h"
#include "version.h"
#include "recovery.h"
#include "scan.h"
#include "led.h"
#include "trace.h"

using namespace fort;
static_assert(ARDUINO_RUNNING_CORE==0,"UI must run on core 0");
HardwareSerial bus(1);
Adafruit_SSD1306 oled(128,64,&Wire,-1);
WebServer server(80);
SemaphoreHandle_t i2cMutex,fsMutex;
QueueHandle_t snapshotQueue,commandQueue;
bool oledOK=false,apOK=false;
String apiToken;

enum CommandType:uint8_t { SET_TIME,SET_CONFIG,TOGGLE_MUTE,START_SCAN,CANCEL_SCAN,APPLY_SCAN,SET_LED,START_TRACE,STOP_TRACE };
struct Command { CommandType type; uint32_t epoch=0; Settings config{}; LedSettings led{}; uint8_t first=1,last=247; };
struct Snapshot {
  Day day{}; Settings config{}; ScanProgress scan{}; LedSettings led{}; TraceCapture trace{};
  bool ledStored=false,ledStorageOK=false;
  uint32_t utc=0,lastGoodMs=0,crcErrors=0,timeouts=0,rejected=0;
  double energy=0,power=0,ratio=0;
  bool rtc=false,storage=false,ratios=false,haveEnergy=false,online=false,muted=false;
  char error[64]="Waiting for a response";
  char errorCode[32]="waiting_for_meter";
};
LedSettings defaultLed() {
  LedSettings s{};s.brightness=RGB_LED_DEFAULT_BRIGHTNESS;s.periodMs=RGB_LED_DEFAULT_PERIOD_MS;return s;
}
// Core 0 owns the RMT driver and animation; core 1 owns persisted settings.
LedSettings renderedLed=defaultLed();
LedAnimation ledAnimation;
bool ledDriverOK=false;
constexpr rmt_channel_t LED_RMT_CHANNEL=RMT_CHANNEL_0;
void initLED() {
  if(!RGB_LED_ENABLED)return;
  rmt_config_t config={};config.rmt_mode=RMT_MODE_TX;config.channel=LED_RMT_CHANNEL;
  config.gpio_num=gpio_num_t(RGB_LED_PIN);config.clk_div=8;config.mem_block_num=1;
  config.tx_config.loop_en=false;config.tx_config.carrier_en=false;
  config.tx_config.idle_output_en=true;config.tx_config.idle_level=RMT_IDLE_LEVEL_LOW;
  ledDriverOK=rmt_config(&config)==ESP_OK&&rmt_driver_install(LED_RMT_CHANNEL,0,0)==ESP_OK;
  if(!ledDriverOK)Serial.println("Onboard LED: RMT initialization failed");
}
void tickLED(const LedSettings *settings=nullptr) {
  if(!ledDriverOK)return;
  if(settings)renderedLed=*settings;
  static uint32_t lastFrame=0;static bool written=false;
  static LedColor previous{};
  uint32_t now=millis();if(written&&uint32_t(now-lastFrame)<20)return;
  lastFrame=now;LedColor color=ledAnimation.sample(renderedLed,now);
  if(!written||color.red!=previous.red||color.green!=previous.green||color.blue!=previous.blue){
    // Keep symbols alive until TX completes. A busy channel skips this frame.
    if(rmt_wait_tx_done(LED_RMT_CHANNEL,0)!=ESP_OK)return;
    static rmt_item32_t items[24];uint8_t grb[]={color.green,color.red,color.blue};
    for(uint8_t i=0;i<24;++i){bool one=grb[i/8]&(1<<(7-i%8));
      items[i].level0=1;items[i].duration0=one?8:4;
      items[i].level1=0;items[i].duration1=one?4:8;
    }
    if(rmt_write_items(LED_RMT_CHANNEL,items,24,false)==ESP_OK){previous=color;written=true;}
  }
}
Snapshot state{}; // UI reads queue copies. Only acquisition task mutates state.
Tracker tracker;
ScanController scanner;
Preferences prefs;
bool fsOK=false,nvsOK=false,archiveOK=true,checkpointOK=true,configOK=true;
uint32_t ct=0,ptNum=0,ptDen=0,lastSave=0,lastRatio=0,lastPoll=0,lastGood=0;
bool haveSample=false,muted=false;
uint32_t previousRaw=0,previousMono=0;
bool rawKnown=false,clockKnown=false;
uint32_t lastClockMono=0,lastClockUTC=0;

struct Checkpoint { uint32_t magic=0x46524432; Day day{}; uint16_t crc=0; };
struct ConfigRecord { Settings settings{};uint16_t crc=0; };
void setError(const char *s,const char *code) { strlcpy(state.error,s,sizeof(state.error));strlcpy(state.errorCode,code,sizeof(state.errorCode)); }
bool readSnapshot(Snapshot &s) { return xQueuePeek(snapshotQueue,&s,0)==pdTRUE; }
uint32_t activeBaud=0;char activeParity=0;
void startBusFor(const Settings &config) {
  if(activeBaud==config.baud&&activeParity==config.parity)return;
  bus.end(); bus.setRxBufferSize(128);
  uint32_t format=config.parity=='E'?SERIAL_8E1:config.parity=='O'?SERIAL_8O1:SERIAL_8N1;
  bus.begin(config.baud,format,PIN_RX,PIN_TX);activeBaud=config.baud;activeParity=config.parity;
  digitalWrite(PIN_RS485_DE,LOW);
}
void startBus() { startBusFor(state.config); }

// Only core 1 owns the UART. Strict length, function, ID, CRC, exception checks.
bool readWordsFor(const Settings &config,uint16_t address,uint8_t count,uint8_t *out,uint8_t attempts=3,bool normal=true) {
  if(count<1||count>3) return false;
  uint8_t tx[8]={config.slave,3,uint8_t(address>>8),uint8_t(address),0,count,0,0};
  uint16_t c=crc16(tx,6); tx[6]=c;tx[7]=c>>8;
  uint32_t bits=config.parity=='N'?10:11;
  uint32_t gap=config.baud>19200?1750:(3500000UL*bits+config.baud-1)/config.baud;
  for(int attempt=0;attempt<attempts;++attempt) {
    esp_task_wdt_reset();
    for(int i=0;i<128&&bus.available();++i)bus.read();
    vTaskDelay(pdMS_TO_TICKS((gap+999)/1000+1));
    digitalWrite(PIN_RS485_DE,HIGH); delayMicroseconds(30);
    bus.write(tx,8); bus.flush(); digitalWrite(PIN_RS485_DE,LOW);
    uint8_t rx[16]; size_t n=0; uint32_t start=millis(),lastByte=micros(); bool overflow=false;
    while(millis()-start<800) {
      while(bus.available()) {
        uint32_t now=micros();
        int v=bus.read(); if(n<sizeof(rx))rx[n++]=v;else {overflow=true;break;}
        lastByte=now;
      }
      if(overflow || (n && uint32_t(micros()-lastByte)>=gap)) break;
      vTaskDelay(1);
    }
    state.trace.append(tx,rx,n,overflow,start,config.baud,config.parity,config.slave,address,count,uint8_t(attempt+1),!normal);
    if(!n) {
      if(normal){++state.timeouts;setError("Modbus response timeout","modbus_timeout");}else ++scanner.progress.timeouts;
    }else if(!overflow&&n>=5&&crc16(rx,n-2)==(uint16_t(rx[n-2])|(uint16_t(rx[n-1])<<8))) {
      if(validFrame(rx,n,config.slave,count)){memcpy(out,rx+3,count*2);return true;}
      if(normal&&rx[0]==config.slave&&rx[1]==0x83&&n==5){snprintf(state.error,sizeof(state.error),"Modbus exception 0x%02X",rx[2]);strlcpy(state.errorCode,"modbus_exception",sizeof(state.errorCode));return false;}
      if(normal)setError("Wrong ID, function or response length","invalid_frame");else ++scanner.progress.rejected;
    }else if(overflow) {
      if(normal)setError("RTU response exceeds the buffer","response_overflow");else ++scanner.progress.rejected;
    }else {
      if(normal){++state.crcErrors;setError("Response CRC mismatch","crc_mismatch");}else ++scanner.progress.crcErrors;
    }
    if(attempt+1<attempts)vTaskDelay(pdMS_TO_TICKS(30*(attempt+1)));
  }
  return false;
}
bool readWords(uint16_t address,uint8_t count,uint8_t *out) {
  return readWordsFor(state.config,address,count,out);
}
void restoreAfterScan() {
  startBus();state.ratios=false;haveSample=false;rawKnown=false;state.power=NAN;
  lastRatio=0;lastPoll=millis();
  setError(scanner.progress.phase==SCAN_FOUND?"Scan finished; result not applied":scanner.progress.phase==SCAN_CANCELLED?"Scan cancelled; previous settings restored":"Scan finished without a match",
    scanner.progress.phase==SCAN_FOUND?"scan_result_pending":scanner.progress.phase==SCAN_CANCELLED?"scan_was_cancelled":"scan_no_meter");
}
void scanStep() {
  const Settings candidate=scanner.progress.candidate;
  startBusFor(candidate);uint8_t energy[4],ratios[6];
  // Two different known read-only registers must validate, including CRC and ratio range.
  bool found=readWordsFor(candidate,0x0082,2,energy,1,false)&&readWordsFor(candidate,0x002c,3,ratios,1,false);
  if(found&&!validScanRatios(ratios)){found=false;++scanner.progress.rejected;}
  scanner.completeProbe(found,millis());
  if(!scanner.running())restoreAfterScan();
}

// Check every I2C transaction, OSF, BCD range and calendar round trip.
bool rtcRead(uint32_t &utc) {
  if(xSemaphoreTake(i2cMutex,pdMS_TO_TICKS(80))!=pdTRUE)return false;
  uint8_t b[7]={},status=0;bool ok=false;
  Wire.beginTransmission(0x68);Wire.write(0x0f);
  if(Wire.endTransmission(false)==0&&Wire.requestFrom(uint8_t(0x68),uint8_t(1))==1) {
    status=Wire.read();Wire.beginTransmission(0x68);Wire.write(0);
    if(Wire.endTransmission(false)==0&&Wire.requestFrom(uint8_t(0x68),uint8_t(7))==7) {
      for(auto &v:b)v=Wire.read();ok=true;
    }
  }
  xSemaphoreGive(i2cMutex);
  if(!ok||(status&0x80)||(b[2]&0x40)||(b[5]&0x80))return false;
  auto dec=[](uint8_t v)->int{return (v>>4)*10+(v&15);};
  for(int i: {0,1,2,4,5,6}) if((b[i]&15)>9||(b[i]>>4)>9)return false;
  int sec=dec(b[0]),min=dec(b[1]),hour=dec(b[2]),d=dec(b[4]),m=dec(b[5]),y=2000+dec(b[6]);
  if(sec>59||min>59||hour>23||d<1||d>31||m<1||m>12||y<2024||y>2099)return false;
  DateTime dt(y,m,d,hour,min,sec);if(!dt.isValid())return false;
  utc=dt.unixtime();return true;
}
bool rtcWrite(uint32_t utc) {
  if(utc<1704067200UL||utc>4102444799UL)return false;
  DateTime d(utc); auto bcd=[](uint8_t v){return uint8_t((v/10)*16+v%10);};
  if(xSemaphoreTake(i2cMutex,pdMS_TO_TICKS(100))!=pdTRUE)return false;
  Wire.beginTransmission(0x68);Wire.write(0);
  uint8_t data[]={bcd(d.second()),bcd(d.minute()),bcd(d.hour()),bcd(d.dayOfTheWeek()+1),bcd(d.day()),bcd(d.month()),bcd(d.year()-2000)};
  Wire.write(data,sizeof(data));bool ok=Wire.endTransmission()==0;
  Wire.beginTransmission(0x68);Wire.write(0x0f);
  if(Wire.endTransmission(false)!=0||Wire.requestFrom(uint8_t(0x68),uint8_t(1))!=1)ok=false;
  else { uint8_t v=Wire.read()&~0x80;Wire.beginTransmission(0x68);Wire.write(0x0f);Wire.write(v);ok=Wire.endTransmission()==0&&ok; }
  xSemaphoreGive(i2cMutex);uint32_t check=0;
  return ok&&rtcRead(check)&&check>=utc&&check-utc<=2;
}

String dateForDay(uint32_t n) { DateTime d(n*86400);char s[16];snprintf(s,sizeof(s),"%04u-%02u-%02u",d.year(),d.month(),d.day());return String(s); }
String dayPath(uint32_t n) {return "/d"+String(n)+".csv";}
String csvDay(const Day &d) {
  String s="date,shift,start_wib,end_wib,start_kwh,end_kwh,consumption_kwh,quality\r\n";
  for(int i=0;i<SLOT_COUNT;++i) {
    const Slot &x=d.slots[i];int a=START_MIN[i],b=a+30;char t[40];
    snprintf(t,sizeof(t),",%s,%02d:%02d,%02d:%02d,",i<6?"morning":"evening",a/60,a%60,b/60,b%60);
    s+=dateForDay(d.number)+t;
    if(x.hasStart)s+=String(x.start,6);s+=',';
    if(x.hasEnd)s+=String(x.end,6);s+=',';
    if(x.quality==COMPLETE&&x.end>=x.start)s+=String(x.end-x.start,6);
    s+=',';s+=qualityCode(x.quality);s+="\r\n";
  }return s;
}
void saveArchive() {
  if(!fsOK||!tracker.day.number){archiveOK=false;return;}
  if(xSemaphoreTake(fsMutex,pdMS_TO_TICKS(200))!=pdTRUE){archiveOK=false;return;}
  String path=dayPath(tracker.day.number),temp="/pending.csv",backup=path+".bak",csv=csvDay(tracker.day);
  File f=LittleFS.open(temp,"w");bool ok=bool(f);
  if(ok){ok=f.print(csv)==csv.length();f.flush();f.close();}
  // Keep old copy if power is lost before the replacement is installed.
  if(ok)ok=installArchive(LittleFS,temp,path,backup);
  // Retention is deliberate and bounded; current day's write must succeed first.
  if(ok){File root=LittleFS.open("/");File item=root.openNextFile();while(item){String name=item.name();item.close();if(name.startsWith("/"))name.remove(0,1);if(name.startsWith("d")){uint32_t day=name.substring(1).toInt();if(day>0&&day+31<=tracker.day.number)LittleFS.remove("/"+name);}item=root.openNextFile();}}
  xSemaphoreGive(fsMutex);archiveOK=ok;
}
void saveCheckpoint(bool archive) {
  if(tracker.day.number&&nvsOK) {
    Checkpoint c{};c.day=tracker.day;c.crc=crc16(reinterpret_cast<const uint8_t*>(&c),offsetof(Checkpoint,crc));
    checkpointOK=prefs.putBytes("day",&c,sizeof(c))==sizeof(c);
  }else checkpointOK=false;
  if(archive)saveArchive();lastSave=millis();
}
void publish() {
  scanner.refresh(millis());state.scan=scanner.progress;
  state.day=tracker.day;state.muted=muted;state.storage=fsOK&&nvsOK&&archiveOK&&checkpointOK&&configOK;
  state.lastGoodMs=lastGood;state.haveEnergy=haveSample;
  state.online=haveSample&&millis()-lastGood<=15000;
  xQueueOverwrite(snapshotQueue,&state);
}

void acquisition(void *) {
  esp_task_wdt_add(nullptr);
  nvsOK=prefs.begin("fort",false);Settings config{};
  if(nvsOK&&prefs.isKey("config")) {
    ConfigRecord record{};
    bool read=prefs.getBytesLength("config")==sizeof(record)&&prefs.getBytes("config",&record,sizeof(record))==sizeof(record);
    uint16_t c=crc16(reinterpret_cast<uint8_t*>(&record),offsetof(ConfigRecord,crc));
    if(read&&validSettings(record.settings)&&record.crc==c)config=record.settings;
    else {config=Settings{};configOK=false;setError("Stored configuration invalid; defaults loaded","config_corrupt");}
  }
  state.config=config;state.led=defaultLed();state.ledStorageOK=nvsOK;
  if(nvsOK&&prefs.isKey("led")) {
    uint8_t bytes[LED_RECORD_SIZE]={};
    state.ledStored=prefs.getBytesLength("led")==sizeof(bytes)&&prefs.getBytes("led",bytes,sizeof(bytes))==sizeof(bytes)&&decodeLed(bytes,sizeof(bytes),state.led);
    state.ledStorageOK=state.ledStored;
  }
  if(nvsOK&&prefs.getBytesLength("day")==sizeof(Checkpoint)) {
    Checkpoint c{};prefs.getBytes("day",&c,sizeof(c));
    if(c.magic==0x46524432&&c.crc==crc16(reinterpret_cast<uint8_t*>(&c),offsetof(Checkpoint,crc))) {tracker.day=c.day;tracker.interrupt();}
    else checkpointOK=false;
  }
  startBus();uint32_t lastRatioTry=0;bool previousRTC=false;
  while(true) {
    esp_task_wdt_reset();Command cmd;
    while(xQueueReceive(commandQueue,&cmd,0)==pdTRUE) {
      if(cmd.type==TOGGLE_MUTE)muted=!muted;
      if(cmd.type==START_TRACE&&!state.trace.active){state.trace.begin();publish();}
      if(cmd.type==STOP_TRACE){state.trace.stop();publish();}
      if(cmd.type==SET_LED&&RGB_LED_ENABLED&&validLed(cmd.led)) {
        state.led=cmd.led;uint8_t bytes[LED_RECORD_SIZE];encodeLed(state.led,bytes);
        state.ledStored=nvsOK&&prefs.putBytes("led",bytes,sizeof(bytes))==sizeof(bytes);
        state.ledStorageOK=state.ledStored;publish();
      }
      if(cmd.type==SET_TIME) {
        tracker.interrupt();clockKnown=false;rawKnown=false;
        if(!rtcWrite(cmd.epoch))setError("RTC write or verification failed","rtc_write_failed");
        saveCheckpoint(true);
      }
      if(cmd.type==START_SCAN&&!scanner.running()&&scanner.begin(cmd.first,cmd.last,state.config,millis())) {
        tracker.interrupt();saveCheckpoint(true);rawKnown=false;haveSample=false;state.ratios=false;state.power=NAN;
        setError("Scan running; recording paused","scanning");publish();
      }
      if(cmd.type==CANCEL_SCAN&&scanner.cancel(millis())){restoreAfterScan();publish();}
      if(cmd.type==APPLY_SCAN&&scanner.progress.phase==SCAN_FOUND) {cmd.type=SET_CONFIG;cmd.config=scanner.progress.result;}
      if(cmd.type==SET_CONFIG&&!scanner.running()&&validSettings(cmd.config)) {
        tracker.interrupt();state.config=cmd.config;state.ratios=false;rawKnown=false;haveSample=false;
        ConfigRecord record{};record.settings=state.config;record.crc=crc16(reinterpret_cast<uint8_t*>(&record),offsetof(ConfigRecord,crc));
        configOK=nvsOK&&prefs.putBytes("config",&record,sizeof(record))==sizeof(record);
        saveCheckpoint(true);startBus();lastRatio=0;lastPoll=0;publish();
      }
    }
    uint32_t utc=0;bool rtcOK=rtcRead(utc);
    if(!rtcOK&&previousRTC)tracker.interrupt();previousRTC=rtcOK;state.rtc=rtcOK;
    if(rtcOK) {
      if(clockKnown) {
        int64_t delta=int64_t(utc)-lastClockUTC;
        uint32_t elapsed=(millis()-lastClockMono)/1000;
        if(delta<0||llabs(delta-int64_t(elapsed))>2){tracker.interrupt();rawKnown=false;}
      }
      lastClockMono=millis();lastClockUTC=utc;clockKnown=true;state.utc=utc;
      uint32_t local=utc+LOCAL_OFFSET,n=local/86400;
      if(tracker.day.number!=n) {
        if(tracker.day.number){tracker.tick(tracker.day.number*86400+86399);saveCheckpoint(true);}
        tracker.newDay(n);saveCheckpoint(true);
      }
      uint8_t before[SLOT_COUNT];for(int i=0;i<SLOT_COUNT;++i)before[i]=tracker.day.slots[i].quality;
      tracker.tick(local);
      for(int i=0;i<SLOT_COUNT;++i)if(before[i]!=tracker.day.slots[i].quality){saveCheckpoint(true);break;}
    }else state.utc=0;
    if(scanner.running()) {
      scanStep();publish();vTaskDelay(pdMS_TO_TICKS(10));continue;
    }
    if(ratioPollDue(state.ratios,millis(),lastRatio,lastRatioTry)) {
        lastRatioTry=millis();uint8_t p[6];
        if(readWords(0x002c,3,p)) {
          uint32_t a=(p[0]<<8)|p[1],b=(p[2]<<8)|p[3],c=(p[4]<<8)|p[5];
          if(a&&b&&c&&a<=9999&&b<=9999&&c<=9999) {
            double ratio=double(a)/b*c;
            if(ct&&(a!=ptNum||b!=ptDen||c!=ct)){tracker.interrupt();rawKnown=false;haveSample=false;}
            ptNum=a;ptDen=b;ct=c;state.ratio=ratio;state.ratios=true;lastRatio=millis();
          }else {state.ratios=false;tracker.interrupt();setError("CT/PT ratio outside the manual range","ratio_out_of_range");}
        }else if(state.ratios&&millis()-lastRatio>120000){state.ratios=false;tracker.interrupt();}
    }
    if(millis()-lastPoll>=2000) {
      lastPoll=millis();uint8_t e[4],p[4];
      // Time stamp brackets the energy read. Discard slow/inconsistent windows.
      uint32_t beginUTC=0,endUTC=0,beginMono=millis();bool beginOK=rtcRead(beginUTC);
      bool energyOK=state.ratios&&readWords(0x0082,2,e);
      bool endOK=rtcRead(endUTC);
      if(energyOK) {
        uint32_t raw=be32(e);bool accepted=true;
        if(rawKnown&&raw<previousRaw) {
          // A wrap cannot be distinguished reliably from a reset: invalidate continuity.
          tracker.interrupt();++state.rejected;setError("Counter decreased; continuity interrupted","counter_reset");
        }
        double energy=raw*0.001*state.ratio;
        // Physical sanity limit: editable compile-time plant limit (100 MW).
        if(rawKnown&&raw>=previousRaw&&millis()!=previousMono) {
          double maxDelta=100000.0*(millis()-previousMono)/3600000.0+0.002*state.ratio;
          if((raw-previousRaw)*0.001*state.ratio>maxDelta){accepted=false;++state.rejected;setError("Energy increase exceeds configured limit","energy_jump");}
        }
        if(accepted) {
          state.energy=energy;haveSample=true;lastGood=millis();previousRaw=raw;previousMono=millis();rawKnown=true;
          uint32_t local=endUTC+LOCAL_OFFSET;
          if(beginOK&&endOK&&endUTC>=beginUTC&&endUTC-beginUTC<=2&&millis()-beginMono<=2500&&local/86400==tracker.day.number) {
            uint8_t before[SLOT_COUNT];for(int i=0;i<SLOT_COUNT;++i)before[i]=tracker.day.slots[i].quality;
            if(tracker.sample(local,energy)) {
              for(int i=0;i<SLOT_COUNT;++i)if(before[i]!=tracker.day.slots[i].quality){saveCheckpoint(true);break;}
            }
          }else tracker.interrupt();
          if(readWords(0x006a,2,p)) {
            state.power=int32_t(be32(p))*0.0001*state.ratio;
            setError("Reading valid","acquisition_ok");
          }else state.power=NAN;
        }
      }
    }
    if(haveSample&&millis()-lastGood>15000)tracker.interrupt();
    if(millis()-lastSave>=300000)saveCheckpoint(true);
    publish();vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void jsonReply(int code,const String &msg,const char *messageCode) {
  JsonDocument d;d["message"]=msg;d["code"]=messageCode;String out;serializeJson(d,out);server.send(code,"application/json",out);
}
bool authorizePost() {
  if(server.arg("token")!=apiToken){jsonReply(403,"Session expired. Reload this page.","invalid_token");return false;}
  if(server.args()>8||server.arg("plain").length()>512){jsonReply(413,"Request is too large","request_too_large");return false;}
  return true;
}
bool parseNumber(const String &s,uint32_t &value) {
  if(!s.length()||s.length()>10)return false;uint64_t v=0;
  for(size_t i=0;i<s.length();++i){if(s[i]<'0'||s[i]>'9')return false;v=v*10+s[i]-'0';if(v>UINT32_MAX)return false;}
  value=v;return true;
}
void enqueueCommand(const Command &c) {
  if(xQueueSend(commandQueue,&c,0)==pdTRUE)jsonReply(202,"Command queued. Check the device status in a moment.","accepted");
  else jsonReply(503,"Command queue is busy. Try again.","queue_busy");
}
// Flash-mapped gzip bytes go straight to lwIP in 1 KiB pieces. A stalled client
// cannot hold a single WiFiClient::write() retry loop across the watchdog period.
void serveAsset(const uint8_t *bytes,size_t length,const char *mime) {
  server.sendHeader("Cache-Control","no-store");server.sendHeader("Content-Encoding","gzip");
  server.sendHeader("X-Content-Type-Options","nosniff");server.setContentLength(length);
  server.send(200,mime,"");
  WiFiClient client=server.client();size_t offset=0;uint32_t start=millis();
  while(client.connected()&&offset<length&&millis()-start<5000) {
    esp_task_wdt_reset();tickLED();size_t amount=min(size_t(1024),length-offset);
    int sent=::send(client.fd(),bytes+offset,amount,MSG_DONTWAIT);
    if(sent>0)offset+=sent;
    else if(sent<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)break;
    vTaskDelay(1);
  }
  if(offset<length)client.stop();
}
String hexFrame(const uint8_t *bytes,size_t length) {
  static const char digits[]="0123456789ABCDEF";char out[TRACE_RX_CAPACITY*3]={};
  size_t offset=0;
  for(size_t i=0;i<length&&i<TRACE_RX_CAPACITY;++i){
    if(i)out[offset++]=' ';out[offset++]=digits[bytes[i]>>4];out[offset++]=digits[bytes[i]&15];
  }
  return String(out);
}
void webSetup() {
  server.on("/",HTTP_GET,[]{serveAsset(WEB_INDEX_GZIP,WEB_INDEX_GZIP_LEN,"text/html; charset=utf-8");});
  server.on("/assets/app.js",HTTP_GET,[]{serveAsset(WEB_APP_GZIP,WEB_APP_GZIP_LEN,"application/javascript; charset=utf-8");});
  server.on("/assets/app.css",HTTP_GET,[]{serveAsset(WEB_STYLE_GZIP,WEB_STYLE_GZIP_LEN,"text/css; charset=utf-8");});
  server.on("/assets/third-party.txt",HTTP_GET,[]{serveAsset(WEB_NOTICES_GZIP,WEB_NOTICES_GZIP_LEN,"text/plain; charset=utf-8");});
  server.on("/api/status",HTTP_GET,[]{
    Snapshot s{};readSnapshot(s);JsonDocument d;
    JsonObject led=d["led"].to<JsonObject>();
    led["available"]=RGB_LED_ENABLED&&ledDriverOK;led["pin"]=RGB_LED_PIN;led["mode"]=ledModeName(s.led.mode);
    led["brightness"]=s.led.brightness;led["period"]=s.led.periodMs;
    led["red"]=s.led.red;led["green"]=s.led.green;led["blue"]=s.led.blue;
    led["stored"]=s.ledStored;led["storageOK"]=s.ledStorageOK;
    d["version"]=FIRMWARE_VERSION;d["token"]=apiToken;d["rtc"]=s.rtc;d["storage"]=s.storage;d["ratios"]=s.ratios;d["muted"]=s.muted;
    bool online=s.online&&millis()-s.lastGoodMs<=15000;
    d["online"]=online;d["energy"]=nullptr;d["power"]=nullptr;d["age"]=nullptr;
    if(s.haveEnergy){d["energy"]=s.energy;d["age"]=(millis()-s.lastGoodMs)/1000;}
    if(online&&isfinite(s.power))d["power"]=s.power;
    d["ratio"]=s.ratio;d["slave"]=s.config.slave;d["baud"]=s.config.baud;d["parity"]=String(s.config.parity);
    JsonObject scan=d["scan"].to<JsonObject>();
    scan["phase"]=scanName(s.scan.phase);scan["tested"]=s.scan.tested;scan["total"]=s.scan.total;
    scan["first"]=s.scan.first;scan["last"]=s.scan.last;scan["elapsed"]=s.scan.elapsedMs/1000;
    scan["slave"]=s.scan.candidate.slave;scan["baud"]=s.scan.candidate.baud;scan["parity"]=String(s.scan.candidate.parity);
    scan["timeouts"]=s.scan.timeouts;scan["crcErrors"]=s.scan.crcErrors;scan["rejected"]=s.scan.rejected;
    if(s.scan.phase==SCAN_FOUND){JsonObject result=scan["result"].to<JsonObject>();result["slave"]=s.scan.result.slave;result["baud"]=s.scan.result.baud;result["parity"]=String(s.scan.result.parity);}
    d["crcErrors"]=s.crcErrors;d["timeouts"]=s.timeouts;d["rejected"]=s.rejected;d["heap"]=ESP.getFreeHeap();d["uptime"]=millis()/1000;d["error"]=s.error;d["error_code"]=s.errorCode;
    if(s.rtc){DateTime t(s.utc+LOCAL_OFFSET);char h[12];snprintf(h,sizeof(h),"%02u:%02u:%02u",t.hour(),t.minute(),t.second());d["clock"]=h;d["date"]=dateForDay((s.utc+LOCAL_OFFSET)/86400);}
    d["dayDate"]=s.day.number?dateForDay(s.day.number):"";
    JsonArray rows=d["slots"].to<JsonArray>();
    for(int i=0;i<SLOT_COUNT;++i){const Slot &x=s.day.slots[i];JsonObject r=rows.add<JsonObject>();r["start"]=START_MIN[i];r["quality"]=qualityName(x.quality);r["quality_code"]=qualityCode(x.quality);r["kwh"]=nullptr;if(x.quality==COMPLETE&&x.end>=x.start)r["kwh"]=x.end-x.start;else if(x.quality==ACTIVE&&x.hasStart&&x.last>=x.start)r["kwh"]=x.last-x.start;}
    String out;serializeJson(d,out);server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
  });
  server.on("/api/trace",HTTP_GET,[]{
    Snapshot s{};readSnapshot(s);JsonDocument d;
    d["version"]=FIRMWARE_VERSION;d["generation"]=s.trace.generation;d["active"]=s.trace.active;
    d["count"]=s.trace.count;d["capacity"]=TRACE_CAPACITY;d["txPin"]=PIN_TX;d["rxPin"]=PIN_RX;
    JsonArray frames=d["frames"].to<JsonArray>();
    for(uint8_t i=0;i<s.trace.count&&i<TRACE_CAPACITY;++i){
      const TraceFrame &frame=s.trace.frames[i];JsonObject row=frames.add<JsonObject>();
      row["atMs"]=frame.atMs;row["baud"]=frame.baud;row["parity"]=String(frame.parity);
      row["slave"]=frame.slave;row["address"]=frame.address;row["words"]=frame.words;row["attempt"]=frame.attempt;
      row["scan"]=frame.scan;row["overflow"]=frame.overflow;row["result"]=traceResultName(frame.result);
      row["tx"]=hexFrame(frame.tx,sizeof(frame.tx));row["rx"]=hexFrame(frame.rx,frame.size);
      row["rxBytes"]=frame.size;
    }
    String out;serializeJson(d,out);server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
  });
  server.on("/api/trace/start",HTTP_POST,[]{
    if(!authorizePost())return;Snapshot s{};readSnapshot(s);
    if(s.trace.active){jsonReply(409,"A raw capture is already running","trace_busy");return;}
    Command c{};c.type=START_TRACE;enqueueCommand(c);
  });
  server.on("/api/trace/stop",HTTP_POST,[]{if(!authorizePost())return;Command c{};c.type=STOP_TRACE;enqueueCommand(c);});
  server.on("/api/led",HTTP_POST,[]{
    if(!authorizePost())return;
    if(!RGB_LED_ENABLED||!ledDriverOK){jsonReply(409,"Onboard LED is disabled or its RMT driver is unavailable","led_unavailable");return;}
    uint32_t brightness=0,period=0,red=0,green=0,blue=0;LedMode mode=LED_OFF;
    if(!parseLedMode(server.arg("mode").c_str(),mode)||!parseNumber(server.arg("brightness"),brightness)||brightness>255||
       !parseNumber(server.arg("period"),period)||period<500||period>30000||
       !parseNumber(server.arg("red"),red)||red>255||!parseNumber(server.arg("green"),green)||green>255||!parseNumber(server.arg("blue"),blue)||blue>255){
      jsonReply(400,"Invalid LED mode, color, brightness or cycle duration","invalid_led");return;
    }
    Command c{};c.type=SET_LED;c.led.mode=mode;c.led.brightness=brightness;c.led.periodMs=period;
    c.led.red=red;c.led.green=green;c.led.blue=blue;enqueueCommand(c);
  });
  server.on("/api/config",HTTP_POST,[]{
    if(!authorizePost())return;Snapshot s{};readSnapshot(s);
    if(s.scan.phase==SCAN_RUNNING){jsonReply(409,"Cancel the scan before changing the connection","scan_busy");return;}
    uint32_t slave=0,baud=0;String parity=server.arg("parity");
    if(!parseNumber(server.arg("slave"),slave)||!parseNumber(server.arg("baud"),baud)||slave<1||slave>247||parity.length()!=1){jsonReply(400,"Invalid serial settings","invalid_config");return;}
    Command c{};c.type=SET_CONFIG;c.config.slave=slave;c.config.baud=baud;c.config.parity=parity[0];
    if(!validSettings(c.config)){jsonReply(400,"Unsupported baud rate or parity","unsupported_serial");return;}enqueueCommand(c);
  });
  server.on("/api/scan/start",HTTP_POST,[]{
    if(!authorizePost())return;Snapshot s{};readSnapshot(s);
    if(s.scan.phase==SCAN_RUNNING){jsonReply(409,"A scan is already running","scan_running_error");return;}
    uint32_t first=0,last=0;
    if(!parseNumber(server.arg("first"),first)||!parseNumber(server.arg("last"),last)||first<1||last>247||first>last){jsonReply(400,"Use IDs 1-247, with first ID <= last ID","invalid_scan_range");return;}
    Command c{};c.type=START_SCAN;c.first=first;c.last=last;enqueueCommand(c);
  });
  server.on("/api/scan/cancel",HTTP_POST,[]{if(!authorizePost())return;Command c{};c.type=CANCEL_SCAN;enqueueCommand(c);});
  server.on("/api/scan/apply",HTTP_POST,[]{
    if(!authorizePost())return;Snapshot s{};readSnapshot(s);
    if(s.scan.phase!=SCAN_FOUND){jsonReply(409,"No scan result is available","no_scan_result");return;}
    Command c{};c.type=APPLY_SCAN;enqueueCommand(c);
  });
  server.on("/api/time",HTTP_POST,[]{
    if(!authorizePost())return;uint32_t epoch=0;
    if(!parseNumber(server.arg("epoch"),epoch)||epoch<1704067200UL||epoch>4102444799UL-LOCAL_OFFSET){jsonReply(400,"Time must be within 2024-2099","invalid_time");return;}
    Snapshot s{};readSnapshot(s);
    if(s.rtc&&s.day.number&&(epoch+LOCAL_OFFSET)/86400<s.day.number){jsonReply(409,"Earlier date rejected to protect existing records","backward_date");return;}
    Command c{};c.type=SET_TIME;c.epoch=epoch;enqueueCommand(c);
  });
  server.on("/api/mute",HTTP_POST,[]{if(!authorizePost())return;Command c{};c.type=TOGGLE_MUTE;enqueueCommand(c);});
  server.on("/api/history",HTTP_GET,[]{
    Snapshot s{};readSnapshot(s);JsonDocument d;JsonArray a=d.to<JsonArray>();
    if(fsOK&&xSemaphoreTake(fsMutex,pdMS_TO_TICKS(100))==pdTRUE){
      for(uint32_t n=s.day.number,k=0;n&&k<31;--n,++k)if(LittleFS.exists(dayPath(n))||LittleFS.exists(dayPath(n)+".bak")){JsonObject o=a.add<JsonObject>();o["day"]=n;o["date"]=dateForDay(n);}
      xSemaphoreGive(fsMutex);
    }
    String out;serializeJson(d,out);server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
  });
  server.on("/api/export",HTTP_GET,[]{
    Snapshot s{};readSnapshot(s);uint32_t day=s.day.number;
    if(server.hasArg("day")&&!parseNumber(server.arg("day"),day)){jsonReply(400,"Invalid archive date","invalid_archive_date");return;}
    if(!day||day>s.day.number||s.day.number-day>=31){jsonReply(404,"Archive is not available","archive_missing");return;}
    server.sendHeader("Content-Disposition","attachment; filename=fort-"+dateForDay(day)+".csv");
    server.sendHeader("Cache-Control","no-store");
    if(day==s.day.number){server.send(200,"text/csv; charset=utf-8",csvDay(s.day));return;}
    if(!fsOK||xSemaphoreTake(fsMutex,pdMS_TO_TICKS(100))!=pdTRUE){jsonReply(503,"Storage is not available","storage_unavailable");return;}
    String p=dayPath(day);if(!LittleFS.exists(p))p+=".bak";
    File f=LittleFS.open(p,"r");
    // Copy small bounded CSV under lock; never keep FS locked for a slow client.
    String csv;if(f&&f.size()<8192)csv=f.readString();if(f)f.close();xSemaphoreGive(fsMutex);
    if(csv.length())server.send(200,"text/csv; charset=utf-8",csv);else jsonReply(404,"Archive is not available","archive_missing");
  });
  server.onNotFound([]{jsonReply(404,"Address not found","not_found");});server.begin();
}

struct Button {
  int pin;Debouncer debounce;
  explicit Button(int p):pin(p){}
  // 35 ms stable debounce; events: 1 short release, 2 long press once at 900 ms.
  uint8_t update(uint32_t now) {
    return debounce.update(digitalRead(pin),now);
  }
};
Button buttons[]={Button(PIN_PREV),Button(PIN_NEXT),Button(PIN_OK)};
uint8_t page=0,slotPage=0;uint32_t lastPaint=0;bool showCredentials=false;
void drawOLED(const Snapshot &s) {
  if(!oledOK||xSemaphoreTake(i2cMutex,pdMS_TO_TICKS(30))!=pdTRUE)return;
  oled.clearDisplay();oled.setTextColor(SSD1306_WHITE);oled.setTextSize(1);oled.setCursor(0,0);
  const char *titles[]={"MIDEA | ENERGY","MORNING INTERVAL","EVENING INTERVAL","HOTSPOT / WIFI","DIAGNOSTICS"};
  oled.println(titles[page]);oled.drawLine(0,11,127,11,SSD1306_WHITE);oled.setCursor(0,16);
  if(page==0){
    if(s.haveEnergy){String v=String(s.energy,2);oled.setTextSize(v.length()>10?1:2);oled.println(v);oled.setTextSize(1);oled.println("kWh import");}else oled.println("Waiting for meter");
    oled.setCursor(0,45);if(s.rtc){DateTime t(s.utc+LOCAL_OFFSET);char b[25];snprintf(b,sizeof(b),"%02u:%02u:%02u WIB",t.hour(),t.minute(),t.second());oled.println(b);}else oled.println("RTC: SET VIA WEB");
    oled.setCursor(98,45);oled.print(s.online?"OK":"OFF");
    oled.setCursor(0,56);oled.print(apOK?"IP 192.168.4.1":"AP FAILED");
  }else if(page==1||page==2){
    int i=page==1?slotPage%6:6+slotPage%3;const Slot &x=s.day.slots[i];int a=START_MIN[i],b=a+30;char t[24];snprintf(t,sizeof(t),"%02d:%02d - %02d:%02d",a/60,a%60,b/60,b%60);oled.println(t);
    oled.setTextSize(2);oled.setCursor(0,29);if(x.quality==COMPLETE)oled.print(String(x.end-x.start,3));else oled.print("--.---");oled.setTextSize(1);oled.setCursor(0,49);oled.println(qualityCode(x.quality));
  }else if(page==3){
    if(showCredentials){oled.println("Password:");oled.println(AP_PASSWORD);}else{oled.println("Midea Engineering");oled.println("AP");}
    oled.setCursor(0,42);oled.println(apOK?WiFi.softAPIP().toString():"AP START FAILED");oled.println("OK: password");
  }else{
    oled.print("CRC:");oled.print(s.crcErrors);oled.print(" T/O:");oled.println(s.timeouts);
    oled.print("RTC:");oled.print(s.rtc?"OK":"ERR");oled.print(" FS:");oled.println(s.storage?"OK":"ERR");
    oled.print("ID:");oled.print(s.config.slave);oled.print(" ");oled.print(s.config.baud);oled.println(s.config.parity);
    oled.println(s.muted?"ALARM MUTED":"ALARM ENABLED");
  }
  oled.display();xSemaphoreGive(i2cMutex);
}
void bootAnimation() {
  uint32_t start=millis();
  while(millis()-start<1500){
    tickLED();uint32_t t=millis()-start;digitalWrite(PIN_BUZZER,(t<140||(t>=250&&t<390)||(t>=500&&t<700)||(t>=850&&t<1120))?HIGH:LOW);
    if(oledOK){oled.clearDisplay();oled.setTextColor(SSD1306_WHITE);oled.setTextSize(1);oled.setCursor(15,7);oled.print("MIDEA ENGINEERING");oled.setTextSize(2);oled.setCursor(18,24);oled.print("FORT");
      int x=105;oled.drawLine(x,19,x-10,33,SSD1306_WHITE);oled.drawLine(x-10,33,x+1,33,SSD1306_WHITE);oled.drawLine(x+1,33,x-9,46,SSD1306_WHITE);oled.drawRect(10,53,108,6,SSD1306_WHITE);oled.fillRect(12,55,(t*104)/1500,2,SSD1306_WHITE);oled.display();}
    delay(35);
  }digitalWrite(PIN_BUZZER,LOW);
}
void setup() {
  Serial.begin(115200); // Native USB: never block waiting for a PC.
  pinMode(PIN_RS485_DE,OUTPUT);digitalWrite(PIN_RS485_DE,LOW);
  pinMode(PIN_BUZZER,OUTPUT);digitalWrite(PIN_BUZZER,LOW);
  for(auto &b:buttons)pinMode(b.pin,INPUT_PULLUP);
  i2cMutex=xSemaphoreCreateMutex();fsMutex=xSemaphoreCreateMutex();
  snapshotQueue=xQueueCreate(1,sizeof(Snapshot));commandQueue=xQueueCreate(6,sizeof(Command));
  if(!i2cMutex||!fsMutex||!snapshotQueue||!commandQueue){Serial.println("FATAL: RTOS allocation failed");delay(2000);ESP.restart();}
  Wire.begin(PIN_SDA,PIN_SCL,100000);Wire.setTimeOut(40);
  Wire.beginTransmission(OLED_ADDRESS);bool present=Wire.endTransmission()==0;
  oledOK=present&&oled.begin(SSD1306_SWITCHCAPVCC,OLED_ADDRESS,false,false);
  if(oledOK)oled.setRotation(OLED_ROTATE_180?2:0);
  initLED();bootAnimation();
  // Never format existing data automatically. First installation: hold OK at boot.
  fsOK=LittleFS.begin(false);
  if(!fsOK&&digitalRead(PIN_OK)==LOW){
    if(oledOK){oled.clearDisplay();oled.setTextSize(1);oled.setCursor(0,0);oled.println("Storage not ready");oled.println("Hold OK for 5 sec");oled.println("to ERASE archives");oled.display();}
    uint32_t t=millis();while(digitalRead(PIN_OK)==LOW&&millis()-t<5000){tickLED();delay(20);}
    if(millis()-t>=5000){LittleFS.end();fsOK=LittleFS.format()&&LittleFS.begin(false);}
  }
  WiFi.mode(WIFI_AP);WiFi.setSleep(false);
  IPAddress ip(192,168,4,1),mask(255,255,255,0);
  apOK=WiFi.softAPConfig(ip,ip,mask)&&WiFi.softAP(AP_SSID,AP_PASSWORD,6,false,4);
  char token[33];snprintf(token,sizeof(token),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());apiToken=token;
  state.led=defaultLed();state.power=NAN;xQueueOverwrite(snapshotQueue,&state);
  webSetup();esp_task_wdt_init(8,true);esp_task_wdt_add(nullptr);
  if(xTaskCreatePinnedToCore(acquisition,"fort-acquisition",12288,nullptr,2,nullptr,1)!=pdPASS){Serial.println("FATAL: cannot create acquisition task");delay(1000);ESP.restart();}
  Serial.printf("Midea Fort ready. AP: %s, IP: %s, UI core: %d\n",AP_SSID,WiFi.softAPIP().toString().c_str(),xPortGetCoreID());
}
void loop() {
  esp_task_wdt_reset();server.handleClient();Snapshot s{};readSnapshot(s);uint32_t now=millis();tickLED(&s.led);
  for(int i=0;i<3;++i){uint8_t event=buttons[i].update(now);if(!event)continue;
    if(i==0){page=(page+4)%5;showCredentials=false;}
    if(i==1){page=(page+1)%5;showCredentials=false;}
    if(i==2&&event==1){if(page==1||page==2)++slotPage;else if(page==3)showCredentials=!showCredentials;else{Command c{};c.type=TOGGLE_MUTE;xQueueSend(commandQueue,&c,0);}}
    if(i==2&&event==2){Command c{};c.type=TOGGLE_MUTE;xQueueSend(commandQueue,&c,0);}
  }
  bool alarm=!s.muted&&s.scan.phase!=SCAN_RUNNING&&now>20000&&(!s.rtc||!s.online||!s.storage);
  digitalWrite(PIN_BUZZER,alarm&&now%10000<120?HIGH:LOW);
  if(now-lastPaint>=300){lastPaint=now;drawOLED(s);}
  delay(5); // Both idle tasks must run; Wi-Fi needs scheduler headroom.
}
