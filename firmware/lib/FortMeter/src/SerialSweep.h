// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
namespace fort {
struct SerialTarget {uint32_t baud=9600;uint8_t slave=1;char parity='N';uint8_t stops=1;};
// Enumerates the preferred ID first, then all remaining IDs without duplicates.
class SerialSweep {
 public:
  uint8_t first=1,last=1,rateCount=0,modeCount=0,stopCount=0;uint32_t total=0;
  uint32_t rates[8]={};char modes[3]={};uint8_t stops[2]={};SerialTarget preferred{};
  bool begin(uint8_t lo,uint8_t hi,const SerialTarget &p,const uint32_t *ratesIn,uint8_t nr,const char *modesIn,uint8_t nm,const uint8_t *stopsIn,uint8_t ns){
    if(!lo||hi>247||lo>hi||!nr||nr>8||!nm||nm>3||!ns||ns>2)return false;
    SerialSweep plan;plan.first=lo;plan.last=hi;plan.preferred=p;plan.rateCount=nr;plan.modeCount=nm;plan.stopCount=ns;
    for(uint8_t i=0;i<nr;++i){if(ratesIn[i]<1200||ratesIn[i]>1000000)return false;for(uint8_t j=0;j<i;++j)if(ratesIn[j]==ratesIn[i])return false;plan.rates[i]=ratesIn[i];}
    for(uint8_t i=0;i<nm;++i){if(modesIn[i]!='N'&&modesIn[i]!='E'&&modesIn[i]!='O')return false;for(uint8_t j=0;j<i;++j)if(modesIn[j]==modesIn[i])return false;plan.modes[i]=modesIn[i];}
    for(uint8_t i=0;i<ns;++i){if(stopsIn[i]!=1&&stopsIn[i]!=2)return false;for(uint8_t j=0;j<i;++j)if(stopsIn[j]==stopsIn[i])return false;plan.stops[i]=stopsIn[i];}
    plan.total=uint32_t(hi-lo+1)*nr*nm*ns;*this=plan;return true;
  }
  bool at(uint32_t index,SerialTarget &out)const{
    if(index>=total||!total)return false;
    uint32_t count=last-first+1,combos=uint32_t(rateCount)*modeCount*stopCount;
    uint32_t id=first+index%count,mode=index/count;
    if(preferred.slave>=first&&preferred.slave<=last){
      if(index<combos){id=preferred.slave;mode=index;}
      else{uint32_t remaining=count-1;if(!remaining)return false;uint32_t offset=index-combos;mode=offset/remaining;id=first+offset%remaining;if(id>=preferred.slave)++id;}
    }
    out.slave=uint8_t(id);out.stops=stops[mode%stopCount];mode/=stopCount;out.parity=modes[mode%modeCount];out.baud=rates[mode/modeCount];return true;
  }
};
} // namespace fort
