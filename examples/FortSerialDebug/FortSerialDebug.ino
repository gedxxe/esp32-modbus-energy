// SPDX-License-Identifier: GPL-3.0-only
// Standalone read-only Fort diagnostic. No external libraries or project headers.
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <atomic>
#include <math.h>

// Defaults only: all diagnostic parameters below can be changed at runtime.
struct DebugOptions {
  int rx=18,tx=17;
  uint32_t responseMs=800,pollMs=2000;
  uint16_t probeAddress=0x0034,energyAddress=0x0082,ratioAddress=0x002C;
  uint8_t probeWords=1;
  double energyScale=0.001;
  bool swappedWords=false,decodeEnergy=true,scanCurrent=false;
} options;
struct Query { uint16_t address; uint8_t words; };
Query auditPlan[8]={{0x0034,1},{0x0035,1},{0x002C,3},{0x0080,1},{0x0082,2},{0x008A,2}};
uint8_t auditCount=6;
// Automatic-direction module: GPIO16 is deliberately unused.
HardwareSerial meter(1);
struct Link { uint32_t baud = 9600; uint8_t id = 1; char parity = 'N'; };
Link current;
uint32_t BAUDS[] = {9600,19200,38400,4800,2400,1200};
char PARITIES[] = {'N','E','O'};
uint8_t firstID = 1, lastID = 247, baudCount=6, parityCount=3;
uint16_t probe = 0;
bool scanning = false, polling = false, abortRequested = false;
uint32_t lastPoll = 0, activeBaud = 0;
char activeParity = 0;
int activeRx=-1,activeTx=-1;
enum ReplyKind : uint8_t { REPLY_NONE, REPLY_DATA, REPLY_EXCEPTION };
ReplyKind lastReply=REPLY_NONE;
// Per-run evidence, not a cable-presence sensor. Snapshots are user-labeled.
struct Evidence {
  uint32_t requests=0,rxBytes=0,passiveBytes=0,staleBytes=0,data=0,exceptions=0,echoes=0;
  uint32_t crcBad=0,shortFrames=0,wrongFrames=0,overflows=0,timeouts=0,txErrors=0;
  uint32_t energyOK=0,energyBad=0;
  bool aborted=false;
} evidence;
struct Capture {
  bool present=false;
  Link link;
  DebugOptions settings;
  Query plan[8];
  uint8_t count=0;
  Evidence result;
} connectedCapture,isolatedCapture;
std::atomic<uint32_t> parityErrors{0}, frameErrors{0}, fifoErrors{0}, bufferErrors{0}, breakErrors{0};
void uartError(hardwareSerial_error_t error) {
  // Driver task callback: counters only, never Serial output from this context.
  if(error==UART_PARITY_ERROR)parityErrors.fetch_add(1,std::memory_order_relaxed);
  if(error==UART_FRAME_ERROR)frameErrors.fetch_add(1,std::memory_order_relaxed);
  if(error==UART_FIFO_OVF_ERROR)fifoErrors.fetch_add(1,std::memory_order_relaxed);
  if(error==UART_BUFFER_FULL_ERROR)bufferErrors.fetch_add(1,std::memory_order_relaxed);
  if(error==UART_BREAK_ERROR)breakErrors.fetch_add(1,std::memory_order_relaxed);
}
#define LOG(...) do { if (Serial) Serial.printf(__VA_ARGS__); } while (0)
void uartStatus() {
  LOG("[UART_EVENTS] parity=%lu framing=%lu fifo_overflow=%lu buffer_full=%lu break=%lu RX_GPIO=%d\n",(unsigned long)parityErrors.load(),(unsigned long)frameErrors.load(),(unsigned long)fifoErrors.load(),(unsigned long)bufferErrors.load(),(unsigned long)breakErrors.load(),digitalRead(options.rx));
}

