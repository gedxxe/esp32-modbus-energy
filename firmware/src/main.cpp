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
#include "flow.h"
#include "process_monitor.h"
#include <FortDiagnostics.h>
#include "clock_guard.h"
#include "command_policy.h"
#include "bounded_write.h"

SET_LOOP_TASK_STACK_SIZE(16384);
using namespace fort;
static_assert(ARDUINO_RUNNING_CORE==0,"UI must run on core 0");
HardwareSerial bus(1);
Adafruit_SSD1306 oled(128,64,&Wire,-1);
void serviceHTTP();
class BoundedWebServer:public WebServer {
 public:
  explicit BoundedWebServer(uint16_t port):WebServer(port){}
  void handleClient()override{requestStarted=millis();WebServer::handleClient();}
 protected:
  uint32_t requestStarted=0;
  size_t _currentClientWrite(const char *data,size_t size)override{
    size_t sent=boundedWrite(data,size,requestStarted,5000,[](){return millis();},
      [this](){return bool(_currentClient.connected());},
      [this](const char *bytes,size_t count){int n=::send(_currentClient.fd(),bytes,count,MSG_DONTWAIT);return n>=0?n:(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?0:-1);},
      [](){serviceHTTP();});
    if(sent<size)_currentClient.stop();return sent;
  }
  size_t _currentClientWrite_P(PGM_P data,size_t size)override{return _currentClientWrite(data,size);}
};
BoundedWebServer server(80);
SemaphoreHandle_t i2cMutex,fsMutex;
QueueHandle_t snapshotQueue,commandQueue;
bool oledOK=false,apOK=false;
String apiToken;

