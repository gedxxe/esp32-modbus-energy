// SPDX-License-Identifier: GPL-3.0-only
// Standalone read-only Fort diagnostic. No external libraries or project headers.
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <atomic>

constexpr int MODBUS_RX = 18, MODBUS_TX = 17;
constexpr uint32_t RESPONSE_MS = 800, POLL_MS = 2000;
constexpr uint16_t ENERGY_REGISTER = 0x0082, RATIO_REGISTER = 0x002C;
// Automatic-direction module: GPIO16 is deliberately unused.
HardwareSerial meter(1);
struct Link { uint32_t baud = 9600; uint8_t id = 1; char parity = 'N'; };
Link current;
const uint32_t BAUDS[] = {9600,19200,38400,4800,2400,1200};
const char PARITIES[] = {'N','E','O'};
uint8_t firstID = 1, lastID = 247;
uint16_t probe = 0;
bool scanning = false, polling = false, abortRequested = false;
uint32_t lastPoll = 0, activeBaud = 0;
char activeParity = 0;
enum ReplyKind : uint8_t { REPLY_NONE, REPLY_DATA, REPLY_EXCEPTION };
ReplyKind lastReply=REPLY_NONE;
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
  LOG("[UART_EVENTS] parity=%lu framing=%lu fifo_overflow=%lu buffer_full=%lu break=%lu RX_GPIO=%d\n",(unsigned long)parityErrors.load(),(unsigned long)frameErrors.load(),(unsigned long)fifoErrors.load(),(unsigned long)bufferErrors.load(),(unsigned long)breakErrors.load(),digitalRead(MODBUS_RX));
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
  for (uint32_t value : BAUDS) if (value == baud) return true;
  return false;
}
void openLink(const Link &link) {
  if (activeBaud == link.baud && activeParity == link.parity) return;
  meter.end(); meter.setRxBufferSize(256);
  uint32_t format = link.parity=='E' ? SERIAL_8E1 : link.parity=='O' ? SERIAL_8O1 : SERIAL_8N1;
  meter.begin(link.baud, format, MODBUS_RX, MODBUS_TX);
  meter.onReceiveError(uartError);
  activeBaud = link.baud; activeParity = link.parity;
}
// While a transaction/scan is busy, x is the immediate abort command.
void checkAbort() {
  for (uint8_t n=0; n<32 && Serial.available(); ++n) {
    char ch = char(Serial.read()); if (ch=='x' || ch=='X') abortRequested = true;
  }
}

bool readRegisters(const Link &link, uint16_t address, uint8_t words, uint8_t *out) {
  lastReply=REPLY_NONE;
  if (words < 1 || words > 3 || uint32_t(address)+words>65536UL || abortRequested) return false;
  openLink(link);
  uint8_t tx[] = {link.id,3,uint8_t(address>>8),uint8_t(address),0,words,0,0};
  uint16_t crc = modbusCRC(tx,6); tx[6]=uint8_t(crc); tx[7]=uint8_t(crc>>8);
  const uint32_t bits = link.parity=='N' ? 10 : 11;
  const uint32_t gapUs = link.baud>19200 ? 1750 : (3500000UL*bits+link.baud-1)/link.baud;
  for (unsigned n=0; n<256 && meter.available(); ++n) meter.read();
  delay((gapUs+999)/1000+1);
  LOG("\n[t=%lu ms] ID=%u %lu 8%c1 READ03 0x%04X words=%u\n", (unsigned long)millis(), link.id, (unsigned long)link.baud, link.parity, address, words);
  hexLog("TX",tx,sizeof(tx));
  if (meter.write(tx,sizeof(tx)) != sizeof(tx)) { LOG("[TX_ERROR] UART write incomplete\n"); return false; }
  meter.flush();
  // Collect bounded frames until the original deadline. An exact request echo is
  // logged/rejected; a later, independently validated response may still succeed.
  uint8_t rx[64]; size_t size=0; bool overflow=false, sawBytes=false, sawEcho=false;
  uint32_t start=millis(), lastByte=micros(); unsigned logged=0;
  while (uint32_t(millis()-start)<RESPONSE_MS) {
    checkAbort(); if (abortRequested) { LOG("[ABORTED]\n"); return false; }
    for (unsigned n=0; n<128 && meter.available(); ++n) {
      int byte=meter.read(); sawBytes=true;
      if (size<sizeof(rx)) rx[size++]=uint8_t(byte); else overflow=true;
      lastByte=micros();
    }
    if (size && uint32_t(micros()-lastByte)>=gapUs) {
      bool show=logged++<4;
      if (show) hexLog(overflow?"RX prefix (overflow)":"RX",rx,size);
      if (!overflow && size==sizeof(tx) && memcmp(rx,tx,sizeof(tx))==0) {
        sawEcho=true; if (show) LOG("[EXACT_TX_ECHO] rejected; waiting for a separate response\n");
      } else if (overflow) { if (show) LOG("[RX_OVERFLOW] frame rejected; only first 64 bytes shown\n");
      } else if (size<5) { if (show) LOG("[SHORT_FRAME] rejected\n");
      } else {
        uint16_t calculated=modbusCRC(rx,size-2), received=uint16_t(rx[size-2])|(uint16_t(rx[size-1])<<8);
        if (show) LOG("CRC calculated=0x%04X received=0x%04X %s\n",calculated,received,calculated==received?"OK":"MISMATCH");
        if (calculated==received && rx[0]==link.id && rx[1]==0x83 && size==5) {
          lastReply=REPLY_EXCEPTION; LOG("[COMM_VALID_EXCEPTION] code=0x%02X; matching slave/CRC/function; requested register was not read\n",rx[2]); return false;
        }
        if (calculated==received && rx[0]==link.id && rx[1]==3 && size==size_t(5+2*words) && rx[2]==2*words) {
          memcpy(out,rx+3,words*2); lastReply=REPLY_DATA; LOG("[VALID_RESPONSE]\n"); return true;
        }
        if (show && calculated==received) LOG("[WRONG_FRAME] ID/function/byte-count/length mismatch\n");
      }
      size=0; overflow=false;
    }
    delay(1); // Allow USB, scheduler and watchdog/idle processing.
  }
  if (size) hexLog("RX tail at deadline (not accepted)",rx,size);
  LOG("[%s] %s%s\n",sawBytes?"NO_VALID_RESPONSE":"TIMEOUT",sawBytes?"RX activity, no accepted response":"no RX bytes",sawEcho?"; exact TX echo observed":"");
  return false;
}
uint32_t bigEndian32(const uint8_t *p) {
  return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}