uint16_t modbusCRC(const uint8_t *bytes, size_t size) {
  uint16_t crc = 0xFFFF;
  while (size--) { crc ^= *bytes++; for (uint8_t bit=0; bit<8; ++bit) crc = (crc>>1)^((crc&1)?0xA001:0); }
  return crc;
}
void hexLog(const char *label, const uint8_t *bytes, size_t size) {
  if (!Serial) return;
  Serial.printf("%s [%u]:", label, unsigned(size));
  for (size_t i=0; i<size; ++i) Serial.printf(" %02X", bytes[i]);
  Serial.println();
}
bool supportedBaud(uint32_t baud) {
  return baud>=1200 && baud<=1000000;
}
void openLink(const Link &link) {
  if (activeBaud == link.baud && activeParity == link.parity && activeRx==options.rx && activeTx==options.tx) return;
  meter.end(); meter.setRxBufferSize(256);
  uint32_t format = link.parity=='E' ? SERIAL_8E1 : link.parity=='O' ? SERIAL_8O1 : SERIAL_8N1;
  meter.begin(link.baud, format, options.rx, options.tx);
  meter.onReceiveError(uartError);
  activeBaud = link.baud; activeParity = link.parity;activeRx=options.rx;activeTx=options.tx;
}
// Keep commands typed/pasted during UART waits; execute them only in loop().
constexpr uint8_t CONSOLE_CAPACITY=16, CONSOLE_LINE=96;
char consoleQueue[CONSOLE_CAPACITY][CONSOLE_LINE]={},consoleLine[CONSOLE_LINE]={};
uint8_t consoleHead=0,consoleTail=0,consoleCount=0,consoleUsed=0;
bool consoleDiscard=false;
void emergencyStop() {
  if(scanning||polling)evidence.aborted=true;
  abortRequested=true;scanning=false;polling=false;
  consoleHead=consoleTail=consoleCount=0;
  LOG("Stopped; queued commands cancelled.\n");
}
void pumpConsole() {
  for(unsigned n=0;n<96&&Serial.available();++n){
    char ch=char(Serial.read());if(ch=='\r')continue;
    // x is an emergency command only at the start of a line, never inside 0xHEX.
    if(!consoleUsed&&!consoleDiscard&&(ch=='x'||ch=='X')){emergencyStop();continue;}
    if(ch=='\n'){
      if(!consoleDiscard&&consoleUsed){
        consoleLine[consoleUsed]=0;
        if(consoleLine[0]!='#'){
          if(!strcmp(consoleLine,"stop"))emergencyStop();
          else if(consoleCount<CONSOLE_CAPACITY){strcpy(consoleQueue[consoleTail],consoleLine);consoleTail=(consoleTail+1)%CONSOLE_CAPACITY;++consoleCount;}
          else LOG("[CONSOLE_FULL] command discarded explicitly; at most 16 queued lines\n");
        }
      }
      consoleUsed=0;consoleDiscard=false;
    }else if(!consoleDiscard){
      if(consoleUsed<CONSOLE_LINE-1)consoleLine[consoleUsed++]=ch;
      else{consoleDiscard=true;LOG("[CONSOLE_LONG] line discarded; maximum 95 characters\n");}
    }
  }
}
void checkAbort(){pumpConsole();}

bool readRegisters(const Link &link, uint16_t address, uint8_t words, uint8_t *out) {
  lastReply=REPLY_NONE;
  if (words < 1 || words > 3 || uint32_t(address)+words>65536UL || abortRequested) return false;
  openLink(link);
  if(!meter){++evidence.txErrors;LOG("[UART_INIT_FAILED] local UART driver unavailable; external module state UNKNOWN\n");return false;}
  uint8_t tx[] = {link.id,3,uint8_t(address>>8),uint8_t(address),0,words,0,0};
  uint16_t crc = modbusCRC(tx,6); tx[6]=uint8_t(crc); tx[7]=uint8_t(crc>>8);
  const uint32_t bits = link.parity=='N' ? 10 : 11;
  const uint32_t gapUs = link.baud>19200 ? 1750 : (3500000UL*bits+link.baud-1)/link.baud;
  uint8_t stale[16];unsigned drained=0;
  for(;drained<256&&meter.available();++drained){int byte=meter.read();if(drained<sizeof(stale))stale[drained]=uint8_t(byte);}
  evidence.staleBytes+=drained;
  if(drained){hexLog("PRE-TX RX prefix (discarded, not a response)",stale,drained<sizeof(stale)?drained:sizeof(stale));LOG("[PRE_TX_RX] discarded=%u; local/unsolicited/stale activity, not caused by this request\n",drained);}
  delay((gapUs+999)/1000+1);
  LOG("\n[t=%lu ms] ID=%u %lu 8%c1 READ03 0x%04X words=%u\n", (unsigned long)millis(), link.id, (unsigned long)link.baud, link.parity, address, words);
  hexLog("TX",tx,sizeof(tx));
  ++evidence.requests;
  if (meter.write(tx,sizeof(tx)) != sizeof(tx)) { ++evidence.txErrors;LOG("[TX_ERROR] UART write incomplete; external module state UNKNOWN\n"); return false; }
  meter.flush();
  // Collect bounded frames until the original deadline. An exact request echo is
  // logged/rejected; a later, independently validated response may still succeed.
  uint8_t rx[64]; size_t size=0; bool overflow=false, sawBytes=false, sawEcho=false;
  uint32_t start=millis(), lastByte=micros(); unsigned logged=0;
  while (uint32_t(millis()-start)<options.responseMs) {
    checkAbort(); if (abortRequested) { evidence.aborted=true;LOG("[ABORTED]\n"); return false; }
    for (unsigned n=0; n<128 && meter.available(); ++n) {
      int byte=meter.read(); sawBytes=true;++evidence.rxBytes;
      if (size<sizeof(rx)) rx[size++]=uint8_t(byte); else overflow=true;
      lastByte=micros();
    }
    if (size && uint32_t(micros()-lastByte)>=gapUs) {
      bool show=logged++<4;
      if (show) hexLog(overflow?"RX prefix (overflow)":"RX",rx,size);
      if (!overflow && size==sizeof(tx) && memcmp(rx,tx,sizeof(tx))==0) {
        sawEcho=true;++evidence.echoes; if (show) LOG("[EXACT_TX_ECHO] rejected; waiting for a separate response\n");
      } else if (overflow) { ++evidence.overflows;if (show) LOG("[RX_OVERFLOW] frame rejected; only first 64 bytes shown\n");
      } else if (size<5) { ++evidence.shortFrames;if (show) LOG("[SHORT_FRAME] rejected\n");
      } else {
        uint16_t calculated=modbusCRC(rx,size-2), received=uint16_t(rx[size-2])|(uint16_t(rx[size-1])<<8);
        if (show) LOG("CRC calculated=0x%04X received=0x%04X %s\n",calculated,received,calculated==received?"OK":"MISMATCH");
        if(calculated!=received)++evidence.crcBad;
        if (calculated==received && rx[0]==link.id && rx[1]==0x83 && size==5) {
          ++evidence.exceptions;lastReply=REPLY_EXCEPTION; LOG("[COMM_VALID_EXCEPTION] code=0x%02X; matching slave/CRC/function; requested register was not read\n",rx[2]); return false;
        }
        if (calculated==received && rx[0]==link.id && rx[1]==3 && size==size_t(5+2*words) && rx[2]==2*words) {
          ++evidence.data;memcpy(out,rx+3,words*2); lastReply=REPLY_DATA; LOG("[VALID_RESPONSE]\n"); return true;
        }
        if(calculated==received){++evidence.wrongFrames;if(show)LOG("[WRONG_FRAME] ID/function/byte-count/length mismatch\n");}
      }
      size=0; overflow=false;
    }
    delay(1); // Allow USB, scheduler and watchdog/idle processing.
  }
  if (size) hexLog("RX tail at deadline (not accepted)",rx,size);
  if(!sawBytes)++evidence.timeouts;
  LOG("[%s] %s%s\n",sawBytes?"NO_VALID_RESPONSE":"TIMEOUT",sawBytes?"RX activity, no accepted response":"no RX bytes",sawEcho?"; exact TX echo observed":"");
  return false;
}
uint32_t bigEndian32(const uint8_t *p) {
  return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}
