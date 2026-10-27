// Compile and execute the actual standalone INO with the real ModbusMaster source.
#include <assert.h>
#include <iostream>
#include <cstdlib>
#undef assert
#define assert(x) do { if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";std::exit(1);} } while(0)
#include <fstream>
#include <iterator>
#include "../../examples/FortSerialDebug/FortSerialDebug.ino"
std::vector<uint8_t> response(std::initializer_list<uint8_t> bytes){std::vector<uint8_t> out(bytes);uint16_t crc=fort::meterCRC(out.data(),out.size());out.push_back(uint8_t(crc));out.push_back(uint8_t(crc>>8));return out;}
void submit(const std::string &line){for(char c:line)Serial.input.push_back(c);Serial.input.push_back('\n');for(unsigned i=0;i<10000&&(Serial.available()||debug.busy());++i)loop();assert(!Serial.available()&&!debug.busy());for(unsigned i=0;i<80;++i)debug.drainSerial();}
int main(){
 setup();assert(debug.options.manualDirection&&debug.options.probeAddress==0x58&&debug.options.probeWords==2);
 fortUART.replies.push_back(response({1,3,4,0,0,0xC3,0x17}));submit("intermediate");assert(debug.evidence.data==1&&fortUART.transmitted.back()[3]==0x58);
 assert(Serial.output.find("VALID_RESPONSE")!=std::string::npos);
 submit("timeout 60");submit("direction auto");assert(!debug.options.manualDirection);submit("direction manual");assert(debug.options.manualDirection);
 submit("pins 16 17");assert(debug.options.rx==18);submit("pins 17 18");assert(debug.options.rx==17);submit("pins 18 17");
 submit("reg FFFF 3");assert(debug.options.rx==18);
 submit("probe 0080 1");submit("gap 20");submit("settle 120");assert(debug.options.probeAddress==0x80&&debug.options.gapMs==20&&debug.options.settleUs==120);
 submit("probe 0058 2");submit("stats clear");fortUART.replies.push_back(response({1,0x83,2}));submit("intermediate");assert(debug.evidence.exceptions==1&&debug.evidence.data==0);
 submit("decode off");submit("plan clear");submit("plan add 0080 1");fortUART.replies.push_back({});fortUART.replies.push_back(response({1,3,2,0x13,0x88}));submit("diagnose");assert(debug.evidence.data==1);
 submit("rxcheck");assert(debug.activeBaud==debug.current.baud);
 debug.submit("poll");loop();Serial.input={'x','\n'};loop();assert(!debug.polling&&!debug.consoleCount);
 const char *recipes[]={"01-passive-rx.txt","02-echo-or-no-reply.txt","03-serial-matrix.txt","04-register-exception.txt","05-raw-map-audit.txt","06-configured-kwh.txt","07-timeout-comparison.txt","08-word-order-comparison.txt"};
 std::string root=__FILE__;std::replace(root.begin(),root.end(),char(92),'/');root=root.substr(0,root.rfind("firmware/test/"));
 for(const char *name:recipes){std::ifstream file(root+"examples/FortSerialDebug/cases/"+name);assert(file);std::string line;while(std::getline(file,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;submit(line);}}
 for(const auto &tx:fortUART.transmitted){assert(tx.size()==8&&tx[1]==3&&tx[0]>0&&tx[0]<=247);assert(fort::meterCRC(tx.data(),6)==(uint16_t(tx[6])|(uint16_t(tx[7])<<8)));}
 assert(Serial.output.find("Fort diagnostics 2.2.0")!=std::string::npos);
 std::cout<<"PASS: actual standalone INO + real ModbusMaster; manual direction, runtime controls, exception safety, fallback plan, all eight recipes and FC03-only requests\n";
}
