#include <assert.h>
#include <iostream>
#include <cstdlib>
#undef assert
#define assert(x) do { if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";std::exit(1);} } while(0)
#include "FortDiagnostics.h"
using namespace fort;
static FortDiagnostics *stopTarget=nullptr;static int cooperateCount=0,lastObservedKind=0;static bool resumed=false;
std::vector<uint8_t> response(std::initializer_list<uint8_t> bytes){std::vector<uint8_t> out(bytes);uint16_t crc=meterCRC(out.data(),out.size());out.push_back(uint8_t(crc));out.push_back(uint8_t(crc>>8));return out;}
int main(){
 uint8_t out[6]={};
 {HardwareSerial port(1);FortDiagnostics engine(port);FortDiagnostics::Capture a,b;
  assert(engine.sameCaptureSettings(a,b));b.settings.manualDirection=false;assert(!engine.sameCaptureSettings(a,b));b=a;
  b.settings.settleUs=200;assert(!engine.sameCaptureSettings(a,b));b=a;
  b.settings.gapMs=300;assert(!engine.sameCaptureSettings(a,b));b=a;
  b.settings.dir=15;assert(!engine.sameCaptureSettings(a,b));
  engine.options.dir=15;assert(!engine.allowedPin(15)&&engine.allowedPin(16));
 }

 {HardwareSerial port(1);char log[8192];FortDiagnostics engine(port,log,sizeof(log));
  port.replies.push_back(response({1,3,4,0,0,0xC3,0x17}));assert(engine.readRegisters(engine.current,0x58,2,out));
  assert(meter::u32((out[0]<<8)|out[1],(out[2]<<8)|out[3])==49943);
  const uint8_t tx[]={1,3,0,0x58,0,2,0x45,0xD8};assert(port.transmitted[0]==std::vector<uint8_t>(tx,tx+8));
  assert(pinEvents.size()>=4);bool high=false,lowAfter=false;for(const auto &e:pinEvents){if(e.pin==16&&e.value==HIGH)high=true;if(high&&e.pin==16&&e.value==LOW)lowAfter=true;}assert(high&&lowAfter);
 }
 // A complete length-delimited response must not wait for a t3.5 software gap.
 {HardwareSerial port(1);FortDiagnostics engine(port);engine.options.responseMs=50;
  port.onWrite=[&](){auto bytes=response({1,3,4,0,0,0xC3,0x17});for(size_t i=0;i<bytes.size();++i)port.scheduled.push_back({testMicros+200+uint64_t(i)*1100,{bytes[i]}});};
  assert(engine.readRegisters(engine.current,0x58,2,out));assert(engine.evidence.data==1);}
 for(unsigned kind=0;kind<7;++kind){HardwareSerial port(1);FortDiagnostics engine(port);engine.options.responseMs=50;auto bytes=response({1,3,4,0,0,0xC3,0x17});
  if(kind==0)bytes[4]^=1;
  if(kind==1)bytes=response({2,3,4,0,0,0xC3,0x17});
  if(kind==2)bytes=response({1,4,4,0,0,0xC3,0x17});
  if(kind==3)bytes=response({1,3,2,0,1});
  if(kind==4)bytes={1,3};
  if(kind==5)bytes=response({1,3,0,0x58,0,2});
  if(kind==6){bytes=response({1,3,255,0,0});bytes.resize(100,0x55);}
  port.replies.push_back(bytes);assert(!engine.readRegisters(engine.current,0x58,2,out));assert(engine.evidence.data==0&&engine.lastReply==FortDiagnostics::REPLY_NONE);
 }
 {HardwareSerial port(1);FortDiagnostics engine(port);port.replies.push_back(response({1,0x83,2}));assert(!engine.readRegisters(engine.current,0x58,2,out));assert(engine.lastReply==FortDiagnostics::REPLY_EXCEPTION&&engine.lastStatus==2);}
 {HardwareSerial port(1);FortDiagnostics engine(port);auto bad=response({1,0x83,2});bad[4]^=1;port.replies.push_back(bad);assert(!engine.readRegisters(engine.current,0x58,2,out));assert(engine.lastStatus==0xE3&&engine.evidence.exceptions==0);}
 {HardwareSerial port(1);FortDiagnostics engine(port);engine.options.responseMs=50;assert(!engine.readRegisters(engine.current,0x58,2,out));assert(engine.lastStatus==0xE2&&engine.evidence.timeouts==1);}
 {HardwareSerial port(1);FortDiagnostics engine(port);port.writeLimit=2;assert(!engine.readRegisters(engine.current,0x58,2,out));assert(engine.lastStatus==0xE5);}
 {HardwareSerial port(1);FortDiagnostics engine(port);port.ready=false;assert(!engine.readRegisters(engine.current,0x58,2,out));port.ready=true;port.replies.push_back(response({1,3,4,0,0,0,7}));assert(engine.readRegisters(engine.current,0x58,2,out));}
 {HardwareSerial port(1);char log[128];FortDiagnostics engine(port,log,sizeof(log));engine.command("timeout 50");engine.command("deeper");char copy[129];engine.copyLog(copy,sizeof(copy));assert(engine.logDiscarded&&strlen(copy)<=128);}
 {HardwareSerial port(1);FortDiagnostics engine(port);port.onWrite=[](){Serial.input={'x','\n'};};engine.serialInputEnabled=true;uint32_t start=millis();assert(!engine.readRegisters(engine.current,0x58,2,out));assert(engine.lastStatus==0xE4&&uint32_t(millis()-start)<10);}
 {HardwareSerial port(1);FortDiagnostics engine(port);engine.options.manualDirection=false;pinEvents.clear();port.replies.push_back(response({1,3,4,0,0,0,7}));assert(engine.readRegisters(engine.current,0x58,2,out));assert(pinEvents.empty());}
 {HardwareSerial port(1);char log[2048];FortDiagnostics engine(port,log,sizeof(log));
  port.replies.push_back(response({1,3,4,0,0,3,0xE8}));port.replies.push_back(response({1,3,6,0,1,0,1,0,80}));
  assert(engine.readEnergy(engine.current));char outLog[2049];engine.copyLog(outLog,sizeof(outLog));assert(std::string(outLog).find("80.000000 kWh")!=std::string::npos);}
 {HardwareSerial port(1);FortDiagnostics engine(port);engine.openLink(engine.current);port.incoming=std::deque<uint8_t>(20,0x55);
  stopTarget=&engine;cooperateCount=0;engine.service=[](){if(++cooperateCount==5)stopTarget->abortRequested=true;};
  assert(!engine.readRegisters(engine.current,0x58,2,out));assert(engine.lastStatus==0xE4&&port.transmitted.empty());}
 {HardwareSerial port(1);FortDiagnostics engine(port);engine.options.responseMs=50;lastObservedKind=0;
  engine.event=[](uint8_t kind,const FortDiagnostics::Link &,uint16_t,uint8_t,const uint8_t*,const uint8_t*,size_t,bool){lastObservedKind=kind;};
  port.onWrite=[&](){port.scheduled.push_back({testMicros+51100,response({1,3,4,0,0,0,7})});};
  assert(!engine.readRegisters(engine.current,0x58,2,out));assert(engine.evidence.data==0&&engine.lastStatus==0xE2);assert(lastObservedKind==5);}
 {HardwareSerial port(1);FortDiagnostics engine(port);engine.serialInputEnabled=true;resumed=false;engine.onResume=[](){resumed=true;};
  port.onWrite=[](){for(char byte:std::string("resume\n"))Serial.input.push_back(byte);};
  assert(!engine.readRegisters(engine.current,0x58,2,out));assert(engine.lastStatus==0xE4&&!resumed&&engine.resumePending);
  engine.tick();assert(resumed&&!engine.resumePending);}
 {HardwareSerial port(1);FortDiagnostics engine(port);engine.serialInputEnabled=true;engine.options.responseMs=50;
  port.onWrite=[](){for(char byte:std::string("status\nreport\n"))Serial.input.push_back(byte);};
  port.replies.push_back(response({1,3,4,0,0,0,7}));assert(engine.readRegisters(engine.current,0x58,2,out));assert(engine.consoleCount==2);
  engine.tick();engine.tick();assert(!engine.consoleCount);}
 // Acrel profile must use its own query width, ratio layout and kWh resolution.
 {HardwareSerial port(1);char log[4096];FortDiagnostics engine(port,log,sizeof(log));
  engine.command("profile acrel-adl400");assert(engine.options.profile==profiles::ACREL_ADL400);
  assert(engine.options.probeAddress==0x64&&engine.options.probeWords==1&&engine.baudCount==5);
  assert(engine.auditPlan[2].address==0x16A&&engine.auditPlan[3].address==0x77);
  port.replies.push_back(response({1,3,2,0,123}));assert(engine.probeLink(engine.current));
  assert(port.transmitted.back()[3]==0x64&&port.transmitted.back()[5]==1);
  port.replies.push_back(response({1,3,4,0,1,0xE2,0x40})); // 123456 -> 1234.56 secondary kWh
  port.replies.push_back(response({1,3,4,0,2,0,40})); // PT=2, CT=40
  assert(engine.readEnergy(engine.current));char copy[4097];engine.copyLog(copy,sizeof(copy));
  assert(std::string(copy).find("98764.800000 kWh")!=std::string::npos);
  assert(port.transmitted.back()[3]==0x8D&&port.transmitted.back()[5]==2);
  const size_t before=port.transmitted.size();engine.command("primary");assert(port.transmitted.size()==before);
  engine.command("profile fort");assert(engine.options.probeAddress==0x58&&engine.options.probeWords==2&&engine.baudCount==6);
  engine.command("profile unknown");assert(engine.options.profile==profiles::FORT);
 }
 const uint8_t signedPower[]={0xFF,0xFF,0xD8,0xF0},voltage[]={0x08,0xFC};
 assert(fabs(profiles::decode(profiles::get(1).power,signedPower)+1.0)<1e-10);
 assert(fabs(profiles::decode(profiles::get(1).voltage,voltage)-230.0)<1e-10);
 assert(meter::signed32(0xFFFFFF9CUL)==-100);assert(meter::float32(0x3F80,0)==1.0f);assert(meter::float32(0,0x3F80)!=1.0f);
 std::cout<<"PASS: real ModbusMaster FC03 reference bytes, manual callback order, immediate length completion, CRC/ID/function/count/echo/overflow/exception rejection, timeout/cancel, UART recovery and decoding\n";
}
