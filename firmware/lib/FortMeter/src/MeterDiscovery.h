// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SerialSweep.h"
#include "MeterProfiles.h"
#include <string.h>
#include <initializer_list>
namespace fort {
constexpr uint8_t DISCOVERY_PROBES=8,DISCOVERY_RESULTS=4,UNKNOWN_PROFILE=255;
struct DiscoveryQuery {uint16_t address;uint8_t words;DiscoveryQuery(uint16_t a=0,uint8_t w=1):address(a),words(w){}};
struct DiscoveryConfig {
  SerialTarget preferred{};uint8_t first=1,last=16,direction=0,count=0;bool matrix=false;uint16_t responseMs=350;
  DiscoveryQuery plan[DISCOVERY_PROBES];
  DiscoveryConfig(){
    plan[0].address=0;plan[0].words=1;count=1;
    for(uint8_t id=0;id<profiles::COUNT;++id){const auto &p=profiles::get(id);const DiscoveryQuery queries[]={{p.current.address,p.current.words},{p.energy.address,p.energy.words},{p.ratios,p.ratioWords}};
      for(const auto &query:queries){bool duplicate=false;for(unsigned i=0;i<count;++i)if(plan[i].address==query.address&&plan[i].words==query.words)duplicate=true;if(!duplicate&&count<DISCOVERY_PROBES)plan[count++]=query;}
    }
  }
};
inline bool validDiscoveryConfig(const DiscoveryConfig &c){
  if(!c.first||c.first>c.last||c.last>247||c.direction>1||!c.count||c.count>DISCOVERY_PROBES||c.responseMs<150||c.responseMs>3000||
    c.preferred.baud<1200||c.preferred.baud>1000000||c.preferred.slave<1||c.preferred.slave>247||(c.preferred.parity!='N'&&c.preferred.parity!='E'&&c.preferred.parity!='O')||(c.preferred.stops!=1&&c.preferred.stops!=2))return false;
  for(unsigned i=0;i<c.count;++i){if(!c.plan[i].words||c.plan[i].words>3||uint32_t(c.plan[i].address)+c.plan[i].words>65536UL)return false;for(unsigned j=0;j<i;++j)if(c.plan[i].address==c.plan[j].address&&c.plan[i].words==c.plan[j].words)return false;}return true;
}
inline bool parseDiscoveryPlan(const char *text,DiscoveryConfig &config){
  if(!text||strlen(text)>63)return false;
  DiscoveryConfig next=config;next.count=0;const char *p=text;
  while(*p){
    if(next.count==DISCOVERY_PROBES)return false;
    uint32_t address=0;unsigned digits=0;
    while(*p&&*p!=':'){char c=*p++;unsigned digit=c>='0'&&c<='9'?unsigned(c-'0'):c>='a'&&c<='f'?unsigned(c-'a'+10):c>='A'&&c<='F'?unsigned(c-'A'+10):16;if(digit>15||++digits>4)return false;address=(address<<4)|digit;}
    if(!digits||*p++!=':'||*p<'1'||*p>'3')return false;
    uint8_t words=uint8_t(*p++-'0');if(address+words>65536UL)return false;
    for(unsigned i=0;i<next.count;++i)if(next.plan[i].address==address&&next.plan[i].words==words)return false;
    next.plan[next.count].address=uint16_t(address);next.plan[next.count++].words=words;
    if(!*p)break;
    if(*p++!=','||!*p)return false;
  }
  if(!next.count)return false;
  config=next;return true;
}
struct ProbeEvidence {
  uint16_t address=0,durationMs[2]={};uint8_t words=0,attempts=0,status[2]={0xE2,0xE2},validMask=0,flags=0,rxSize[2]={},txSize[2]={};
  uint8_t tx[2][8]={},rx[2][16]={};
  bool confirmed()const{return (validMask&3)==3;}
  bool data()const{return confirmed()&&status[0]==0&&status[1]==0;}
};
struct DiscoveryDevice {
  SerialTarget link{};uint8_t direction=0,count=0;uint32_t boot=0,run=0,utc=0,elapsedMs=0;
  uint32_t requests=0,timeouts=0,crcErrors=0,rejected=0,exceptions=0;
  ProbeEvidence probes[DISCOVERY_PROBES];
  bool confirmed()const{for(unsigned i=0;i<count;++i)if(probes[i].confirmed())return true;return false;}
  bool dataAt(uint16_t address,uint8_t words)const{for(unsigned i=0;i<count;++i)if(probes[i].address==address&&probes[i].words==words&&probes[i].data())return true;return false;}
  uint8_t mapEvidence()const{
    uint8_t mask=0;for(uint8_t id=0;id<profiles::COUNT&&id<8;++id){const auto &p=profiles::get(id);if(dataAt(p.current.address,p.current.words)&&dataAt(p.energy.address,p.energy.words)&&dataAt(p.ratios,p.ratioWords))mask|=uint8_t(1<<id);}return mask;
  }
};
enum DiscoveryPhase:uint8_t { DISCOVERY_IDLE,DISCOVERY_RUNNING,DISCOVERY_COMPLETE,DISCOVERY_CANCELLED,DISCOVERY_CAPACITY };
inline const char *discoveryName(uint8_t phase){const char *names[]={"idle","running","complete","cancelled","capacity"};return phase<=DISCOVERY_CAPACITY?names[phase]:"invalid";}
class MeterDiscovery {
 public:
  DiscoveryConfig config{};DiscoveryDevice found[DISCOVERY_RESULTS]{},current{};
  uint8_t phase=DISCOVERY_IDLE,count=0,query=0;uint32_t run=0,tested=0,total=0,elapsedMs=0,requests=0,timeouts=0,crcErrors=0,rejected=0,exceptions=0;
  bool inFlight=false;
  bool running()const{return phase==DISCOVERY_RUNNING;}
  bool begin(const DiscoveryConfig &c,uint32_t runId,uint32_t now,uint32_t boot){
    if(running()||!runId||!validDiscoveryConfig(c))return false;
    uint32_t rates[8]={c.preferred.baud};char parities[3]={c.preferred.parity};uint8_t stops[2]={c.preferred.stops},nr=1,np=1,ns=1;
    const uint32_t defaults[]={9600,19200,38400,4800,2400,1200,57600,115200};
    if(c.matrix){bool standard=false;for(auto rate:defaults)if(rate==c.preferred.baud)standard=true;nr=0;if(standard)rates[nr++]=c.preferred.baud;for(auto rate:defaults)if(rate!=c.preferred.baud)rates[nr++]=rate;for(char parity:{'N','E','O'})if(parity!=c.preferred.parity)parities[np++]=parity;stops[ns++]=c.preferred.stops==1?2:1;}
    SerialSweep next;if(!next.begin(c.first,c.last,c.preferred,rates,nr,parities,np,stops,ns))return false;
    *this=MeterDiscovery{};sweep=next;config=c;run=runId;started=now;bootId=boot;total=sweep.total*c.count;phase=DISCOVERY_RUNNING;prepare();return true;
  }
  void cancel(){if(running())phase=DISCOVERY_CANCELLED;}
  void refresh(uint32_t now){if(running())elapsedMs=now-started;}
  template<class Reader> void step(Reader &reader,uint32_t now,uint32_t utc){
    if(!running())return;
    struct FlightGuard{bool &flag;explicit FlightGuard(bool &f):flag(f){flag=true;}~FlightGuard(){flag=false;}} guard(inFlight);
    refresh(now);ProbeEvidence &probe=current.probes[query];probe.address=config.plan[query].address;probe.words=config.plan[query].words;current.count=query+1;
    auto link=reader.current;link.id=current.link.slave;link.baud=current.link.baud;link.parity=current.link.parity;link.stops=current.link.stops;
    uint32_t timeout=reader.options.responseMs;reader.options.responseMs=config.responseMs;
    for(unsigned attempt=0;attempt<2&&running()&&!reader.abortRequested;++attempt){
      uint8_t data[6]={};uint32_t before=reader.clockMs();bool accepted=reader.readRegisters(link,probe.address,probe.words,data);
      const auto &frame=reader.transport.last;probe.durationMs[attempt]=uint16_t(reader.clockMs()-before);probe.status[attempt]=reader.lastStatus;probe.attempts=attempt+1;
      probe.txSize[attempt]=frame.txSize>8?8:frame.txSize;memcpy(probe.tx[attempt],frame.tx,probe.txSize[attempt]);
      if(frame.overflow||frame.rxSize>16)probe.flags|=uint8_t(1<<attempt);
      probe.rxSize[attempt]=frame.rxSize>16?16:frame.rxSize;memcpy(probe.rx[attempt],frame.rx,probe.rxSize[attempt]);
      ++requests;++current.requests;
      if(accepted||frame.exception){probe.validMask|=uint8_t(1<<attempt);if(frame.exception){++exceptions;++current.exceptions;}}
      else if(frame.status==0xE2){++timeouts;++current.timeouts;}
      else if(frame.status==0xE3){++crcErrors;++current.crcErrors;}
      else{++rejected;++current.rejected;}
      if(!accepted&&!frame.exception)break;
    }
    reader.options.responseMs=timeout;refresh(reader.clockMs());
    if(reader.abortRequested){cancel();return;}if(!running())return;
    ++tested;++query;
    if(query<config.count)return;
    current.utc=utc;current.elapsedMs=elapsedMs;
    if(current.confirmed()){found[count++]=current;if(count==DISCOVERY_RESULTS){phase=DISCOVERY_CAPACITY;return;}}
    ++candidate;if(candidate>=sweep.total){phase=DISCOVERY_COMPLETE;return;}prepare();
  }
 private:
  SerialSweep sweep{};uint32_t started=0,bootId=0,candidate=0;
  void prepare(){current=DiscoveryDevice{};sweep.at(candidate,current.link);current.direction=config.direction;current.run=run;current.boot=bootId;query=0;}
};
} // namespace fort
