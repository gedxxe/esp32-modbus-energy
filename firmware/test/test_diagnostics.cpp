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
  engine.command("profile acrel-adl400");assert(engine.options.profile==profiles::ACREL_ADL400_VERIFIED);
  assert(engine.options.probeAddress==0&&engine.options.probeWords==2&&engine.baudCount==5);engine.command("ratios FFFE");assert(engine.options.ratioAddress==0xFFFE);engine.command("ratios 008D");
  assert(engine.auditPlan[2].address==0x5300&&engine.auditPlan[3].address==0x006A&&engine.auditPlan[5].address==0x5332);
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
 // Reproduce the user's ADL400 scan through the actual shared ModbusMaster transport.
 {HardwareSerial port(1);char log[8192];FortDiagnostics engine(port,log,sizeof(log));engine.command("profile acrel-adl400");engine.command("id 220");
  const auto &p=profiles::get(engine.options.profile);assert(p.energy.address==0&&p.energy.words==2&&p.energy.encoding==profiles::U32);
  bool reverse=false,changeRatio=false,failEnergy=false;unsigned ratios=0;
  port.onWrite=[&](){const auto &tx=port.transmitted.back();assert(tx[0]==220&&tx[1]==3);uint16_t a=uint16_t(tx[2])<<8|tx[3];
   if(failEnergy&&a==0){for(auto b:response({220,0x83,2}))port.incoming.push_back(b);return;}
   uint32_t raw=a==0x64?10000:a==0?9798:a==0xA?9790:a==0x14?(reverse?1:0):a==0x1E?100:a==0x8D?1:a==0x8E?(changeRatio&&++ratios>1?81:80):a==0x5300?0x436B8CB0UL:a==0x5332?0x4247D70AUL:a==0x5330?0x3F733333UL:0x42C80000UL;
   if(tx[5]==1){for(auto b:response({220,3,2,uint8_t(raw>>8),uint8_t(raw)}))port.incoming.push_back(b);}
   else for(auto b:response({220,3,4,uint8_t(raw>>24),uint8_t(raw>>16),uint8_t(raw>>8),uint8_t(raw)}))port.incoming.push_back(b);
  };
  assert(engine.readEnergy(engine.current));assert(port.transmitted.size()==3&&port.transmitted[0][2]==0&&port.transmitted[0][3]==0&&port.transmitted[0][5]==2&&port.transmitted[1][3]==0x8D);
  char copy[8193];engine.copyLog(copy,sizeof(copy));assert(std::string(copy).find("7838.400000 kWh")!=std::string::npos);
  uint8_t voltage[]={0x43,0x6B,0x8C,0xB0},hz[]={0x42,0x47,0xD7,0x0A};assert(fabs(profiles::decode(p.voltage,voltage)-235.54956)<.0001&&fabs(profiles::decode(p.frequency,hz)-49.96)<.0001);
  assert(fabs(profiles::primaryEnergy(p.id,9798,1,1,80)-7838.4)<1e-8);assert(fabs(7838.9-7838.4)<.01*80);
  engine.clearLog();engine.command("adl400");engine.copyLog(copy,sizeof(copy));assert(engine.evidence.requests==15);
  std::string report(copy);assert(report.find("[ADL400_BEGIN] ID=220")!=std::string::npos&&report.find("address=0x0000 energy=7838.400000")!=std::string::npos&&report.find("[ADL400_REVERSE] zero")!=std::string::npos&&report.find("complete=1")!=std::string::npos);
  engine.clearLog();reverse=true;engine.command("adl400");engine.copyLog(copy,sizeof(copy));assert(std::string(copy).find("[ADL400_REVERSE] nonzero")!=std::string::npos);
  engine.clearLog();changeRatio=true;ratios=0;engine.command("adl400");engine.copyLog(copy,sizeof(copy));assert(std::string(copy).find("valid_stable=0")!=std::string::npos&&std::string(copy).find("[ADL400_SCALED]")==std::string::npos);
  changeRatio=false;assert(engine.discoveryOptions.count==3);auto cfg=engine.discoveryOptions;cfg.preferred.slave=220;cfg.first=cfg.last=220;cfg.responseMs=150;
  assert(engine.discovery.begin(cfg,333,millis(),1));while(engine.discovery.running())engine.discovery.step(engine,millis(),1793185200);
  assert(engine.discovery.found[0].mapEvidence()&(1<<profiles::ACREL_ADL400_VERIFIED));SavedMeter savedMeter;savedMeter.id=8;savedMeter.profile=profiles::ACREL_ADL400_VERIFIED;savedMeter.verified=true;savedMeter.evidence=engine.discovery.found[0];strcpy(savedMeter.name,"ADL panel");strcpy(savedMeter.model,"ADL400");Settings bound;assert(canActivateSaved(savedMeter,bound)&&bound.slave==220);
  failEnergy=true;const size_t before=port.transmitted.size();assert(!engine.readEnergy(engine.current));assert(port.transmitted.size()==before+1); // Never fall back to forward energy.
  uint8_t oldBytes[METER_CONFIG_BYTES];Settings old;old.profile=profiles::ACREL_ADL400;old.slave=220;encodeMeterConfig(old,37,oldBytes);Settings upgraded;uint16_t saved=37;
  assert(decodeMeterConfig(oldBytes,sizeof(oldBytes),upgraded,saved)&&upgraded.profile==profiles::ACREL_ADL400_VERIFIED&&upgraded.slave==220&&saved==0);
  assert(!profiles::selectable(profiles::ACREL_ADL400));uint8_t id=0;assert(!profiles::parse("adl400-legacy",id)&&profiles::parse("acrel-adl400",id)&&id==profiles::ACREL_ADL400_VERIFIED);
 }
 // The universal survey continues after a silent first probe and records exceptions separately.
 {HardwareSerial port(1);FortDiagnostics engine(port);DiscoveryConfig config;config.first=1;config.last=1;config.responseMs=150;
  assert(engine.discovery.begin(config,17,millis(),99));
  port.onWrite=[&](){const auto &tx=port.transmitted.back();if(tx[3]==0x64)for(auto byte:response({1,0x83,2}))port.incoming.push_back(byte);};
  while(engine.discovery.running())engine.discovery.step(engine,millis(),1793185200);
  assert(engine.discovery.phase==DISCOVERY_COMPLETE&&engine.discovery.count==1&&engine.discovery.tested==config.count);
  const auto &device=engine.discovery.found[0];assert(device.confirmed()&&device.mapEvidence()==0&&device.dataAt(0x64,1)==false&&device.probes[3].confirmed());assert(device.probes[0].status[0]==0xE2);
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
  Settings old;old.magic=0x46525433;old.profile=profiles::ACREL_ADL400;uint8_t legacy[16]={};memcpy(legacy,&old,12);meterPut16(legacy+12,crc16(legacy,12));uint16_t id=99;assert(decodeMeterConfig(legacy,16,chosen,id)&&chosen.profile==profiles::ACREL_ADL400_VERIFIED&&id==0);
  encodeMeterConfig(chosen,123,legacy);assert(decodeMeterConfig(legacy,16,chosen,id)&&id==123);legacy[12]^=1;assert(!decodeMeterConfig(legacy,16,chosen,id));
 }
 {HardwareSerial port(1);FortDiagnostics engine(port);engine.command("timeout 150");engine.command("range 1 1");engine.command("scanmode current");engine.command("discover");assert(engine.discovery.running());engine.command("stop");assert(!engine.discovery.running()&&engine.discovery.phase==DISCOVERY_CANCELLED);}
 {HardwareSerial port(1);FortDiagnostics engine(port);auto link=engine.current;link.stops=2;engine.openLink(link);unsigned before=port.beginCalls;engine.openLink(link);assert(port.beginCalls==before);link.stops=1;engine.openLink(link);assert(port.beginCalls==before+1);}
 {HardwareSerial port(1);FortDiagnostics engine(port);DiscoveryConfig config;config.first=1;config.last=5;config.responseMs=150;
  port.onWrite=[&](){const auto &tx=port.transmitted.back();std::vector<uint8_t> bytes={tx[0],3,uint8_t(tx[5]*2)};bytes.resize(3+tx[5]*2,0);uint16_t crc=meterCRC(bytes.data(),bytes.size());bytes.push_back(uint8_t(crc));bytes.push_back(uint8_t(crc>>8));for(auto byte:bytes)port.incoming.push_back(byte);};
  assert(engine.discovery.begin(config,2,millis(),50));while(engine.discovery.running())engine.discovery.step(engine,millis(),0);
  assert(engine.discovery.phase==DISCOVERY_CAPACITY&&engine.discovery.count==4&&engine.discovery.tested==4*config.count);
  auto &found=engine.discovery.found[0];assert(found.mapEvidence()==uint8_t(7));SavedMeter saved;saved.id=9;saved.profile=profiles::ACREL_ADL400;saved.verified=true;strcpy(saved.name,"Panel");strcpy(saved.model,"ADL400");saved.evidence=found;Settings settings;assert(validSavedMeter(saved)&&!canActivateSaved(saved,settings));
  saved.profile=profiles::HABUER;strcpy(saved.model,"Habuer reference");assert(canActivateSaved(saved,settings)&&settings.profile==profiles::HABUER);
  MeterRegistry registry;registry.generation=1;registry.nextId=10;registry.count=1;registry.entries[0]=saved;uint8_t encodedRegistry[METER_REGISTRY_BYTES];encodeMeterRegistry(registry,encodedRegistry);MeterRegistry restoredRegistry;assert(decodeMeterRegistry(encodedRegistry,sizeof(encodedRegistry),restoredRegistry)&&restoredRegistry.entries[0].profile==profiles::HABUER);
  saved.evidence.link.stops=2;assert(!canActivateSaved(saved,settings));
  DiscoveryConfig plan;assert(parseDiscoveryPlan("FFFE:2,0000:1",plan)&&plan.count==2);auto previous=plan;assert(!parseDiscoveryPlan("FFFF:2",plan)&&plan.count==previous.count);assert(!parseDiscoveryPlan("0000:1,0000:1",plan));assert(!parseDiscoveryPlan("0000:1,",plan));
 }

 // The user's Habuer map is parameter-addressed. Every request stays independent.
 {HardwareSerial port(1);char storage[8192];FortDiagnostics engine(port,storage,sizeof(storage));engine.command("profile habuer");
  assert(engine.options.profile==profiles::HABUER&&engine.options.probeAddress==0x30&&engine.options.probeWords==2&&engine.auditCount==4);
  int failure=0;bool invalidPF=false,invalidHz=false;
  port.onWrite=[&](){const auto &tx=port.transmitted.back();uint16_t address=uint16_t(tx[2])<<8|tx[3];
   if(failure==address)return;
   uint32_t raw=address==0x30?(invalidPF?0x3FC00000UL:0x3F600000UL):address==0x32?(invalidHz?0x7FC00000UL:0x42480000UL):address==0x34?123456UL:654321UL;
   assert(address==0x30||address==0x32||address==0x34||address==0x35);assert(tx[5]==2);
   for(auto byte:response({tx[0],3,4,uint8_t(raw>>24),uint8_t(raw>>16),uint8_t(raw>>8),uint8_t(raw)}))port.incoming.push_back(byte);
  };
  size_t before=port.transmitted.size();assert(engine.readEnergy(engine.current));
  const uint16_t addresses[]={0x30,0x32,0x34,0x35};for(unsigned i=0;i<4;++i)assert(port.transmitted[before+i][3]==addresses[i]);
  char text[4097];engine.copyLog(text,sizeof(text));assert(std::string(text).find("12345.600000 kWh")!=std::string::npos);assert(std::string(text).find("[COHERENT_SNAPSHOT]")!=std::string::npos);
  const auto &profile=profiles::get(profiles::HABUER);assert(fabs(profiles::primaryEnergy(profile.id,123456,20,2,400)-12345.6)<1e-8);
  MeterReadings last;last.meterEnergy=42;ReadingQuality quality;
  auto read=[&](const profiles::Register &reg,uint8_t *bytes){return engine.readRegisters(engine.current,reg.address,reg.words,bytes);};
  invalidPF=true;before=port.transmitted.size();assert(!readMeterReadings(profile,read,last,&quality)&&last.meterEnergy==42&&quality.invalid==FIELD_PF&&port.transmitted.size()==before+4);
  invalidPF=false;invalidHz=true;assert(!readMeterReadings(profile,read,last,&quality)&&quality.invalid==FIELD_FREQUENCY&&last.meterEnergy==42);
  invalidHz=false;failure=0x30;engine.options.responseMs=150;assert(!readMeterReadings(profile,read,last,&quality)&&quality.transportFailed==FIELD_PF&&last.meterEnergy==42);
  failure=0;assert(readMeterReadings(profile,read,last,&quality)&&quality.complete);assert(fabs(last.powerFactor-.875)<1e-8&&last.frequency==50&&fabs(last.reactiveEnergy-65432.1)<1e-8);
  assert(!profiles::available(profile.current)&&!profiles::available(profile.voltage)&&!profiles::available(profile.power)&&profile.ratioWords==0);
  assert(!profiles::valid(255)&&!profiles::available(profiles::get(255).probe)&&std::isnan(profiles::primaryEnergy(255,123,1,1,1)));
  DiscoveryConfig defaults;assert(defaults.count==8&&validDiscoveryConfig(defaults));
  for(uint8_t id=0;id<3;++id){DiscoveryQuery probes[3];size_t count=profiles::discoveryQueries(profiles::get(id),probes,3);for(size_t i=0;i<count;++i){bool included=false;for(unsigned j=0;j<defaults.count;++j)included=included||(probes[i].address==defaults.plan[j].address&&probes[i].words==defaults.plan[j].words);assert(included);}}
 }
 // Real transport tests for additive ACR/APM profiles and persisted commissioning.
 for(uint8_t id: {uint8_t(profiles::ACREL_ACR),uint8_t(profiles::ACREL_APM)}){
  HardwareSerial port(1);FortDiagnostics engine(port);engine.selectProfile(id);const auto &p=profiles::get(id);
  bool invalid=false;auto bits=[](float f){uint32_t raw;memcpy(&raw,&f,4);return raw;};
  port.onWrite=[&](){const auto &tx=port.transmitted.back();uint16_t address=uint16_t(tx[2])<<8|tx[3];uint8_t n=tx[5];
   uint32_t raw=address==p.powerFactor.address?(id==profiles::ACREL_ACR?95:950):address==p.frequency.address?5000:address==p.energy.address?(invalid?0x7FC00000UL:bits(id==profiles::ACREL_ACR?1234.5f:589000.f)):address==p.reactiveEnergy.address?bits(id==profiles::ACREL_ACR?22.5f:22500.f):123;
   std::vector<uint8_t> reply={tx[0],3,uint8_t(n*2)};if(n==2){reply.push_back(raw>>24);reply.push_back(raw>>16);}reply.push_back(raw>>8);reply.push_back(raw);uint16_t checksum=crc16(reply.data(),reply.size());reply.push_back(checksum);reply.push_back(checksum>>8);for(auto b:reply)port.incoming.push_back(b);
  };
  assert(p.primaryFloat==p.energy.address);assert(engine.readEnergy(engine.current));assert(port.transmitted.size()==4);
  const uint16_t order[]={p.powerFactor.address,p.frequency.address,p.energy.address,p.reactiveEnergy.address};
  for(unsigned i=0;i<4;++i){const auto &tx=port.transmitted[i];assert(tx[1]==3&&(uint16_t(tx[2])<<8|tx[3])==order[i]);}
  MeterReadings accepted;ReadingQuality q;auto read=[&](const profiles::Register &r,uint8_t *out){return engine.readRegisters(engine.current,r.address,r.words,out);};
  assert(readMeterReadings(p,read,accepted,&q));assert(fabs(accepted.meterEnergy-(id==profiles::ACREL_ACR?1234.5:589))<1e-8);
  invalid=true;assert(!readMeterReadings(p,read,accepted,&q)&&q.invalid==FIELD_ENERGY);invalid=false;
  assert(engine.discoveryOptions.count==2);DiscoveryConfig cfg=engine.discoveryOptions;cfg.first=cfg.last=1;cfg.responseMs=150;
  assert(engine.discovery.begin(cfg,88,millis(),9));while(engine.discovery.running())engine.discovery.step(engine,millis(),1793185200);
  MeterRegistry registry;registry.count=1;registry.entries[0].id=1;registry.entries[0].profile=id;registry.entries[0].evidence=engine.discovery.found[0];strcpy(registry.entries[0].name,"Panel");strcpy(registry.entries[0].model,p.key);
  Settings settings;settings.profile=id;assert(!productionProfileReady(settings,1,registry));registry.entries[0].verified=true;
  assert(productionProfileReady(settings,1,registry));assert(!productionProfileReady(settings,0,registry));settings.slave=2;assert(!productionProfileReady(settings,1,registry));
  engine.selectProfile(profiles::FORT);assert(engine.discoveryOptions.count==8);
 }
 {uint8_t negative[]={0xFF,0x9C};assert(fabs(profiles::decode(profiles::get(profiles::ACREL_ACR).power,negative)+.1)<1e-12);
  assert(std::isnan(profiles::primaryEnergy(profiles::ACREL_APM,0x7F800000,1,1,1)));
  assert(profiles::get(profiles::ACREL_APM).energy.address==3000&&profiles::get(profiles::ACREL_APM).current.address==1100);
 }
 assert(meter::signed32(0xFFFFFF9CUL)==-100);assert(meter::float32(0x3F80,0)==1.0f);assert(meter::float32(0,0x3F80)!=1.0f);
 std::cout<<"PASS: real ModbusMaster FC03 reference bytes, manual callback order, immediate length completion, CRC/ID/function/count/echo/overflow/exception rejection, timeout/cancel, UART recovery and decoding\n";
}
