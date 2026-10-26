"""Generate production class from the independently usable INO; no example include at compile."""
from pathlib import Path
import hashlib
import sys
root=Path(__file__).resolve().parent.parent
source=(root/'examples/FortSerialDebug/FortSerialDebug.ino').read_text(encoding='utf-8')
body=source[source.index('// Defaults only:'):source.index('void setup() {')]
body=body.replace('HardwareSerial meter(1);','HardwareSerial &meter;')
body=body.replace('consoleHead=consoleTail=consoleCount=0;', 'consoleHead=consoleTail=consoleCount=consoleUsed=0;consoleDiscard=false;consoleLine[0]=0;')
body=body.replace('bool allowedPin(unsigned pin){return ', 'bool allowedPin(unsigned pin){if(directionPinReserved&&pin==16)return false;return ')
body=body.replace('uint32_t BAUDS[]', 'uint32_t BAUDS[6]').replace('char PARITIES[]', 'char PARITIES[3]').replace('constexpr uint8_t CONSOLE_CAPACITY', 'static constexpr uint8_t CONSOLE_CAPACITY')
body=body.replace('meter.onReceiveError(uartError);', 'meter.onReceiveError([this](hardwareSerial_error_t error){uartError(error);});')
body=body.replace('  meter.onReceiveError([this](hardwareSerial_error_t error){uartError(error);});','  if(!meter.setRxFIFOFull(link.baud<=38400?1:16)||!meter.setRxTimeout(1))LOG("[UART_RX_TUNING_UNAVAILABLE] default buffering retained; timestamps remain software observations\\n");\n  meter.onReceiveError([this](hardwareSerial_error_t error){uartError(error);});')
body=body.replace('++evidence.txErrors;LOG("[TX_ERROR]', 'if(event)event(8,link,address,words,tx,nullptr,0,false);++evidence.txErrors;LOG("[TX_ERROR]')