struct Command { CommandType type; uint32_t epoch=0,id=0,queuedAt=0,modeRevision=0; Settings config{}; LedSettings led{}; uint8_t first=1,last=247; uint16_t address=meter::CURRENT_A;uint8_t words=2;bool currentOnly=false; };
struct CommandResult { uint32_t id=0;char error[80]={}; };
constexpr uint8_t COMMAND_RESULTS=8;
uint32_t acceptedCommand=0; // Core 0 only. Results are core 1 queue copies.
struct Snapshot {
  uint32_t modeRevision=0,completedCommand=0,rejectedCommand=0;
  char commandError[80]={};
  CommandResult commandResults[COMMAND_RESULTS]={};uint8_t commandResultNext=0;
  Day day{}; Settings config{}; ScanProgress scan{}; LedSettings led{}; TraceCapture trace{}; UartFlow flow{}; bool diagnosticActive=false,diagnosticManual=true; uint16_t scanAddress=meter::CURRENT_A;uint8_t scanWords=2;bool scanException=false;
  bool ledStored=false,ledStorageOK=false;
  uint32_t acquisitionCycleMs=0,utc=0,lastGoodMs=0,crcErrors=0,timeouts=0,rejected=0,busBusy=0;
  double energy=0,power=NAN,ratio=0,secondaryEnergy=NAN,currentA=NAN,voltageA=NAN,frequency=NAN;
  uint32_t lastContactMs=0,lastSecondaryMs=0;uint8_t modbusStatus=0xE2;
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
void serviceHTTP(){esp_task_wdt_reset();tickLED();vTaskDelay(1);}
constexpr size_t DIAGNOSTIC_LOG_SIZE=8192;
char diagnosticLog[DIAGNOSTIC_LOG_SIZE]{};
DiagnosticEngine normalReader(bus),diagnostic(bus,diagnosticLog,sizeof(diagnosticLog));
struct DiagnosticInput { uint32_t generation=0;char line[96]={}; };
struct DiagnosticStop { uint32_t generation=0;uint8_t action=0; };
uint32_t diagnosticInputGeneration=0; // Core 0 writer; copied in each message.
uint32_t diagnosticActiveGeneration=0; // Core 1 only.
struct DiagnosticView {
  uint32_t generation=0,discarded=0;bool active=false,busy=false;
  DiagnosticEngine::Evidence evidence{};
  char log[DIAGNOSTIC_LOG_SIZE+1]={};
};
QueueHandle_t diagnosticInputQueue,diagnosticStopQueue,diagnosticViewQueue;
DiagnosticView diagnosticView{}; // Core 1 writer; core 0 reads queue copies only.
bool diagnosticActive=false,resumeRequested=false,entryRequested=false;
uint32_t lastDiagnosticPublish=0,lastFlowPublish=0;
void serviceDiagnostic();
void uartEvent(uint8_t,const DiagnosticEngine::Link &,uint16_t,uint8_t,const uint8_t *,const uint8_t *,size_t,bool);
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
DiagnosticEngine::Link diagnosticLink(const Settings &settings){DiagnosticEngine::Link link;link.id=settings.slave;link.baud=settings.baud;link.parity=settings.parity;return link;}
void startBusFor(const Settings &config){normalReader.options.rx=PIN_RX;normalReader.options.tx=PIN_TX;normalReader.options.manualDirection=config.direction==DIRECTION_MANUAL;normalReader.openLink(diagnosticLink(config));}
void startBus(){startBusFor(state.config);}
uint8_t currentAttempt=1;bool readingNormally=true;
void uartEvent(uint8_t kind,const DiagnosticEngine::Link &link,uint16_t address,uint8_t words,const uint8_t *tx,const uint8_t *rx,size_t size,bool overflow){
  TraceResult result=kind==6?TRACE_TIMEOUT:TRACE_INVALID;
  if(kind==4&&tx)result=inspectTrace(tx,rx,size,overflow,link.id,words);
  if(kind==4||kind==5||kind==6){
    if(tx)state.trace.append(tx,rx,size,overflow,millis(),link.baud,link.parity,link.id,address,words,currentAttempt,scanner.running(),diagnosticActive,kind==5);
  }
  state.flow.append(kind,micros(),link.baud,link.parity,link.id,address,words,kind==1?tx:rx,kind==1?8:size,overflow,diagnosticActive,result,diagnosticActive?diagnostic.options.rx:normalReader.options.rx,diagnosticActive?diagnostic.options.tx:normalReader.options.tx);
}
// Production polling and diagnostics use the identical strict FC03 transport.
// Echoes and extra bytes remain visible and are rejected within a transaction.
bool readWordsFor(const Settings &config,uint16_t address,uint8_t count,uint8_t *out,uint8_t attempts=1,bool normal=true){
  if(count<1||count>3||uint32_t(address)+count>65536UL)return false;
  readingNormally=normal;
  for(uint8_t attempt=0;attempt<attempts;++attempt){
    currentAttempt=attempt+1;normalReader.abortRequested=false;
    const auto before=normalReader.evidence;
    normalReader.options.manualDirection=config.direction==DIRECTION_MANUAL;
    bool ok=normalReader.readRegisters(diagnosticLink(config),address,count,out);state.modbusStatus=normalReader.lastStatus;
    uint32_t badCRC=normalReader.evidence.crcBad-before.crcBad;
    uint32_t busy=normalReader.evidence.busBusy-before.busBusy;
    uint32_t rejected=(normalReader.evidence.echoes-before.echoes)+(normalReader.evidence.wrongFrames-before.wrongFrames)+(normalReader.evidence.overflows-before.overflows)+busy;
    uint32_t timeouts=normalReader.evidence.timeouts-before.timeouts;
    if(normal){state.crcErrors+=badCRC;state.rejected+=rejected;state.timeouts+=timeouts;state.busBusy+=busy;}
    else{scanner.progress.crcErrors+=badCRC;scanner.progress.rejected+=rejected;scanner.progress.timeouts+=timeouts;scanner.progress.busBusy+=busy;}
    if(ok){if(normal)state.lastContactMs=millis();return true;}
    if(normalReader.lastReply==DiagnosticEngine::REPLY_EXCEPTION){
      if(normal)state.lastContactMs=millis();
      if(normal)setError("Modbus exception. Register not read","modbus_exception");else ++scanner.progress.exceptions;
      return false;
    }
    if(busy){if(normal)setError("RX did not become quiet. Request not transmitted","bus_not_quiet");}
    else if(normalReader.evidence.txErrors!=before.txErrors){if(normal)setError("Local UART initialization or write failed","local_uart_error");else ++scanner.progress.rejected;}
    else if(normalReader.evidence.timeouts!=before.timeouts){if(normal)setError("Modbus response timeout","modbus_timeout");}
    else if(normalReader.evidence.crcBad!=before.crcBad){if(normal)setError("Response CRC mismatch","crc_mismatch");}
    else if(normalReader.evidence.shortFrames!=before.shortFrames){if(normal)setError("Incomplete Modbus response","invalid_frame");}
    else{if(!rejected){if(normal)++state.rejected;else ++scanner.progress.rejected;}if(normal)setError("RX did not contain an accepted response","invalid_frame");}
    if(attempt+1<attempts)vTaskDelay(pdMS_TO_TICKS(30*(attempt+1)));
  }
  return false;
}
bool readWords(uint16_t address,uint8_t count,uint8_t *out){return readWordsFor(state.config,address,count,out);}
void restoreAfterScan() {
  ++state.modeRevision;startBus();state.ratios=false;haveSample=false;rawKnown=false;state.power=NAN;
  lastRatio=0;lastPoll=millis();
  setError(scanner.progress.phase==SCAN_FOUND?"Scan finished. Result not applied":scanner.progress.phase==SCAN_CANCELLED?"Scan cancelled. Previous settings restored":"Scan finished without a match",
    scanner.progress.phase==SCAN_FOUND?"scan_result_pending":scanner.progress.phase==SCAN_CANCELLED?"scan_was_cancelled":"scan_no_meter");
}
void scanStep() {
  const Settings candidate=scanner.progress.candidate;
  startBusFor(candidate);uint8_t data[6];
  bool dataRead=readWordsFor(candidate,state.scanAddress,state.scanWords,data,1,false);
  bool found=dataRead||normalReader.lastReply==DiagnosticEngine::REPLY_EXCEPTION;
  if(found){bool exception=!dataRead;bool confirmed=readWordsFor(candidate,state.scanAddress,state.scanWords,data,1,false);found=confirmed||normalReader.lastReply==DiagnosticEngine::REPLY_EXCEPTION;state.scanException=exception||!confirmed;}
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
  state.diagnosticManual=diagnostic.options.manualDirection;state.day=tracker.day;state.muted=muted;state.storage=fsOK&&nvsOK&&archiveOK&&checkpointOK&&configOK;
  state.lastGoodMs=lastGood;state.haveEnergy=haveSample;
  state.online=state.lastContactMs&&uint32_t(millis()-state.lastContactMs)<=15000;
  xQueueOverwrite(snapshotQueue,&state);
}

void publishDiagnostic(){
  diagnosticView.active=diagnosticActive;diagnosticView.busy=diagnostic.busy();diagnosticView.evidence=diagnostic.evidence;
  diagnosticView.discarded=diagnostic.logDiscarded;diagnostic.copyLog(diagnosticView.log,sizeof(diagnosticView.log));
  xQueueOverwrite(diagnosticViewQueue,&diagnosticView);lastDiagnosticPublish=millis();
}
void serviceDiagnosticStop(){
  DiagnosticStop stop;
  if(xQueueReceive(diagnosticStopQueue,&stop,0)==pdTRUE){diagnosticActiveGeneration=stop.generation;diagnostic.emergencyStop();if(stop.action==2){resumeRequested=true;entryRequested=false;}}
}
void serviceDiagnostic(){
  esp_task_wdt_reset();diagnostic.drainSerial();serviceDiagnosticStop();
  if(uint32_t(millis()-lastFlowPublish)>=50){publish();lastFlowPublish=millis();}
  if(diagnosticActive&&!resumeRequested){
    DiagnosticInput input;
    for(uint8_t n=0;n<4&&diagnostic.consoleCount<DiagnosticEngine::CONSOLE_CAPACITY&&xQueueReceive(diagnosticInputQueue,&input,0)==pdTRUE;++n){
      if(input.generation!=diagnosticActiveGeneration)continue;
      if(!strcmp(input.line,"resume")){resumeRequested=true;diagnostic.emergencyStop();}
      else if(!diagnostic.submit(input.line))diagnostic.logf("[CONSOLE_FULL] command refused. Wait for queued work\n");
    }
    if(uint32_t(millis()-lastDiagnosticPublish)>=100){publishDiagnostic();publish();}
  }
}
void enterDiagnostic(){
  if(diagnosticActive||scanner.running()||uxQueueMessagesWaiting(commandQueue))return;
  ++state.modeRevision;diagnosticActive=true;state.diagnosticActive=true;publish();
  tracker.interrupt();saveCheckpoint(true);haveSample=false;rawKnown=false;state.lastContactMs=0;state.ratios=false;state.power=NAN;
  currentAttempt=1;entryRequested=false;resumeRequested=false;diagnostic.activeBaud=0;
  // Normal reader retains shared library defaults and register plans.
  diagnostic.options=normalReader.options;diagnostic.options.rx=PIN_RX;diagnostic.options.tx=PIN_TX;diagnostic.options.manualDirection=state.config.direction==DIRECTION_MANUAL;
  memcpy(diagnostic.auditPlan,normalReader.auditPlan,sizeof(diagnostic.auditPlan));diagnostic.auditCount=normalReader.auditCount;
  memcpy(diagnostic.BAUDS,normalReader.BAUDS,sizeof(diagnostic.BAUDS));memcpy(diagnostic.PARITIES,normalReader.PARITIES,sizeof(diagnostic.PARITIES));
  diagnostic.baudCount=normalReader.baudCount;diagnostic.parityCount=normalReader.parityCount;diagnostic.firstID=normalReader.firstID;diagnostic.lastID=normalReader.lastID;
  diagnostic.connectedCapture=DiagnosticEngine::Capture{};diagnostic.isolatedCapture=DiagnosticEngine::Capture{};
  diagnostic.current=diagnosticLink(state.config);diagnostic.evidence=DiagnosticEngine::Evidence{};diagnostic.clearLog();
  diagnostic.parityErrors.store(0);diagnostic.frameErrors.store(0);diagnostic.fifoErrors.store(0);diagnostic.bufferErrors.store(0);diagnostic.breakErrors.store(0);
  diagnostic.serialInputEnabled=true;diagnostic.serialOutput=true;++diagnosticView.generation;
  diagnostic.logf("[MAINTENANCE] Scheduled recording interrupted. Test settings RAM-only. Type resume or choose Resume measurement when finished.\n");diagnostic.help();diagnostic.showStatus();publishDiagnostic();publish();
}
void leaveDiagnostic(){
  diagnostic.emergencyStop();diagnostic.serialInputEnabled=false;
  ++state.modeRevision;diagnosticActive=false;state.diagnosticActive=false;resumeRequested=false;
  diagnostic.activeBaud=0;normalReader.activeBaud=0;normalReader.options=DiagnosticEngine::DebugOptions{};startBus();
  haveSample=false;rawKnown=false;state.lastContactMs=0;state.ratios=false;state.power=NAN;lastRatio=0;lastPoll=millis();
  diagnostic.logf("[RESUMED] Saved production settings restored. Test readings were never fed into interval records.\n");publishDiagnostic();publish();
}
char serialControl[16]={};uint8_t serialControlUsed=0;
void pollDebugEntry(){
  if(diagnosticActive||entryRequested)return;
  for(unsigned i=0;i<32&&Serial.available();++i){char ch=char(Serial.read());if(ch=='\r')continue;
    if(ch=='\n'){serialControl[serialControlUsed]=0;if(!strcmp(serialControl,"debug")){entryRequested=true;serialControlUsed=0;break;}else if(serialControlUsed)Serial.println("Main firmware: type debug + Enter to enter diagnostics. Web: Meter setup / Diagnostic console.");serialControlUsed=0;}
    else if(serialControlUsed<sizeof(serialControl)-1)serialControl[serialControlUsed++]=ch;
  }
}

// Runs only at core-1 transaction boundaries, never from the UART service hook.
// 202 acknowledges admission; each completion/rejection is visible in status.
void completeCommand(const Command &cmd,const char *error=""){
  if(!cmd.id)return; // Physical buttons do not have an HTTP acknowledgement.
  state.completedCommand=cmd.id;
  if(*error){state.rejectedCommand=cmd.id;strlcpy(state.commandError,error,sizeof(state.commandError));}
  CommandResult &result=state.commandResults[state.commandResultNext];
  result.id=cmd.id;strlcpy(result.error,error,sizeof(result.error));
  state.commandResultNext=(state.commandResultNext+1)%COMMAND_RESULTS;publish();
}
void processCommands(){
  Command cmd;
  for(uint8_t handled=0;handled<6&&xQueueReceive(commandQueue,&cmd,0)==pdTRUE;++handled){
    esp_task_wdt_reset();
    if(!commandContextValid(cmd.type,cmd.modeRevision,state.modeRevision,cmd.queuedAt,millis(),diagnosticActive,scanner.running(),scanner.progress.phase==SCAN_FOUND)){
      completeCommand(cmd,"Device mode changed or command expired. Refresh status and retry");continue;
    }
    const char *error="";
    switch(cmd.type){
      case TOGGLE_MUTE:muted=!muted;break;
      case START_TRACE:
        if(state.trace.active)error="A raw capture is already running";else state.trace.begin();break;
      case STOP_TRACE:state.trace.stop();break;
      case SET_LED:{
        if(!RGB_LED_ENABLED||!validLed(cmd.led)){error="Invalid or unavailable LED configuration";break;}
        state.led=cmd.led;uint8_t bytes[LED_RECORD_SIZE];encodeLed(state.led,bytes);
        state.ledStored=nvsOK&&prefs.putBytes("led",bytes,sizeof(bytes))==sizeof(bytes);state.ledStorageOK=state.ledStored;
        if(!state.ledStored)error="LED changed in RAM. Saving settings failed";break;
      }
      case SET_TIME:{
        uint32_t effectiveUTC=0;
        auto result=resolveQueuedClockTime(cmd.epoch,cmd.queuedAt,millis(),LOCAL_OFFSET,tracker.day.number,effectiveUTC);
        if(result!=CLOCK_ACCEPTED){error="RTC request expired or precedes retained records. Sync again";break;}
        tracker.interrupt();clockKnown=false;rawKnown=false;
        if(!rtcWrite(effectiveUTC)){error="RTC write or verification failed";setError(error,"rtc_write_failed");}
        saveCheckpoint(true);break;
      }
      case START_SCAN:
        if(!scanner.begin(cmd.first,cmd.last,state.config,millis(),cmd.currentOnly)){error="Invalid scan request";break;}
        ++state.modeRevision;state.scanAddress=cmd.address;state.scanWords=cmd.words;state.scanException=false;
        tracker.interrupt();saveCheckpoint(true);rawKnown=false;haveSample=false;state.ratios=false;state.power=NAN;
        setError("Scan running. Recording paused","scanning");break;
      case CANCEL_SCAN:
        if(scanner.cancel(millis()))restoreAfterScan();else error="Scan already finished";break;
      case APPLY_SCAN:
      case SET_CONFIG:{
        Settings settings=cmd.type==APPLY_SCAN?scanner.progress.result:cmd.config;
        if(!validSettings(settings)){error="Invalid serial configuration";break;}
        ++state.modeRevision;tracker.interrupt();state.config=settings;state.ratios=false;rawKnown=false;haveSample=false;state.lastContactMs=0;
        ConfigRecord record{};record.settings=state.config;record.crc=crc16(reinterpret_cast<uint8_t*>(&record),offsetof(ConfigRecord,crc));
        configOK=nvsOK&&prefs.putBytes("config",&record,sizeof(record))==sizeof(record);
        if(!configOK)error="Connection changed in RAM. Saving settings failed";
        saveCheckpoint(true);startBus();lastRatio=0;lastPoll=0;break;
      }
      default:error="Unsupported command";break;
    }
    completeCommand(cmd,error);publish();
  }
}

void acquisition(void *) {
  esp_task_wdt_add(nullptr);
  nvsOK=prefs.begin("fort",false);Settings config{};
  if(nvsOK&&prefs.isKey("config")) {
    ConfigRecord record{};
    bool read=prefs.getBytesLength("config")==sizeof(record)&&prefs.getBytes("config",&record,sizeof(record))==sizeof(record);
    uint16_t c=crc16(reinterpret_cast<uint8_t*>(&record),offsetof(ConfigRecord,crc));
    if(read&&record.crc==c&&validSettings(record.settings))config=record.settings;
    else if(read&&record.crc==c&&migrateLegacySettings(record.settings)){
      config=record.settings;ConfigRecord migrated{};migrated.settings=config;migrated.crc=crc16(reinterpret_cast<uint8_t*>(&migrated),offsetof(ConfigRecord,crc));
      configOK=prefs.putBytes("config",&migrated,sizeof(migrated))==sizeof(migrated);
    }
    else {config=Settings{};configOK=false;setError("Stored configuration invalid. Defaults loaded","config_corrupt");}
  }
  state.config=config;state.led=defaultLed();state.ledStorageOK=nvsOK;
  if(nvsOK&&prefs.isKey("led")) {
    uint8_t bytes[LED_RECORD_SIZE]={};
    state.ledStored=prefs.getBytesLength("led")==sizeof(bytes)&&prefs.getBytes("led",bytes,sizeof(bytes))==sizeof(bytes)&&decodeLed(bytes,sizeof(bytes),state.led);
    state.ledStorageOK=state.ledStored;
  }
  if(nvsOK&&prefs.isKey("day")&&prefs.getBytesLength("day")==sizeof(Checkpoint)) {
    Checkpoint c{};prefs.getBytes("day",&c,sizeof(c));
    if(c.magic==0x46524432&&c.crc==crc16(reinterpret_cast<uint8_t*>(&c),offsetof(Checkpoint,crc))) {tracker.day=c.day;tracker.interrupt();}
    else checkpointOK=false;
  }
  normalReader.service=serviceDiagnostic;normalReader.event=uartEvent;diagnostic.service=serviceDiagnostic;diagnostic.event=uartEvent;diagnostic.directionPinReserved=!RS485_AUTO_DIRECTION;diagnostic.onResume=[](){resumeRequested=true;diagnostic.emergencyStop();};
  startBus();uint32_t previousCycle=millis(),lastRatioTry=0,lastContactTry=millis()-3000;bool previousRTC=false;
  while(true) {
    state.acquisitionCycleMs=millis()-previousCycle;previousCycle=millis();
    esp_task_wdt_reset();pollDebugEntry();
    serviceDiagnostic();
    DiagnosticInput next;bool haveNext=false;
    if(!diagnosticActive){
      for(uint8_t handled=0;handled<16&&xQueueReceive(diagnosticInputQueue,&next,0)==pdTRUE;++handled){
        if(next.generation!=diagnosticActiveGeneration)continue;
        bool passive=!strcmp(next.line,"report")||!strcmp(next.line,"status")||!strcmp(next.line,"help")||!strcmp(next.line,"compare")||!strcmp(next.line,"plan show");
        if(passive){
          if(!diagnosticView.generation){diagnostic.current=diagnosticLink(state.config);diagnostic.options.rx=PIN_RX;diagnostic.options.tx=PIN_TX;}
          diagnostic.logf("[PRODUCTION_SETTINGS] ID=%u baud=%lu 8%c1 RX=%d TX=%d. Test settings below are RAM only\n",state.config.slave,(unsigned long)state.config.baud,state.config.parity,PIN_RX,PIN_TX);
          diagnostic.command(next.line);publishDiagnostic();
        }else{haveNext=true;break;}
      }
    }
    processCommands();
    if(!diagnosticActive&&(entryRequested||haveNext)&&!scanner.running())enterDiagnostic();
    if(haveNext){
      serviceDiagnosticStop(); // Preserve FIFO: submit the held first input before transferring later lines.
      if(next.generation!=diagnosticActiveGeneration||resumeRequested){diagnostic.logf("[CANCELLED] Pending command cancelled by stop/resume\n");}
      else if(diagnosticActive){if(!diagnostic.submit(next.line))diagnostic.logf("[CONSOLE_FULL] Command refused. Retry after current work.\n");}
      else{diagnostic.logf("[COMMAND_REJECTED] Production scan or pending controls own UART. Retry after they finish.\n");publishDiagnostic();}
    }
    if(diagnosticActive){if(resumeRequested)leaveDiagnostic();else{readingNormally=false;diagnostic.tick();publishDiagnostic();publish();vTaskDelay(pdMS_TO_TICKS(5));continue;}}
    uint32_t utc=0;bool rtcOK=rtcRead(utc);
    if(rtcOK&&!protectsRecordedDay(utc,LOCAL_OFFSET,tracker.day.number)){rtcOK=false;tracker.interrupt();rawKnown=false;setError("RTC date precedes retained records. Correct RTC via web","rtc_before_records");}
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
    if(!state.lastContactMs||uint32_t(millis()-state.lastContactMs)>15000){
      if(uint32_t(millis()-lastContactTry)<3000){publish();vTaskDelay(pdMS_TO_TICKS(50));continue;}
      lastContactTry=millis();uint8_t handshake[4];
      bool contactData=readWordsFor(state.config,meter::CURRENT_A,2,handshake,1,true);
      if(!contactData&&normalReader.lastReply!=DiagnosticEngine::REPLY_EXCEPTION){state.currentA=NAN;state.voltageA=NAN;state.frequency=NAN;publish();vTaskDelay(pdMS_TO_TICKS(50));continue;}
      state.lastContactMs=millis();state.currentA=contactData?double(be32(handshake))/10000.0:NAN;
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
      bool energyRead=readWords(meter::SECONDARY_ENERGY,2,e);
      if(energyRead){state.secondaryEnergy=double(be32(e))/1000.0;state.lastSecondaryMs=millis();state.lastContactMs=millis();}
      bool energyOK=state.ratios&&energyRead;
      bool endOK=rtcRead(endUTC);
      if(energyOK) {
        uint32_t raw=be32(e);bool accepted=true;
        if(rawKnown&&raw<previousRaw) {
          // A wrap cannot be distinguished reliably from a reset: invalidate continuity.
          tracker.interrupt();++state.rejected;setError("Counter decreased. Continuity interrupted","counter_reset");
        }
        double energy=raw*0.001*state.ratio;
        // Physical sanity limit: editable compile-time plant limit (100 MW).
        if(rawKnown&&raw>=previousRaw&&millis()!=previousMono) {
          double maxDelta=100000.0*(millis()-previousMono)/3600000.0+0.002*state.ratio;
          if((raw-previousRaw)*0.001*state.ratio>maxDelta){accepted=false;++state.rejected;setError("Energy increase exceeds configured limit","energy_jump");}
        }
        if(accepted) {
          state.energy=energy;setError("Reading valid","acquisition_ok");haveSample=true;lastGood=millis();previousRaw=raw;previousMono=millis();rawKnown=true;
          uint32_t local=endUTC+LOCAL_OFFSET;
          if(beginOK&&endOK&&endUTC>=beginUTC&&endUTC-beginUTC<=2&&millis()-beginMono<=2500&&local/86400==tracker.day.number) {
            uint8_t before[SLOT_COUNT];for(int i=0;i<SLOT_COUNT;++i)before[i]=tracker.day.slots[i].quality;
            if(tracker.sample(local,energy)) {
              for(int i=0;i<SLOT_COUNT;++i)if(before[i]!=tracker.day.slots[i].quality){saveCheckpoint(true);break;}
            }
          }else tracker.interrupt();
        }
      }
      if(readWordsFor(state.config,meter::POWER_TOTAL,2,p,1,true))state.power=meter::signed32(be32(p))/10000.0;else state.power=NAN;
    }
    if(haveSample&&millis()-lastGood>15000)tracker.interrupt();
    static uint32_t lastElectrical=0;
    if(uint32_t(millis()-lastElectrical)>=10000&&state.lastContactMs&&uint32_t(millis()-state.lastContactMs)<15000){
      lastElectrical=millis();uint8_t raw[4];
      state.currentA=readWordsFor(state.config,meter::CURRENT_A,2,raw,1,true)?double(be32(raw))/10000.0:NAN;
      state.voltageA=readWordsFor(state.config,meter::VOLTAGE_A,2,raw,1,true)?double(be32(raw))/10000.0:NAN;
      state.frequency=readWordsFor(state.config,meter::FREQUENCY,1,raw,1,true)?double((uint16_t(raw[0])<<8)|raw[1])/100.0:NAN;
    }
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
void enqueueCommand(const Command &input) {
  Snapshot s{};readSnapshot(s);
  if(!commandModeAllowed(input.type,s.diagnosticActive,s.scan.phase==SCAN_RUNNING,s.scan.phase==SCAN_FOUND)){
    jsonReply(409,"Device mode changed. Refresh status and retry","mode_changed");return;
  }
  Command c=input;c.id=acceptedCommand+1;if(!c.id)c.id=1;c.queuedAt=millis();c.modeRevision=s.modeRevision;
  if(xQueueSend(commandQueue,&c,0)==pdTRUE){
    acceptedCommand=c.id;JsonDocument d;d["accepted"]=true;d["code"]="accepted";d["commandId"]=c.id;
    String out;serializeJson(d,out);server.send(202,"application/json",out);
  }else jsonReply(503,"Command queue is busy. Try again.","queue_busy");
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
    JsonObject control=d["control"].to<JsonObject>();control["accepted"]=acceptedCommand;control["completed"]=s.completedCommand;control["rejected"]=s.rejectedCommand;control["lastError"]=s.commandError;control["pending"]=uxQueueMessagesWaiting(commandQueue);
    JsonArray outcomes=control["results"].to<JsonArray>();for(const auto &result:s.commandResults)if(result.id){JsonObject row=outcomes.add<JsonObject>();row["id"]=result.id;row["error"]=result.error;}
    JsonObject led=d["led"].to<JsonObject>();
    led["available"]=RGB_LED_ENABLED&&ledDriverOK;led["pin"]=RGB_LED_PIN;led["mode"]=ledModeName(s.led.mode);
    led["brightness"]=s.led.brightness;led["period"]=s.led.periodMs;
    led["red"]=s.led.red;led["green"]=s.led.green;led["blue"]=s.led.blue;
    led["stored"]=s.ledStored;led["storageOK"]=s.ledStorageOK;
    d["transport"]="ModbusMaster 2.0.1+fort.1";d["direction"]=s.config.direction==DIRECTION_MANUAL?"manual":"auto";d["directionPin"]=PIN_RS485_DE;d["settleUs"]=100;d["modbusStatus"]=s.modbusStatus;
    if(isfinite(s.secondaryEnergy)){d["secondaryEnergy"]=s.secondaryEnergy;d["secondaryAge"]=(millis()-s.lastSecondaryMs)/1000;}else d["secondaryEnergy"]=nullptr;
    d["currentA"]=nullptr;d["voltageA"]=nullptr;d["frequency"]=nullptr;
    if(isfinite(s.currentA))d["currentA"]=s.currentA;if(isfinite(s.voltageA))d["voltageA"]=s.voltageA;if(isfinite(s.frequency))d["frequency"]=s.frequency;
    if(s.lastContactMs)d["contactAge"]=(millis()-s.lastContactMs)/1000;else d["contactAge"]=nullptr;
    d["diagnosticActive"]=s.diagnosticActive;d["version"]=FIRMWARE_VERSION;d["token"]=apiToken;d["rtc"]=s.rtc;d["storage"]=s.storage;d["ratios"]=s.ratios;d["muted"]=s.muted;
    bool online=s.online&&uint32_t(millis()-s.lastContactMs)<=15000;
    d["online"]=online;d["energy"]=nullptr;d["power"]=nullptr;d["age"]=nullptr;
    if(s.haveEnergy){d["energy"]=s.energy;d["age"]=(millis()-s.lastGoodMs)/1000;}
    if(online&&isfinite(s.power))d["power"]=s.power;
    d["ratio"]=s.ratio;d["slave"]=s.config.slave;d["baud"]=s.config.baud;d["parity"]=String(s.config.parity);
    JsonObject scan=d["scan"].to<JsonObject>();
    scan["confirmations"]=s.scan.phase==SCAN_FOUND?2:0;scan["direction"]=s.config.direction==DIRECTION_MANUAL?"manual":"auto";scan["mode"]=s.scan.currentOnly?"current":"matrix";scan["address"]=s.scanAddress;scan["words"]=s.scanWords;scan["exception"]=s.scanException;scan["profileVerified"]=false;
    scan["phase"]=scanName(s.scan.phase);scan["tested"]=s.scan.tested;scan["total"]=s.scan.total;
    scan["first"]=s.scan.first;scan["last"]=s.scan.last;scan["elapsed"]=s.scan.elapsedMs/1000;
    scan["slave"]=s.scan.candidate.slave;scan["baud"]=s.scan.candidate.baud;scan["parity"]=String(s.scan.candidate.parity);
    scan["busBusy"]=s.scan.busBusy;scan["exceptions"]=s.scan.exceptions;scan["timeouts"]=s.scan.timeouts;scan["crcErrors"]=s.scan.crcErrors;scan["rejected"]=s.scan.rejected;
    if(s.scan.phase==SCAN_FOUND){JsonObject result=scan["result"].to<JsonObject>();result["slave"]=s.scan.result.slave;result["baud"]=s.scan.result.baud;result["parity"]=String(s.scan.result.parity);result["direction"]=s.scan.result.direction==DIRECTION_MANUAL?"manual":"auto";}
    d["busBusy"]=s.busBusy;d["crcErrors"]=s.crcErrors;d["timeouts"]=s.timeouts;d["rejected"]=s.rejected;d["heap"]=ESP.getFreeHeap();d["uptime"]=millis()/1000;d["error"]=s.error;d["error_code"]=s.errorCode;
    if(s.rtc){DateTime t(s.utc+LOCAL_OFFSET);char h[12];snprintf(h,sizeof(h),"%02u:%02u:%02u",t.hour(),t.minute(),t.second());d["clock"]=h;d["date"]=dateForDay((s.utc+LOCAL_OFFSET)/86400);}
    d["dayDate"]=s.day.number?dateForDay(s.day.number):"";
    JsonArray rows=d["slots"].to<JsonArray>();
    for(int i=0;i<SLOT_COUNT;++i){const Slot &x=s.day.slots[i];JsonObject r=rows.add<JsonObject>();r["start"]=START_MIN[i];r["quality"]=qualityName(x.quality);r["quality_code"]=qualityCode(x.quality);r["kwh"]=nullptr;if(x.quality==COMPLETE&&x.end>=x.start)r["kwh"]=x.end-x.start;else if(x.quality==ACTIVE&&x.hasStart&&x.last>=x.start)r["kwh"]=x.last-x.start;}
    String out;serializeJson(d,out);server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
  });
  // Short event pages preserve order and make loss explicit. No UART reads here.
  server.on("/api/system",HTTP_GET,[]{
    static CoreLoadSample samples;samples.update(millis());Snapshot s{};readSnapshot(s);JsonDocument d;
    d["uptime"]=millis()/1000;d["cpuMHz"]=getCpuFrequencyMhz();d["heapFree"]=ESP.getFreeHeap();d["heapMin"]=ESP.getMinFreeHeap();d["heapLargest"]=ESP.getMaxAllocHeap();d["heapTotal"]=ESP.getHeapSize();d["psramFree"]=ESP.getFreePsram();d["psramTotal"]=ESP.getPsramSize();d["taskCount"]=uxTaskGetNumberOfTasks();d["sampledAt"]=samples.sampledAt;
    d["diagnosticActive"]=s.diagnosticActive;d["scanActive"]=s.scan.phase==SCAN_RUNNING;d["uiCycleMs"]=uiCycleMs;d["acquisitionCycleMs"]=s.acquisitionCycleMs;
    d["commandQueue"]=uxQueueMessagesWaiting(commandQueue);d["diagnosticQueue"]=uxQueueMessagesWaiting(diagnosticInputQueue);
    JsonArray cores=d["cores"].to<JsonArray>();for(unsigned i=0;i<2;++i){JsonObject core=cores.add<JsonObject>();core["id"]=i;core["load"]=nullptr;if(samples.ready[i])core["load"]=samples.percent[i];core["method"]="sampled_non_idle_ticks";}
    JsonArray tasks=d["tasks"].to<JsonArray>();
    TaskHandle_t handles[]={uiTask,acquisitionTask,idleHandles[0],idleHandles[1]};const char *names[]={"loopTask / HTTP + UI","fort-acquisition / UART","IDLE0","IDLE1"};const uint8_t pinned[]={0,1,0,1};const char *states[]={"running","ready","blocked","suspended","deleted","invalid"};
    for(unsigned i=0;i<4;++i)if(handles[i]){JsonObject task=tasks.add<JsonObject>();task["name"]=names[i];task["core"]=pinned[i];task["priority"]=uxTaskPriorityGet(handles[i]);task["stackFreeMin"]=uxTaskGetStackHighWaterMark(handles[i])*sizeof(StackType_t);unsigned state=unsigned(eTaskGetState(handles[i]));task["state"]=states[state<6?state:5];}
    d["taskListScope"]="owned_and_idle";
    String out;serializeJson(d,out);server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
  });
  server.on("/api/uart",HTTP_GET,[]{
    Snapshot s{};readSnapshot(s);uint32_t after=0;
    if(server.hasArg("after")&&!parseNumber(server.arg("after"),after)){jsonReply(400,"Invalid event cursor","invalid_cursor");return;}
    JsonDocument d;d["boot"]=s.flow.boot;d["sequence"]=s.flow.sequence;d["nowUs"]=micros();d["diagnosticActive"]=s.diagnosticActive;
    d["autoDirection"]=s.diagnosticActive?!s.diagnosticManual:s.config.direction==DIRECTION_AUTO;d["capacity"]=FLOW_CAPACITY;d["oldest"]=s.flow.count?s.flow.at(0).seq:0;
    d["gap"]=s.flow.count&&after&&int32_t(s.flow.at(0).seq-after)>1;
    JsonArray rows=d["events"].to<JsonArray>();
    for(uint8_t i=0;i<s.flow.count;++i){const FlowEvent &e=s.flow.at(i);if(after&&int32_t(e.seq-after)<=0)continue;
      JsonObject row=rows.add<JsonObject>();row["rxPin"]=e.rxPin;row["txPin"]=e.txPin;row["seq"]=e.seq;row["atUs"]=e.atUs;row["kind"]=e.kind;row["baud"]=e.baud;row["parity"]=String(e.parity);row["slave"]=e.slave;row["address"]=e.address;row["words"]=e.words;row["diagnostic"]=e.diagnostic;row["overflow"]=e.overflow;row["result"]=traceResultName(e.result);row["bytes"]=hexFrame(e.bytes,e.size);
    }
    String out;serializeJson(d,out);server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
  });
  server.on("/api/diagnostic",HTTP_GET,[]{
    static DiagnosticView view; // UI core only; avoid an 8 KiB stack frame.
    if(xQueuePeek(diagnosticViewQueue,&view,0)!=pdTRUE){view.active=false;view.busy=false;view.log[0]=0;}
    JsonDocument d;d["pendingCommands"]=uxQueueMessagesWaiting(diagnosticInputQueue);d["version"]=FIRMWARE_VERSION;d["active"]=view.active;d["busy"]=view.busy;d["generation"]=view.generation;d["discardedBytes"]=view.discarded;d["log"]=view.log;
    const auto &e=view.evidence;JsonObject counts=d["evidence"].to<JsonObject>();
    counts["requests"]=e.requests;counts["rxBytes"]=e.rxBytes;counts["passiveBytes"]=e.passiveBytes;counts["preTxBytes"]=e.staleBytes;counts["busBusy"]=e.busBusy;counts["data"]=e.data;counts["exceptions"]=e.exceptions;counts["echoes"]=e.echoes;counts["echoPrefixes"]=e.echoPrefixes;counts["crcBad"]=e.crcBad;counts["shortFrames"]=e.shortFrames;counts["wrongFrames"]=e.wrongFrames;counts["overflows"]=e.overflows;counts["timeouts"]=e.timeouts;counts["localErrors"]=e.txErrors;counts["energyOK"]=e.energyOK;counts["energyBad"]=e.energyBad;counts["aborted"]=e.aborted;
    String out;serializeJson(d,out);server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
  });
  server.on("/api/diagnostic",HTTP_POST,[]{
    if(!authorizePost())return;String line=server.arg("command");
    if(!line.length()||line.length()>95){jsonReply(400,"Use one ASCII command, at most 95 characters","invalid_diagnostic_command");return;}
    for(size_t i=0;i<line.length();++i)if(line[i]<32||line[i]>126){jsonReply(400,"One printable ASCII command per request","invalid_diagnostic_command");return;}
    if(line=="x"||line=="stop"||line=="resume"){
      DiagnosticStop stop;stop.action=line=="resume"?2:1;stop.generation=++diagnosticInputGeneration;xQueueOverwrite(diagnosticStopQueue,&stop);jsonReply(202,"Stop/resume queued","accepted");return;
    }
    Snapshot s{};readSnapshot(s);if(s.scan.phase==SCAN_RUNNING){jsonReply(409,"Cancel production scan first","scan_busy");return;}
    DiagnosticInput input;input.generation=diagnosticInputGeneration;strlcpy(input.line,line.c_str(),sizeof(input.line));
    if(xQueueSend(diagnosticInputQueue,&input,0)==pdTRUE)jsonReply(202,"Diagnostic command queued. Recording will pause","accepted");else jsonReply(503,"Diagnostic queue full","queue_busy");
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
      row["diagnostic"]=frame.diagnostic;row["incomplete"]=frame.incomplete;row["scan"]=frame.scan;row["overflow"]=frame.overflow;row["result"]=traceResultName(frame.result);
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
    if(s.diagnosticActive){jsonReply(409,"Resume measurement before changing saved settings","diagnostic_busy");return;}
    if(s.scan.phase==SCAN_RUNNING){jsonReply(409,"Cancel the scan before changing the connection","scan_busy");return;}
    uint32_t slave=0,baud=0;String parity=server.arg("parity");
    if(!parseNumber(server.arg("slave"),slave)||!parseNumber(server.arg("baud"),baud)||slave<1||slave>247||parity.length()!=1){jsonReply(400,"Invalid serial settings","invalid_config");return;}
    Command c{};c.type=SET_CONFIG;c.config.slave=slave;c.config.baud=baud;c.config.parity=parity[0];
    String direction=server.arg("direction");if(direction!="manual"&&direction!="auto"){jsonReply(400,"Choose manual or automatic direction","invalid_config");return;}c.config.direction=direction=="manual"?DIRECTION_MANUAL:DIRECTION_AUTO;
    if(!validSettings(c.config)){jsonReply(400,"Unsupported baud rate or parity","unsupported_serial");return;}enqueueCommand(c);
  });
  server.on("/api/scan/start",HTTP_POST,[]{
    if(!authorizePost())return;Snapshot s{};readSnapshot(s);
    if(s.diagnosticActive){jsonReply(409,"Resume measurement before starting production scan","diagnostic_busy");return;}
    if(s.scan.phase==SCAN_RUNNING){jsonReply(409,"A scan is already running","scan_running_error");return;}
    uint32_t first=0,last=0;
    if(!parseNumber(server.arg("first"),first)||!parseNumber(server.arg("last"),last)||first<1||last>247||first>last){jsonReply(400,"Use IDs 1-247, with first ID <= last ID","invalid_scan_range");return;}
    Command c{};c.type=START_SCAN;c.first=first;c.last=last;
    uint32_t address=meter::CURRENT_A,words=2;
    if(server.hasArg("address")&&!parseNumber(server.arg("address"),address)){jsonReply(400,"Invalid probe address","invalid_diagnostic_command");return;}
    if(server.hasArg("words")&&!parseNumber(server.arg("words"),words)){jsonReply(400,"Invalid word count","invalid_diagnostic_command");return;}
    if(address>65535||words<1||words>3||address+words>65536){jsonReply(400,"Probe outside supported register bounds","invalid_diagnostic_command");return;}
    String mode=server.arg("mode");if(mode.length()&&mode!="current"&&mode!="matrix"){jsonReply(400,"Invalid scan mode","invalid_diagnostic_command");return;}
    c.currentOnly=mode=="current";c.address=address;c.words=words;enqueueCommand(c);
  });
  server.on("/api/scan/cancel",HTTP_POST,[]{if(!authorizePost())return;Command c{};c.type=CANCEL_SCAN;enqueueCommand(c);});
  server.on("/api/scan/apply",HTTP_POST,[]{
    if(!authorizePost())return;Snapshot s{};readSnapshot(s);
    if(s.diagnosticActive){jsonReply(409,"Resume measurement before applying production settings","diagnostic_busy");return;}
    if(s.scan.phase!=SCAN_FOUND){jsonReply(409,"No scan result is available","no_scan_result");return;}
    Command c{};c.type=APPLY_SCAN;enqueueCommand(c);
  });
  server.on("/api/time",HTTP_POST,[]{
    if(!authorizePost())return;Snapshot active{};readSnapshot(active);if(active.diagnosticActive){jsonReply(409,"Resume measurement before setting RTC","diagnostic_busy");return;}uint32_t epoch=0;
    if(!parseNumber(server.arg("epoch"),epoch)||!validClockEpoch(epoch,LOCAL_OFFSET)){jsonReply(400,"Time must be within 2024-2099","invalid_time");return;}
    Snapshot s{};readSnapshot(s);
    if(!protectsRecordedDay(epoch,LOCAL_OFFSET,s.day.number)){jsonReply(409,"Earlier date rejected to protect existing records","backward_date");return;}
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
    if(s.diagnosticActive){oled.println("UART DIAGNOSTICS");oled.println("Recording paused");oled.println("Resume via web");oled.display();xSemaphoreGive(i2cMutex);return;}
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
  diagnosticInputQueue=xQueueCreate(16,sizeof(DiagnosticInput));diagnosticStopQueue=xQueueCreate(1,sizeof(DiagnosticStop));diagnosticViewQueue=xQueueCreate(1,sizeof(DiagnosticView));
  state.flow.boot=esp_random();
  if(!i2cMutex||!fsMutex||!snapshotQueue||!commandQueue||!diagnosticInputQueue||!diagnosticStopQueue||!diagnosticViewQueue){Serial.println("FATAL: RTOS allocation failed");delay(2000);ESP.restart();}
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
  initProcessMonitor();webSetup();esp_task_wdt_init(8,true);esp_task_wdt_add(nullptr);
  if(xTaskCreatePinnedToCore(acquisition,"fort-acquisition",12288,nullptr,2,&acquisitionTask,1)!=pdPASS){Serial.println("FATAL: cannot create acquisition task");delay(1000);ESP.restart();}
  Serial.printf("Midea Fort ready. AP: %s, IP: %s, UI core: %d\n",AP_SSID,WiFi.softAPIP().toString().c_str(),xPortGetCoreID());
}
void loop() {
  static uint32_t previousCycle=millis();uiCycleMs=millis()-previousCycle;previousCycle=millis();
  esp_task_wdt_reset();server.handleClient();Snapshot s{};readSnapshot(s);uint32_t now=millis();tickLED(&s.led);
  for(int i=0;i<3;++i){uint8_t event=buttons[i].update(now);if(!event)continue;
    if(i==0){page=(page+4)%5;showCredentials=false;}
    if(i==1){page=(page+1)%5;showCredentials=false;}
    if(i==2&&event==1){if(page==1||page==2)++slotPage;else if(page==3)showCredentials=!showCredentials;else{Command c{};c.type=TOGGLE_MUTE;xQueueSend(commandQueue,&c,0);}}
    if(i==2&&event==2){Command c{};c.type=TOGGLE_MUTE;xQueueSend(commandQueue,&c,0);}
  }
  bool alarm=!s.muted&&s.scan.phase!=SCAN_RUNNING&&!s.diagnosticActive&&now>20000&&(!s.rtc||!s.online||!s.storage);
  digitalWrite(PIN_BUZZER,alarm&&now%10000<120?HIGH:LOW);
  if(now-lastPaint>=300){lastPaint=now;drawOLED(s);}
  delay(5); // Both idle tasks must run; Wi-Fi needs scheduler headroom.
}