bool readEnergy(const Link &link) {
  if(!options.decodeEnergy){LOG("[DECODE_DISABLED] use reg/deeper for raw values; no kWh computed\n");return false;}
  uint8_t energy[4], ratios[6];
  if (!readRegisters(link,options.energyAddress,2,energy) || !readRegisters(link,options.ratioAddress,3,ratios)){++evidence.energyBad;return false;}
  uint16_t ptNum=(uint16_t(ratios[0])<<8)|ratios[1];
  uint16_t ptDen=(uint16_t(ratios[2])<<8)|ratios[3];
  uint16_t ct=(uint16_t(ratios[4])<<8)|ratios[5];
  if (!ptNum || !ptDen || !ct || ptNum>9999 || ptDen>9999 || ct>9999) { ++evidence.energyBad;LOG("[RATIO_REJECTED] PT=%u/%u CT=%u\n",ptNum,ptDen,ct); return false; }
  uint8_t ordered[4];memcpy(ordered,energy,4);
  if(options.swappedWords){ordered[0]=energy[2];ordered[1]=energy[3];ordered[2]=energy[0];ordered[3]=energy[1];}
  uint32_t raw=bigEndian32(ordered); double secondary=double(raw)*options.energyScale;
  LOG("[DECODE_SETTINGS] energy=0x%04X ratios=0x%04X scale=%.12g order=%s; configured interpretation, compare with meter display\n",options.energyAddress,options.ratioAddress,options.energyScale,options.swappedWords?"swap":"normal");
  double multiplier=double(ptNum)/ptDen*ct;
  LOG("[ENERGY] raw=%lu secondary=%.3f kWh PT=%u/%u CT=%u multiplier=%.6f\n",(unsigned long)raw,secondary,ptNum,ptDen,ct,multiplier);
  ++evidence.energyOK;LOG("[PRIMARY_IMPORT] %.6f kWh\n",secondary*multiplier);
  return true;
}
// Level 1: UART receive activity only. A slave may be completely silent without requests.
void listenOnly(uint32_t seconds) {
  polling=false; abortRequested=false; openLink(current);
  LOG("[BASIC] passive RX for %lu seconds; NO Modbus request transmitted. Current format only.\n",(unsigned long)seconds);
  uartStatus();
  uint8_t bytes[64]; uint32_t total=0, start=millis(); unsigned chunks=0;
  while(uint32_t(millis()-start)<seconds*1000UL && !abortRequested) {
    checkAbort(); size_t size=0;
    for(unsigned i=0;i<64 && meter.available();++i)bytes[size++]=uint8_t(meter.read());
    total+=size;
    if(size && chunks++<8)hexLog("PASSIVE RX chunk",bytes,size);
    delay(1);
  }
  LOG("[BASIC_END] bytes=%lu; no TX sent. No RX does NOT prove no RS485 connection. Chunks are not validated frames.\n",(unsigned long)total);
  evidence.passiveBytes+=total;if(abortRequested)evidence.aborted=true;
  uartStatus();
}
// Level 2 does not require a compatible energy map or valid CT/PT values.
bool probeLink(const Link &link) {
  uint8_t data[6]; bool dataOK=readRegisters(link,options.probeAddress,options.probeWords,data);
  bool replied=dataOK || lastReply==REPLY_EXCEPTION;
  if(dataOK)LOG("[COMM_FOUND] validated FC03 response; register 0x%04X first-word=0x%04X\n",options.probeAddress,(uint16_t(data[0])<<8)|data[1]);
  else if(replied)LOG("[COMM_FOUND] valid exception response; register/profile support still unknown\n");
  else LOG("[COMM_NOT_CONFIRMED] no matching response; inspect RX/echo/UART errors\n");
  return replied;
}
void dumpRegisters(const Link &link,uint16_t address,uint8_t words) {
  uint8_t data[6];
  if(readRegisters(link,address,words,data))for(uint8_t i=0;i<words;++i) {
    uint16_t value=(uint16_t(data[i*2])<<8)|data[i*2+1];
    LOG("[REGISTER] 0x%04X = 0x%04X (%u unsigned); raw word only\n",address+i,value,value);
  }
}
// Level 3: documented read-only map, independent reads, then kWh conversion.
void deepCheck(const Link &link) {
  LOG("[DEEPER] %u configured raw queries; energy decode %s\n",auditCount,options.decodeEnergy?"ON":"OFF");
  for(unsigned i=0;i<auditCount&&!abortRequested;++i)dumpRegisters(link,auditPlan[i].address,auditPlan[i].words);
  if(options.decodeEnergy&&!abortRequested&&!readEnergy(link))LOG("[PROFILE_NOT_CONFIRMED] energy/scaling not accepted; earlier valid protocol reply remains valid\n");
  uartStatus();
}