bool readEnergy(const Link &link) {
  uint8_t energy[4], ratios[6];
  if (!readRegisters(link,ENERGY_REGISTER,2,energy) || !readRegisters(link,RATIO_REGISTER,3,ratios)) return false;
  uint16_t ptNum=(uint16_t(ratios[0])<<8)|ratios[1];
  uint16_t ptDen=(uint16_t(ratios[2])<<8)|ratios[3];
  uint16_t ct=(uint16_t(ratios[4])<<8)|ratios[5];
  if (!ptNum || !ptDen || !ct || ptNum>9999 || ptDen>9999 || ct>9999) { LOG("[RATIO_REJECTED] PT=%u/%u CT=%u\n",ptNum,ptDen,ct); return false; }
  uint32_t raw=bigEndian32(energy); double secondary=double(raw)*0.001;
  double multiplier=double(ptNum)/ptDen*ct;
  LOG("[ENERGY] raw=%lu secondary=%.3f kWh PT=%u/%u CT=%u multiplier=%.6f\n",(unsigned long)raw,secondary,ptNum,ptDen,ct,multiplier);
  LOG("[PRIMARY_IMPORT] %.6f kWh\n",secondary*multiplier);
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
  uartStatus();
}
// Level 2 does not require a compatible energy map or valid CT/PT values.
bool probeLink(const Link &link) {
  uint8_t data[2]; bool dataOK=readRegisters(link,0x0034,1,data);
  bool replied=dataOK || lastReply==REPLY_EXCEPTION;
  if(dataOK)LOG("[COMM_FOUND] validated FC03 response; register 0x0034 raw=0x%04X\n",(uint16_t(data[0])<<8)|data[1]);
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
  const uint16_t addresses[]={0x0034,0x0035,0x002C,0x0080,0x0082,0x008A};
  const uint8_t words[]={1,1,3,1,2,2};
  LOG("[DEEPER] communication settings, CT/PT, frequency, integer energy, primary-float raw words\n");
  for(unsigned i=0;i<6 && !abortRequested;++i)dumpRegisters(link,addresses[i],words[i]);
  if(!abortRequested && !readEnergy(link))LOG("[PROFILE_NOT_CONFIRMED] energy/scaling not accepted; this does not negate an earlier valid Modbus reply\n");
  uartStatus();
}

