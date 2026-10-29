#pragma once
#include <stdint.h>
#include "../lib/FortMeter/src/SerialSweep.h"
#include "../lib/FortMeter/src/MeterProfiles.h"

namespace fort {
enum Direction:uint8_t { DIRECTION_MANUAL=0,DIRECTION_AUTO=1 };
struct Settings { uint32_t magic=0x46525434; uint32_t baud=9600; uint8_t slave=1; char parity='N';uint8_t direction=DIRECTION_MANUAL,profile=profiles::FORT; };
static_assert(sizeof(Settings)==12,"Persisted settings layout changed");
inline bool validBaud(uint32_t b) { return b==1200||b==2400||b==4800||b==9600||b==19200||b==38400; }
inline bool validSettings(const Settings &s) {
  return s.magic==0x46525434&&profiles::valid(s.profile)&&s.baud<=profiles::get(s.profile).maxBaud&&s.direction<=DIRECTION_AUTO&&s.slave>=1&&s.slave<=247&&validBaud(s.baud)&&(s.parity=='N'||s.parity=='E'||s.parity=='O');
}
inline bool migrateLegacySettings(Settings &s){
  const uint32_t old=s.magic;
  if((old!=0x46525431&&old!=0x46525432&&old!=0x46525433)||s.slave<1||s.slave>247||!validBaud(s.baud)||(s.parity!='N'&&s.parity!='E'&&s.parity!='O'))return false;
  if(old==0x46525431)s.direction=DIRECTION_MANUAL;
  if(old!=0x46525433)s.profile=profiles::FORT;
  s.magic=0x46525434;return validSettings(s);
}

enum ScanPhase:uint8_t { SCAN_IDLE,SCAN_RUNNING,SCAN_FOUND,SCAN_CANCELLED,SCAN_EXHAUSTED };
inline const char *scanName(ScanPhase phase) {
  const char *names[]={"idle","running","found","cancelled","exhausted"};
  return phase<=SCAN_EXHAUSTED?names[phase]:"idle";
}
struct ScanProgress {
  ScanPhase phase=SCAN_IDLE;
  Settings candidate{},result{};
  uint16_t tested=0,total=0;
  uint8_t first=1,last=247;bool currentOnly=false;
  uint32_t elapsedMs=0,timeouts=0,crcErrors=0,rejected=0,exceptions=0,busBusy=0;
};
// Bounded, allocation-free search: preferred baud/parity and ID first, no duplicates.
class ScanController {
 public:
  ScanProgress progress{};
  bool running() const { return progress.phase==SCAN_RUNNING; }
  bool begin(uint8_t first,uint8_t last,const Settings &preferred,uint32_t now,bool currentOnly=false) {
    if(running()||first<1||last>247||first>last||!validSettings(preferred))return false;
    const uint32_t defaults[]={9600,19200,38400,4800,2400,1200};const char parities[]={'N','E','O'};
    uint32_t rates[6]={preferred.baud};char modes[3]={preferred.parity};uint8_t nr=1,nm=1,stops[]={1};
    if(!currentOnly){for(auto baud:defaults)if(baud!=preferred.baud&&baud<=profiles::get(preferred.profile).maxBaud)rates[nr++]=baud;for(auto parity:parities)if(parity!=preferred.parity)modes[nm++]=parity;}
    SerialTarget base;base.baud=preferred.baud;base.slave=preferred.slave;base.parity=preferred.parity;
    if(!sweep.begin(first,last,base,rates,nr,modes,nm,stops,1))return false;
    preferredDirection=preferred.direction;preferredProfile=preferred.profile;started=now;progress=ScanProgress{};progress.currentOnly=currentOnly;
    progress.first=first;progress.last=last;progress.total=uint16_t(sweep.total);
    progress.phase=SCAN_RUNNING;updateCandidate();return true;
  }
  void refresh(uint32_t now) { if(running())progress.elapsedMs=now-started; }
  void completeProbe(bool matched,uint32_t now) {
    if(!running())return;
    refresh(now);++progress.tested;
    if(matched){progress.result=progress.candidate;progress.phase=SCAN_FOUND;}
    else if(progress.tested==progress.total)progress.phase=SCAN_EXHAUSTED;
    else updateCandidate();
  }
  bool cancel(uint32_t now) {
    if(!running())return false;
    refresh(now);progress.phase=SCAN_CANCELLED;return true;
  }
  void reset(){if(!running())progress=ScanProgress{};}
 private:
  SerialSweep sweep;uint32_t started=0;uint8_t preferredDirection=0,preferredProfile=0;
  void updateCandidate(){SerialTarget link;if(!sweep.at(progress.tested,link))return;
    progress.candidate=Settings{};progress.candidate.baud=link.baud;progress.candidate.slave=link.slave;progress.candidate.parity=link.parity;
    progress.candidate.profile=preferredProfile;progress.candidate.direction=preferredDirection;
  }
};
inline bool validScanRatios(const uint8_t *p) {
  for(int i=0;i<3;++i){uint16_t v=(uint16_t(p[i*2])<<8)|p[i*2+1];if(!v||v>9999)return false;}
  return true;
}
} // namespace fort