// Report observations first, then the next bounded experiment. Never equate
// silence with an absent/dead module: the four-pin board has no presence input.
void reportEvidence() {
  LOG("\n[REPORT_BEGIN] counts for last active test/scan/poll (stats clear resets); not a hardware presence detector\n");
  LOG("[EVIDENCE] requests=%lu rx_bytes=%lu passive_bytes=%lu pre_tx_bytes=%lu valid_data=%lu valid_exception=%lu echo=%lu crc_bad=%lu short=%lu wrong=%lu overflow=%lu timeout=%lu local_uart_error=%lu energy_ok=%lu energy_failed=%lu aborted=%u\n",
      (unsigned long)evidence.requests,(unsigned long)evidence.rxBytes,(unsigned long)evidence.passiveBytes,(unsigned long)evidence.staleBytes,
      (unsigned long)evidence.data,(unsigned long)evidence.exceptions,(unsigned long)evidence.echoes,
      (unsigned long)evidence.crcBad,(unsigned long)evidence.shortFrames,(unsigned long)evidence.wrongFrames,
      (unsigned long)evidence.overflows,(unsigned long)evidence.timeouts,(unsigned long)evidence.txErrors,
      (unsigned long)evidence.energyOK,(unsigned long)evidence.energyBad,evidence.aborted);
  if(evidence.aborted)LOG("[INCOMPLETE_TEST] aborted; repeat the same test before comparing\n");
  if(evidence.data)LOG("[TRANSPORT_DATA_CONFIRMED] matching FC03/ID/length/CRC data; meter identity and energy scale still require comparison with its display\n");
  else if(evidence.exceptions)LOG("[TRANSPORT_EXCEPTION_ONLY] matching CRC-valid exception; communication replied but queried registers were not read\n");
  else if(evidence.txErrors)LOG("[LOCAL_UART_ERROR] fix local UART initialization/write first; cannot diagnose external module health\n");
  else if(evidence.echoes)LOG("[ECHO_WITHOUT_REPLY] exact requests returned, no accepted slave reply; compare connected/isolated captures\n");
  else if(evidence.rxBytes||evidence.passiveBytes||evidence.staleBytes)LOG("[RX_UNVALIDATED] activity exists but no accepted reply; inspect hex, UART events and serial settings\n");
  else if(evidence.requests)LOG("[NO_RX_UNRESOLVED] no reply. Module absent/unpowered/failed, open RX, silent meter, wrong serial/ID and unsupported silent registers remain indistinguishable\n");
  else LOG("[NO_REQUEST_EVIDENCE] no active request recorded; passive silence is normal for an unpolled slave\n");
  if(evidence.energyOK)LOG("[PROFILE_DECODED_UNVERIFIED] configured integer/scale/CTPT decoded; confirm against Fort import-energy display before acceptance\n");
  else LOG("[ENERGY_NOT_VERIFIED] do not interpret missing or rejected energy as zero\n");
  if(!evidence.data&&!evidence.exceptions)LOG("[NEXT_SERIAL_INPUT] status; rxcheck (optional TTL input test); use a short scan with meter settings or check connected/check isolated comparison. These are Serial Monitor commands.\n");
  uartStatus();LOG("[REPORT_END] Copy OUTPUT from REPORT_BEGIN plus preceding TX/RX/settings when requesting support. Do not type log labels as commands.\n");
}
void diagnoseRun() {
  evidence=Evidence{};
  LOG("[DIAGNOSE_BEGIN] current ID/format only, not a full scan. Tests selected probe AND configured plan even if the first address is silent.\n");
  listenOnly(3);
  if(!abortRequested)probeLink(current);
  if(!abortRequested)deepCheck(current);
  if(abortRequested)evidence.aborted=true;
  reportEvidence();
}
// Optional weak-pull test of the TTL RX INPUT only. Does not drive RX HIGH/LOW,
// send bytes, measure module VCC, or measure the differential A/B bus.
void rxCheck() {
  polling=false;abortRequested=false;meter.end();activeBaud=0;
  unsigned highs[2]={};bool cancelled=false;
  for(unsigned phase=0;phase<2&&!cancelled;++phase){
    pinMode(options.rx,phase==0?INPUT_PULLUP:INPUT_PULLDOWN);delay(2);
    for(unsigned i=0;i<32;++i){checkAbort();if(abortRequested){cancelled=true;break;}highs[phase]+=digitalRead(options.rx)?1:0;delay(1);}
  }
  pinMode(options.rx,INPUT);openLink(current);
  if(cancelled){LOG("[RX_CHECK_ABORTED] UART restored\n");return;}
  LOG("[RX_BIAS] GPIO%d pullup_high=%u/32 pulldown_high=%u/32; UART restored; no Modbus TX\n",options.rx,highs[0],highs[1]);
  if(highs[0]==32&&highs[1]==0)LOG("[RX_WEAK_BIAS_FOLLOWING] weak pulls determine logic level: high impedance is possible (open lead, unplugged, unpowered or disabled receiver); NOT proof of which cause\n");
  else if(highs[0]==32&&highs[1]==32)LOG("[RX_HIGH_HELD] HIGH resists weak pulldown: consistent with driven idle or external bias; NOT proof module is healthy/present\n");
  else if(highs[0]==0&&highs[1]==0)LOG("[RX_LOW_HELD] LOW resists weak pullup: investigate held-low/break/short/driver state; NOT proof module is damaged\n");
  else LOG("[RX_VARIABLE] sampled TTL level changes or weak bias competes; use UART hex/events or an instrument; no automatic fault verdict\n");
}
bool sameCaptureSettings(const Capture &a,const Capture &b){
  const DebugOptions &x=a.settings,&y=b.settings;
  if(a.link.id!=b.link.id||a.link.baud!=b.link.baud||a.link.parity!=b.link.parity||a.count!=b.count)return false;
  if(x.rx!=y.rx||x.tx!=y.tx||x.responseMs!=y.responseMs||x.probeAddress!=y.probeAddress||x.probeWords!=y.probeWords||x.energyAddress!=y.energyAddress||x.ratioAddress!=y.ratioAddress||x.energyScale!=y.energyScale||x.swappedWords!=y.swappedWords||x.decodeEnergy!=y.decodeEnergy)return false;
  for(uint8_t i=0;i<a.count;++i)if(a.plan[i].address!=b.plan[i].address||a.plan[i].words!=b.plan[i].words)return false;
  return true;
}
void compareCaptures(){
  if(!connectedCapture.present||!isolatedCapture.present){LOG("[COMPARE_NEEDS_TWO] run check connected with A/B attached, then check isolated with ONLY A/B detached at module. Labels are user assertions, not detected states.\n");return;}
  if(!sameCaptureSettings(connectedCapture,isolatedCapture)){LOG("[COMPARE_CONFIG_CHANGED] repeat both tests with identical ID/format/pins/probe/plan/profile/timeout\n");return;}
  const Evidence &a=connectedCapture.result,&b=isolatedCapture.result;
  if(a.txErrors||b.txErrors){LOG("[COMPARE_LOCAL_UART_ERROR] local UART failed in a capture; resolve before assessing the physical path\n");return;}
  if(a.aborted||b.aborted){LOG("[COMPARE_INCOMPLETE] an aborted capture cannot support comparison; repeat it\n");return;}
  LOG("[COMPARE] user-labeled A/B connected: rx=%lu echo=%lu data=%lu exception=%lu; A/B isolated: rx=%lu echo=%lu data=%lu exception=%lu\n",(unsigned long)a.rxBytes,(unsigned long)a.echoes,(unsigned long)a.data,(unsigned long)a.exceptions,(unsigned long)b.rxBytes,(unsigned long)b.echoes,(unsigned long)b.data,(unsigned long)b.exceptions);
  if(b.data||b.exceptions)LOG("[ISOLATION_CONTRADICTED] matching responses recorded in user-labeled isolated state; verify test state or other signal path before attributing replies to Fort\n");
  else if(a.data||a.exceptions)LOG("[REPLY_REQUIRES_CONNECTED_PATH] validated reply seen only with A/B connected in these tests; energy/map acceptance remains separate\n");
  else if(b.rxBytes||b.passiveBytes||b.staleBytes)LOG("[LOCAL_ACTIVITY_REPRODUCED] RX also exists with user-labeled A/B isolation; those isolated bytes are not evidence of Fort replies. Module/TTL/ESP path or local noise still need localization\n");
  else if(a.rxBytes||a.passiveBytes||a.staleBytes)LOG("[CONNECTED_ACTIVITY_ONLY] bytes seen only connected, but none valid; compare serial settings and waveform; meter data not established\n");
  else LOG("[BOTH_SILENT_UNRESOLVED] no RX in either test. Cannot distinguish missing/dead module from other silent paths using software alone; measure module supply/TTL TX/RX or substitute one known-good link\n");
}
void captureCheck(bool connected){
  LOG("[USER_LABELED_TEST] %s; firmware cannot detect whether you physically performed this connection state\n",connected?"A/B connected":"A/B detached at module; TTL and power retained");
  Capture &capture=connected?connectedCapture:isolatedCapture;
  capture.present=false;capture.link=current;capture.settings=options;capture.count=auditCount;memcpy(capture.plan,auditPlan,sizeof(auditPlan));
  diagnoseRun();capture.result=evidence;capture.present=true;compareCaptures();
}