body=body.replace('#define LOG(...) do { if (Serial) Serial.printf(__VA_ARGS__); } while (0)','#define LOG(...) logf(__VA_ARGS__)')
body=body.replace('  if (!Serial) return;','')
body=body.replace('Serial.printf(', 'logf(').replace('Serial.println();','logf("\\n");')
body=body.replace('void checkAbort(){pumpConsole();}', 'void checkAbort(){if(service)service();if(serialInputEnabled)pumpConsole();}')
body=body.replace('  pumpConsole();\n  if(scanning||!consoleCount)return;', '  if(serialInputEnabled)pumpConsole();\n  if(scanning||!consoleCount)return;')
body=body.replace('if(!meter){++evidence.txErrors;', 'if(!meter){if(event)event(8,link,address,words,nullptr,nullptr,0,false);++evidence.txErrors;')
body=body.replace('  hexLog("TX",tx,sizeof(tx));', '  hexLog("TX",tx,sizeof(tx));\n  if(event)event(1,link,address,words,tx,nullptr,0,false);')
body=body.replace('  meter.flush();', '  meter.flush();\n  if(event)event(2,link,address,words,tx,nullptr,0,false);')
body=body.replace('    for (unsigned n=0; n<128 && meter.available(); ++n) {', '    bool firstChunk=size==0;\n    for (unsigned n=0; n<128 && meter.available(); ++n) {')
body=body.replace('    if (size && uint32_t(micros()-lastByte)>=gapUs) {', '    if(firstChunk&&size&&event)event(3,link,address,words,tx,rx,size,overflow);\n    if (size && uint32_t(micros()-lastByte)>=gapUs) {\n      if(event)event(4,link,address,words,tx,rx,size,overflow);')
body=body.replace('  if (size) {hexLog(', '  if(size&&event)event(5,link,address,words,tx,rx,size,overflow);\n  if (size) {hexLog(')
body=body.replace('  if(!sawBytes)++evidence.timeouts;', '  if(!sawBytes&&event)event(6,link,address,words,tx,nullptr,0,false);\n  if(!sawBytes)++evidence.timeouts;')
body=body.replace('evidence.staleBytes+=drained;', 'evidence.staleBytes+=drained;if(drained&&event)event(7,link,address,words,tx,stale,drained<sizeof(stale)?drained:sizeof(stale),drained>sizeof(stale));')
body=body.replace('    total+=size;', '    total+=size;if(size&&event)event(9,current,0,0,nullptr,bytes,size,false);')
body=body.replace('void command(const char *line) {', 'void command(const char *line) {\n  if(!strcmp(line,"resume")){if(onResume)onResume();return;}')
body=body.replace('  LOG("[SERIAL_INPUT] %s\\n",line);command(line);', '  executing=true;LOG("[SERIAL_INPUT] %s\\n",line);command(line);executing=false;')
body=body.replace('FortSerialDebug 1.3.0 | Serial 115200 + Newline', 'Fort main diagnostics | Web console or Serial 115200 + Newline')
loop=source[source.index('void loop() {'):]
loop=loop[loop.index('  serialInput();'):loop.rindex('  delay(5);')]
helpers=r'''
  explicit DiagnosticEngine(HardwareSerial &port,char *storage=nullptr,size_t capacity=0):meter(port),logStorage(storage),logCapacity(capacity){}
  void (*service)()=nullptr;
  void (*event)(uint8_t,const Link &,uint16_t,uint8_t,const uint8_t *,const uint8_t *,size_t,bool)=nullptr;
  void (*onResume)()=nullptr;
  bool serialInputEnabled=false,serialOutput=false,executing=false,directionPinReserved=false;
  char *logStorage=nullptr;size_t logCapacity=0,logStart=0,logUsed=0;
  uint32_t logDiscarded=0,serialOffset=0;
  void logf(const char *format,...) {
    if(!logStorage&&!serialOutput)return;
    char text[512];va_list args;va_start(args,format);int wanted=vsnprintf(text,sizeof(text),format,args);va_end(args);
    size_t size=wanted<0?0:size_t(wanted)<sizeof(text)?size_t(wanted):sizeof(text)-1;
    for(size_t i=0;logStorage&&i<size;++i){
      if(logUsed<logCapacity)logStorage[(logStart+logUsed++)%logCapacity]=text[i];
      else{logStorage[logStart]=text[i];logStart=(logStart+1)%logCapacity;++logDiscarded;}
    }

  }
  void copyLog(char *out,size_t capacity)const{
    if(!capacity)return;
    size_t count=logUsed<capacity-1?logUsed:capacity-1;
    for(size_t i=0;i<count;++i)out[i]=logStorage[(logStart+i)%logCapacity];
    out[count]=0;
  }
  void clearLog(){logStart=logUsed=0;logDiscarded=serialOffset=0;}
  void drainSerial(){
    if(!serialOutput||!Serial||!logStorage)return;
    if(serialOffset<logDiscarded)serialOffset=logDiscarded;
    size_t available=Serial.availableForWrite(),pending=logDiscarded+logUsed-serialOffset;
    size_t amount=available<pending?available:pending;if(amount>128)amount=128;
    uint8_t text[128];for(size_t i=0;i<amount;++i)text[i]=logStorage[(logStart+serialOffset-logDiscarded+i)%logCapacity];
    if(amount)serialOffset+=Serial.write(text,amount);
  }
  bool submit(const char *line){
    size_t size=strlen(line);if(!size||size>=CONSOLE_LINE)return false;
    if(!strcmp(line,"x")||!strcmp(line,"stop")){emergencyStop();return true;}
    if(consoleCount>=CONSOLE_CAPACITY)return false;
    strcpy(consoleQueue[consoleTail],line);consoleTail=(consoleTail+1)%CONSOLE_CAPACITY;++consoleCount;return true;
  }
  bool busy()const{return executing||scanning||polling||consoleCount;}
  void tick(){
'''
prefix='// Generated by tools/build-diagnostic.py. Do not edit directly.\n// SPDX-License-Identifier: GPL-3.0-only\n#pragma once\n#include <Arduino.h>\n#include <atomic>\n#include <math.h>\n#include <stdarg.h>\n#include <string.h>\n#include <ctype.h>\nnamespace fort {\nclass DiagnosticEngine {\n public:\n'
result=prefix+body+helpers+loop+'\n  }\n};\n#undef LOG\n} // namespace fort\n'
result='// INO SHA256: '+hashlib.sha256(source.encode('utf-8')).hexdigest()+'\n'+result
path=root/'firmware/include/diagnostic_engine.h'
if '--check' in sys.argv:
    if not path.exists()or path.read_text(encoding='utf-8')!=result:raise SystemExit('Stale diagnostic engine; run python tools/build-diagnostic.py')
else:path.write_text(result,encoding='utf-8',newline='\n')
print('Diagnostic engine verified'if '--check'in sys.argv else'Generated production diagnostic engine')
