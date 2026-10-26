#pragma once
#include <stdint.h>

namespace fort {
struct Settings { uint32_t magic=0x46525431; uint32_t baud=9600; uint8_t slave=1; char parity='N'; };
static_assert(sizeof(Settings)==12,"Persisted settings layout changed");
inline bool validBaud(uint32_t b) { return b==1200||b==2400||b==4800||b==9600||b==19200||b==38400; }
inline bool validSettings(const Settings &s) {
  return s.magic==0x46525431&&s.slave>=1&&s.slave<=247&&validBaud(s.baud)&&(s.parity=='N'||s.parity=='E'||s.parity=='O');
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
    const uint32_t defaults[]={9600,19200,38400,4800,2400,1200};
    const char parities[]={'N','E','O'};
    bauds[0]=preferred.baud;int i=1;for(auto b:defaults)if(b!=preferred.baud)bauds[i++]=b;
    modes[0]=preferred.parity;i=1;for(auto p:parities)if(p!=preferred.parity)modes[i++]=p;
    preferredID=preferred.slave;started=now;progress=ScanProgress{};
    combinations=currentOnly?1:18;progress.currentOnly=currentOnly;
    progress.first=first;progress.last=last;progress.total=(last-first+1)*combinations;
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
 private:
  uint32_t bauds[6]{},started=0;
  char modes[3]{};
  uint8_t preferredID=1,combinations=18;
  void updateCandidate() {
    uint16_t count=progress.last-progress.first+1,index=progress.tested%count,combo=progress.tested/count;
    uint16_t id=progress.first+index;
    if(preferredID>=progress.first&&preferredID<=progress.last) {
      // Fast pass: all 18 serial modes at the configured ID before other IDs.
      if(progress.tested<combinations){id=preferredID;combo=progress.tested;}
      else {
        uint16_t remaining=count-1,offset=progress.tested-combinations;
        combo=offset/remaining;id=progress.first+offset%remaining;if(id>=preferredID)++id;
      }
    }
    progress.candidate=Settings{};progress.candidate.baud=bauds[combo/3];
    progress.candidate.parity=modes[combo%3];progress.candidate.slave=id;
  }
};
inline bool validScanRatios(const uint8_t *p) {
  for(int i=0;i<3;++i){uint16_t v=(uint16_t(p[i*2])<<8)|p[i*2+1];if(!v||v>9999)return false;}
  return true;
}
} // namespace fort