uint16_t scanTotal(){return uint16_t(lastID-firstID+1)*(options.scanCurrent?1:baudCount*parityCount);}
bool validQuery(unsigned address,unsigned words){return address<=65535&&words>=1&&words<=3&&address+words<=65536UL;}
bool allowedPin(unsigned pin){return (pin>=10&&pin<=18)||pin==21||(pin>=39&&pin<=42)||pin==47;}
void showPlan(){for(uint8_t i=0;i<auditCount;++i)LOG("[PLAN] %u address=0x%04X words=%u\n",i+1,auditPlan[i].address,auditPlan[i].words);}
void showStatus(){
  LOG("[SETTINGS] ID=%u baud=%lu 8%c1 RX=%d TX=%d timeout=%lu poll=%lu probe=0x%04X/%u range=%u..%u mode=%s candidates=%u\n",current.id,(unsigned long)current.baud,current.parity,options.rx,options.tx,(unsigned long)options.responseMs,(unsigned long)options.pollMs,options.probeAddress,options.probeWords,firstID,lastID,options.scanCurrent?"current":"matrix",scanTotal());
  LOG("[PROFILE] energy=0x%04X ratios=0x%04X scale=%.12g order=%s decode=%s; RAM only\n",options.energyAddress,options.ratioAddress,options.energyScale,options.swappedWords?"swap":"normal",options.decodeEnergy?"on":"off");
  LOG("[SCAN_BAUDS]");for(uint8_t i=0;i<baudCount;++i)LOG(" %lu",(unsigned long)BAUDS[i]);
  LOG("\n[SCAN_PARITIES]");for(uint8_t i=0;i<parityCount;++i)LOG(" %c",PARITIES[i]);LOG("\n");
  showPlan();uartStatus();
}
void help(){
  LOG("\nFortSerialDebug 1.3.0 | Serial 115200 + Newline | FC03 ONLY | no register/NVS/filesystem writes\n");
  LOG("INPUT: type ONE command in Arduino IDE Serial Monitor INPUT, press Enter/Send, wait for OUTPUT.\n");
  LOG("START: type status, press Enter; type diagnose, press Enter; copy resulting OUTPUT for support.\n");
  LOG("EVIDENCE: report | rxcheck | check connected | check isolated | compare (connection labels are user supplied)\n");
  LOG("LAYERS: basic | listen 1..30 | intermediate | deeper | diagnose | read | poll | status | help\n");
  LOG("RUNTIME: id 1 | baud 9600 | parity N | pins 18 17 (RX TX) | timeout 800 | interval 2000\n");
  LOG("PROBES: probe 0034 1 | reg 0082 2 (HEX PDU address, 1..3 words; no offset added)\n");
  LOG("SCAN: range 1 16 | scanmode current or matrix | bauds 9600 19200 | parities NEO | scan\n");
  LOG("AUDIT: stats clear | plan clear | plan add 0080 1 | plan show (8-query limit)\n");
  LOG("DECODE: energy 0082 | ratios 002C | scale 0.001 | order normal or swap | decode on or off\n");
  LOG("x aborts and clears queued work, even in a UART wait. Up to 16 lines queue; # starts a comment.\n");
  LOG("Default RX18/TX17; auto-direction module, GPIO16 unused. RX silence alone proves nothing.\n");
}
void command(const char *line) {
  unsigned a=0,b=0; unsigned long baud=0; char parity=0, extra=0;double scalar=0;char token[16]={};
  if (!strcmp(line,"help") || !strcmp(line,"h")) { help(); return; }
  if (!strcmp(line,"x") || !strcmp(line,"stop")) { abortRequested=true; scanning=false; polling=false; LOG("Stopped.\n"); return; }
  if (!strcmp(line,"status")) { showStatus(); return; }
  if (!strcmp(line,"report")) { reportEvidence(); return; }
  if (scanning) { LOG("Scan active; use x to abort.\n"); return; }
  if(!strcmp(line,"rxcheck")){rxCheck();return;}
  if(!strcmp(line,"check connected")){captureCheck(true);return;}
  if(!strcmp(line,"check isolated")){captureCheck(false);return;}
  if(!strcmp(line,"compare")){compareCaptures();return;}
  if(sscanf(line,"pins %u %u %c",&a,&b,&extra)==2&&a!=b&&allowedPin(a)&&allowedPin(b)){polling=false;options.rx=a;options.tx=b;openLink(current);showStatus();return;}
  if(sscanf(line,"timeout %lu %c",&baud,&extra)==1&&baud>=50&&baud<=5000){polling=false;options.responseMs=baud;showStatus();return;}
  if(sscanf(line,"interval %lu %c",&baud,&extra)==1&&baud>=100&&baud<=60000){polling=false;options.pollMs=baud;showStatus();return;}
  if(sscanf(line,"probe %x %u %c",&a,&b,&extra)==2&&validQuery(a,b)){polling=false;options.probeAddress=a;options.probeWords=b;showStatus();return;}
  if(sscanf(line,"energy %x %c",&a,&extra)==1&&validQuery(a,2)){polling=false;options.energyAddress=a;showStatus();return;}
  if(sscanf(line,"ratios %x %c",&a,&extra)==1&&validQuery(a,3)){polling=false;options.ratioAddress=a;showStatus();return;}
  if(sscanf(line,"scale %lf %c",&scalar,&extra)==1&&isfinite(scalar)&&scalar>=1e-12&&scalar<=1e6){polling=false;options.energyScale=scalar;showStatus();return;}
  if(sscanf(line,"order %15s %c",token,&extra)==1&&(!strcmp(token,"normal")||!strcmp(token,"swap"))){polling=false;options.swappedWords=!strcmp(token,"swap");showStatus();return;}
  if(sscanf(line,"decode %15s %c",token,&extra)==1&&(!strcmp(token,"on")||!strcmp(token,"off"))){polling=false;options.decodeEnergy=!strcmp(token,"on");showStatus();return;}
  if(sscanf(line,"scanmode %15s %c",token,&extra)==1&&(!strcmp(token,"current")||!strcmp(token,"matrix"))){polling=false;options.scanCurrent=!strcmp(token,"current");showStatus();return;}
  if(!strcmp(line,"plan clear")){polling=false;auditCount=0;LOG("[PLAN] cleared; add raw queries before deeper\n");return;}
  if(!strcmp(line,"plan show")){showPlan();return;}
  if(sscanf(line,"plan add %x %u %c",&a,&b,&extra)==2&&validQuery(a,b)&&auditCount<8){polling=false;auditPlan[auditCount++]={uint16_t(a),uint8_t(b)};showPlan();return;}
  if(!strncmp(line,"bauds ",6)){
    uint32_t rates[6]={};uint8_t count=0;const char *cursor=line+6;bool ok=true;
    while(*cursor&&ok){while(*cursor==' ')++cursor;if(!*cursor)break;unsigned long value=0;int used=0;
      if(sscanf(cursor,"%lu%n",&value,&used)!=1||!supportedBaud(value)||count==6){ok=false;break;}
      for(uint8_t i=0;i<count;++i)if(rates[i]==value)ok=false;
      if(!ok)break;
      rates[count++]=value;cursor+=used;if(*cursor&&*cursor!=' ')ok=false;
    }
    if(ok&&count){polling=false;memcpy(BAUDS,rates,sizeof(rates));baudCount=count;showStatus();return;}
  }
  if(sscanf(line,"parities %15s %c",token,&extra)==1){
    uint8_t count=strlen(token);bool ok=count>=1&&count<=3;char modes[3]={};
    for(uint8_t i=0;i<count&&ok;++i){modes[i]=char(toupper(uint8_t(token[i])));if(modes[i]!='N'&&modes[i]!='E'&&modes[i]!='O')ok=false;for(uint8_t j=0;j<i;++j)if(modes[j]==modes[i])ok=false;}
    if(ok){polling=false;memcpy(PARITIES,modes,3);parityCount=count;showStatus();return;}
  }
  if(!strcmp(line,"stats clear")){evidence=Evidence{};parityErrors.store(0);frameErrors.store(0);fifoErrors.store(0);bufferErrors.store(0);breakErrors.store(0);uartStatus();return;}
  if (!strcmp(line,"basic")) { evidence=Evidence{};listenOnly(5); return; }
  if (sscanf(line,"listen %u %c",&a,&extra)==1 && a>=1 && a<=30) { evidence=Evidence{};listenOnly(a); return; }
  if (!strcmp(line,"intermediate")) { evidence=Evidence{};abortRequested=false; polling=false; probeLink(current); uartStatus(); return; }
  if (!strcmp(line,"deeper")) { evidence=Evidence{};abortRequested=false; polling=false; deepCheck(current); return; }
  if (!strcmp(line,"diagnose")) { diagnoseRun(); return; }
  if (sscanf(line,"reg %x %u %c",&a,&b,&extra)==2 && validQuery(a,b)) { evidence=Evidence{};abortRequested=false; polling=false; dumpRegisters(current,uint16_t(a),uint8_t(b)); uartStatus(); return; }
  if (!strcmp(line,"scan")) {
    evidence=Evidence{};probe=0; scanning=true; polling=false; abortRequested=false;
    showStatus();LOG("Communication scan starts: %u candidates; valid exception counts as a reply. Type x to abort.\n",scanTotal()); return;
  }
  if (!strcmp(line,"read")) { evidence=Evidence{};abortRequested=false; polling=false; readEnergy(current); return; }
  if (!strcmp(line,"poll")) { evidence=Evidence{};abortRequested=false; polling=!polling; lastPoll=millis()-options.pollMs; LOG("Polling %s\n",polling?"ON":"OFF"); return; }
  if (sscanf(line,"id %u %c",&a,&extra)==1 && a>=1 && a<=247) { polling=false;current.id=a;showStatus();return; }
  if (sscanf(line,"baud %lu %c",&baud,&extra)==1 && supportedBaud(baud)) { polling=false;current.baud=baud;showStatus();return; }
  if (sscanf(line,"parity %c %c",&parity,&extra)==1) {
    parity=char(toupper(uint8_t(parity))); if (parity=='N'||parity=='E'||parity=='O') { polling=false;current.parity=parity;showStatus();return; }
  }
  if (sscanf(line,"range %u %u %c",&a,&b,&extra)==2 && a>=1 && b<=247 && a<=b) { polling=false;firstID=a;lastID=b;showStatus();return; }
  LOG("Unknown/invalid command. Type help.\n");
}
void serialInput(){
  pumpConsole();
  if(scanning||!consoleCount)return;
  char line[CONSOLE_LINE];strcpy(line,consoleQueue[consoleHead]);consoleHead=(consoleHead+1)%CONSOLE_CAPACITY;--consoleCount;
  LOG("[SERIAL_INPUT] %s\n",line);command(line);
}
void setup() {
  Serial.begin(115200); // Never wait for USB Serial to connect.
  help();
}
void loop() {
  static bool serialWasOpen=false;
  bool connected=bool(Serial); if (connected&&!serialWasOpen) help(); serialWasOpen=connected;
  serialInput();
  if (scanning) {
    checkAbort();
    if (abortRequested) { scanning=false; LOG("Scan aborted. Previous reader settings retained.\n"); }
    else {
      uint16_t count=lastID-firstID+1, mode=probe/count;
      Link candidate; candidate.id=firstID+probe%count; candidate.baud=options.scanCurrent?current.baud:BAUDS[mode/parityCount]; candidate.parity=options.scanCurrent?current.parity:PARITIES[mode%parityCount];
      LOG("\nSCAN %u/%u\n",probe+1,scanTotal());
      bool found=probeLink(candidate); ++probe;
      if (abortRequested) { scanning=false; LOG("Scan aborted. Previous reader settings retained.\n"); }
      else if (found) { current=candidate; scanning=false; LOG("[COMM_FOUND] ID=%u %lu 8%c1; reader settings selected in RAM. Use deeper to check Fort registers before poll.\n",current.id,(unsigned long)current.baud,current.parity); }
      else if (probe>=scanTotal()) { scanning=false; LOG("[SCAN_FINISHED] no matching Modbus reply found. Settings retained. UART RX/errors/echo and register support are separate observations.\n"); }
    }
  } else if (polling && uint32_t(millis()-lastPoll)>=options.pollMs) {
    lastPoll=millis(); readEnergy(current); if (abortRequested) polling=false;
  }
  delay(5);
}