void help() {
  LOG("\nFortSerialDebug 1.1.0 | ESP32-S3 | SERIAL MONITOR 115200, Newline\n");
  LOG("Only Modbus function 03. No register writes, NVS, filesystem, Wi-Fi or external libraries.\n");
  LOG("UART1 TX=GPIO17 -> module TTL input; RX=GPIO18 <- module TTL output.\n");
  LOG("Automatic-direction module: GPIO16 unused. Follow verified input/output directions.\n");
  LOG("Levels:\n  basic            passive RX for 5 seconds, sends no request\n  intermediate     FC03 probe; valid exception also proves a Modbus reply\n  deeper           documented register audit + kWh/CT/PT\n  diagnose         basic 3 seconds, intermediate, then deeper if responsive\n  listen 10        passive RX 1..30 seconds at current UART format\n  reg 0082 2       bounded FC03 raw read; address is HEX, count 1..3\nCommands:\n  read             read kWh + CT/PT once\n  poll             toggle repeated reads, target 2 seconds\n  id 1             select reader slave ID (1..247)\n  baud 9600        1200/2400/4800/9600/19200/38400\n  parity N         N, E or O; 8 data bits, 1 stop bit\n  range 1 247      inclusive scan range; use range 1 1 for wiring diagnosis\n  scan             find communication across range x 6 bauds x 3 parities; includes valid exceptions\n  x                abort scan/poll, including during a response wait\n  status           reader settings\n  help             show this help\n");
  LOG("Full scan: 4446 candidates, about an hour or more with no replies. Start with range 1 1.\n");
}
void command(const char *line) {
  unsigned a=0,b=0; unsigned long baud=0; char parity=0, extra=0;
  if (!strcmp(line,"help") || !strcmp(line,"h")) { help(); return; }
  if (!strcmp(line,"x") || !strcmp(line,"stop")) { abortRequested=true; scanning=false; polling=false; LOG("Stopped.\n"); return; }
  if (!strcmp(line,"status")) { LOG("Reader: ID=%u baud=%lu 8%c1 range=%u..%u\n",current.id,(unsigned long)current.baud,current.parity,firstID,lastID); uartStatus(); return; }
  if (scanning) { LOG("Scan active; use x to abort.\n"); return; }
  if (!strcmp(line,"basic")) { listenOnly(5); return; }
  if (sscanf(line,"listen %u %c",&a,&extra)==1 && a>=1 && a<=30) { listenOnly(a); return; }
  if (!strcmp(line,"intermediate")) { abortRequested=false; polling=false; probeLink(current); uartStatus(); return; }
  if (!strcmp(line,"deeper")) { abortRequested=false; polling=false; deepCheck(current); return; }
  if (!strcmp(line,"diagnose")) { listenOnly(3); if(!abortRequested&&probeLink(current))deepCheck(current); else uartStatus(); return; }
  if (sscanf(line,"reg %x %u %c",&a,&b,&extra)==2 && a<=65535 && b>=1 && b<=3 && a+b<=65536UL) { abortRequested=false; polling=false; dumpRegisters(current,uint16_t(a),uint8_t(b)); uartStatus(); return; }
  if (!strcmp(line,"scan")) {
    probe=0; scanning=true; polling=false; abortRequested=false;
    LOG("Communication scan starts: %u..%u, %u candidates; FC03 0x0034. Valid exception counts as a reply. Type x to abort.\n",firstID,lastID,unsigned(lastID-firstID+1)*18); return;
  }
  if (!strcmp(line,"read")) { abortRequested=false; readEnergy(current); return; }
  if (!strcmp(line,"poll")) { abortRequested=false; polling=!polling; lastPoll=millis()-POLL_MS; LOG("Polling %s\n",polling?"ON":"OFF"); return; }
  if (sscanf(line,"id %u %c",&a,&extra)==1 && a>=1 && a<=247) { current.id=a; return; }
  if (sscanf(line,"baud %lu %c",&baud,&extra)==1 && supportedBaud(baud)) { current.baud=baud; return; }
  if (sscanf(line,"parity %c %c",&parity,&extra)==1) {
    parity=char(toupper(uint8_t(parity))); if (parity=='N'||parity=='E'||parity=='O') { current.parity=parity; return; }
  }
  if (sscanf(line,"range %u %u %c",&a,&b,&extra)==2 && a>=1 && b<=247 && a<=b) { firstID=a; lastID=b; return; }
  LOG("Unknown/invalid command. Type help.\n");
}
void serialInput() {
  static char line[80]; static uint8_t used=0; static bool tooLong=false;
  for (unsigned n=0; n<80 && Serial.available(); ++n) {
    char ch=char(Serial.read());
    if((scanning||polling)&&(ch=='x'||ch=='X')) {abortRequested=true;scanning=false;polling=false;used=0;LOG("Stopped.\n");continue;}
    if (ch=='\r') continue;
    if (ch=='\n') {
      if (tooLong) LOG("Command too long; discarded.\n");
      else if (used) { line[used]=0; command(line); }
      used=0; tooLong=false;
    } else if (used<sizeof(line)-1 && !tooLong) line[used++]=ch;
    else tooLong=true;
  }
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
      Link candidate; candidate.id=firstID+probe%count; candidate.baud=BAUDS[mode/3]; candidate.parity=PARITIES[mode%3];
      LOG("\nSCAN %u/%u\n",probe+1,count*18);
      bool found=probeLink(candidate); ++probe;
      if (abortRequested) { scanning=false; LOG("Scan aborted. Previous reader settings retained.\n"); }
      else if (found) { current=candidate; scanning=false; LOG("[COMM_FOUND] ID=%u %lu 8%c1; reader settings selected in RAM. Use deeper to check Fort registers before poll.\n",current.id,(unsigned long)current.baud,current.parity); }
      else if (probe>=count*18) { scanning=false; LOG("[SCAN_FINISHED] no matching Modbus reply found. Settings retained. UART RX/errors/echo and register support are separate observations.\n"); }
    }
  } else if (polling && uint32_t(millis()-lastPoll)>=POLL_MS) {
    lastPoll=millis(); readEnergy(current); if (abortRequested) polling=false;
  }
  delay(5);
}
