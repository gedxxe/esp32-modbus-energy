#include <assert.h>
#include <iostream>
#include <cstdlib>
#undef assert
#define assert(x) do { if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";std::exit(1);} } while(0)
#include "FortDiagnostics.h"
#include "../include/meter_registry_store.h"
#include <map>
using namespace fort;
static FortDiagnostics *stopTarget=nullptr;static int cooperateCount=0,lastObservedKind=0;static bool resumed=false;
std::vector<uint8_t> response(std::initializer_list<uint8_t> bytes){std::vector<uint8_t> out(bytes);uint16_t crc=meterCRC(out.data(),out.size());out.push_back(uint8_t(crc));out.push_back(uint8_t(crc>>8));return out;}
struct RegistryBackend {
 std::map<std::string,std::vector<uint8_t>> values;bool failWrite=false,failRead=false;
 bool isKey(const char *key){return values.count(key);}
 size_t getBytesLength(const char *key){return values[key].size();}
 size_t getBytes(const char *key,void *out,size_t n){if(failRead)return 0;auto &v=values[key];if(v.size()<n)return 0;memcpy(out,v.data(),n);return n;}
 size_t putBytes(const char *key,const void *bytes,size_t n){if(failWrite)return 0;auto p=static_cast<const uint8_t*>(bytes);values[key]=std::vector<uint8_t>(p,p+n);return n;}
};
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
  assert(engine.options.probeAddress==0x64&&engine.options.probeWords==1&&engine.baudCount==5);engine.command("ratios FFFE");assert(engine.options.ratioAddress==0xFFFE);engine.command("ratios 008D");
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
 // The universal survey continues after a silent first probe and records exceptions separately.
 {HardwareSerial port(1);FortDiagnostics engine(port);DiscoveryConfig config;config.first=1;config.last=1;config.responseMs=150;
  assert(engine.discovery.begin(config,17,millis(),99));
  port.onWrite=[&](){const auto &tx=port.transmitted.back();if(tx[3]==0x64)for(auto byte:response({1,0x83,2}))port.incoming.push_back(byte);};
  while(engine.discovery.running())engine.discovery.step(engine,millis(),1793185200);
  assert(engine.discovery.phase==DISCOVERY_COMPLETE&&engine.discovery.count==1&&engine.discovery.tested==7);
  const auto &device=engine.discovery.found[0];assert(device.confirmed()&&device.mapEvidence()==0&&device.probes[4].confirmed());assert(device.probes[0].status[0]==0xE2);
  RegistryBackend backend;MeterRegistryStore storage;assert(storage.load(backend,true));MeterRegistry next=storage.registry;
  next.count=1;next.nextId=2;next.entries[0].id=1;strcpy(next.entries[0].name,"Panel 4");strcpy(next.entries[0].model,"Unknown meter");next.entries[0].evidence=device;
  assert(storage.commit(backend,next));MeterRegistryStore reboot;assert(reboot.load(backend,true)&&reboot.registry.entries[0].id==1);
  Settings chosen;assert(!canActivateSaved(reboot.registry.entries[0],chosen));
  auto saved=storage.registry;backend.failWrite=true;strcpy(next.entries[0].name,"Changed");assert(!storage.commit(backend,next));assert(!strcmp(storage.registry.entries[0].name,"Panel 4"));
  backend.failWrite=false;assert(storage.commit(backend,next));assert(backend.values.size()==3);
  backend.values["meters_b"][30]^=1;MeterRegistryStore recover;assert(recover.load(backend,true));assert(!strcmp(recover.registry.entries[0].name,"Panel 4"));
  // A newer bank may be lost after its ID was reserved. Recovery must skip that ID.
  auto third=recover.registry;third.entries[1]=third.entries[0];third.entries[1].id=third.nextId++;strcpy(third.entries[1].name,"Second");third.count=2;assert(recover.commit(backend,third));
  backend.values["meters_b"][30]^=1;MeterRegistryStore older;assert(older.load(backend,true));assert(older.registry.nextId==3&&older.registry.count==1);
  uint8_t bytes[METER_REGISTRY_BYTES];encodeMeterRegistry(saved,bytes);MeterRegistry decoded;assert(decodeMeterRegistry(bytes,sizeof(bytes),decoded));bytes[40]^=1;assert(!decodeMeterRegistry(bytes,sizeof(bytes),decoded));
  Settings old;old.magic=0x46525433;old.profile=profiles::ACREL_ADL400;uint8_t legacy[16]={};memcpy(legacy,&old,12);meterPut16(legacy+12,crc16(legacy,12));uint16_t id=99;assert(decodeMeterConfig(legacy,16,chosen,id)&&chosen.profile==profiles::ACREL_ADL400&&id==0);
  encodeMeterConfig(chosen,123,legacy);assert(decodeMeterConfig(legacy,16,chosen,id)&&id==123);legacy[12]^=1;assert(!decodeMeterConfig(legacy,16,chosen,id));
 }
 {HardwareSerial port(1);FortDiagnostics engine(port);engine.command("timeout 150");engine.command("range 1 1");engine.command("scanmode current");engine.command("discover");assert(engine.discovery.running());engine.command("stop");assert(!engine.discovery.running()&&engine.discovery.phase==DISCOVERY_CANCELLED);}
 {HardwareSerial port(1);FortDiagnostics engine(port);auto link=engine.current;link.stops=2;engine.openLink(link);unsigned before=port.beginCalls;engine.openLink(link);assert(port.beginCalls==before);link.stops=1;engine.openLink(link);assert(port.beginCalls==before+1);}
 {HardwareSerial port(1);FortDiagnostics engine(port);DiscoveryConfig config;config.first=1;config.last=5;config.responseMs=150;
  port.onWrite=[&](){const auto &tx=port.transmitted.back();std::vector<uint8_t> bytes={tx[0],3,uint8_t(tx[5]*2)};bytes.resize(3+tx[5]*2,0);uint16_t crc=meterCRC(bytes.data(),bytes.size());bytes.push_back(uint8_t(crc));bytes.push_back(uint8_t(crc>>8));for(auto byte:bytes)port.incoming.push_back(byte);};
  assert(engine.discovery.begin(config,2,millis(),50));while(engine.discovery.running())engine.discovery.step(engine,millis(),0);
  assert(engine.discovery.phase==DISCOVERY_CAPACITY&&engine.discovery.count==4&&engine.discovery.tested==28);
  auto &found=engine.discovery.found[0];assert(found.mapEvidence()==3);SavedMeter saved;saved.id=9;saved.profile=profiles::ACREL_ADL400;saved.verified=true;strcpy(saved.name,"Panel");strcpy(saved.model,"ADL400");saved.evidence=found;Settings settings;assert(canActivateSaved(saved,settings));
  saved.evidence.link.stops=2;assert(!canActivateSaved(saved,settings));
  DiscoveryConfig plan;assert(parseDiscoveryPlan("FFFE:2,0000:1",plan)&&plan.count==2);auto previous=plan;assert(!parseDiscoveryPlan("FFFF:2",plan)&&plan.count==previous.count);assert(!parseDiscoveryPlan("0000:1,0000:1",plan));assert(!parseDiscoveryPlan("0000:1,",plan));
 }
 assert(meter::signed32(0xFFFFFF9CUL)==-100);assert(meter::float32(0x3F80,0)==1.0f);assert(meter::float32(0,0x3F80)!=1.0f);
 std::cout<<"PASS: real ModbusMaster FC03 reference bytes, manual callback order, immediate length completion, CRC/ID/function/count/echo/overflow/exception rejection, timeout/cancel, UART recovery and decoding\n";
}
