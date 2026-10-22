#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

namespace fort {
inline uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xffff;
  while (n--) { c ^= *p++; for (int i=0;i<8;++i) c = (c>>1)^((c&1)?0xa001:0); }
  return c;
}
inline uint32_t be32(const uint8_t *p) {
  return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}
enum Quality : uint8_t { WAITING, ACTIVE, COMPLETE, PARTIAL, MISSING };
inline const char *qualityName(uint8_t q) {
  const char *names[] = {"menunggu","berjalan","lengkap-estimasi","parsial","tidak-tersedia"};
  return q <= MISSING ? names[q] : "invalid";
}
constexpr int SLOT_COUNT=9;
constexpr uint16_t START_MIN[SLOT_COUNT]={360,390,420,450,480,510,990,1020,1050};
constexpr uint32_t MAX_GAP=15;
struct Slot {
  double start=0, end=0, last=0;
  uint8_t quality=WAITING;
  bool hasStart=false, hasEnd=false, broken=false;
};
struct Day {
  uint32_t number=0;
  Slot slots[SLOT_COUNT];
};
// All timestamps passed here are WIB-as-epoch, not UTC. No allocation/hardware.
class Tracker {
 public:
  Day day{};
  bool previous=false;
  uint32_t prevTime=0;
  double prevEnergy=0;
  void interrupt() {
    previous=false;
    for (auto &s:day.slots) if(s.quality==ACTIVE) { s.broken=true; s.quality=PARTIAL; }
  }
  void newDay(uint32_t n) { day=Day{}; day.number=n; previous=false; }
  void tick(uint32_t now) {
    for(int i=0;i<SLOT_COUNT;++i) {
      auto &s=day.slots[i]; uint32_t end=day.number*86400+START_MIN[i]*60+1800;
      if(now>end+MAX_GAP && !s.hasEnd) {
        uint8_t q=s.hasStart || s.quality==PARTIAL ? PARTIAL : MISSING;
        if(s.quality!=COMPLETE) s.quality=q;
      }
    }
  }
  bool sample(uint32_t now,double e) {
    if(!isfinite(e)||e<0||now/86400!=day.number) return false;
    // Reject backward time; caller must explicitly interrupt after a clock change.
    if(previous && now<=prevTime) return false;
    bool continuous=previous && now-prevTime<=MAX_GAP && e>=prevEnergy;
    for(int i=0;i<SLOT_COUNT;++i) {
      Slot &s=day.slots[i]; uint32_t a=day.number*86400+START_MIN[i]*60,b=a+1800;
      auto at=[&](uint32_t t) { return prevEnergy+(e-prevEnergy)*double(t-prevTime)/double(now-prevTime); };
      if(!s.hasStart && now==a) { s.start=e; s.last=e; s.hasStart=true; s.quality=ACTIVE; }
      if(!s.hasStart && continuous && prevTime<a && now>a) {
        s.start=at(a); s.hasStart=true; s.quality=ACTIVE;
      }
      bool intersects=previous && prevTime<b && now>a;
      if(intersects && !continuous && !s.hasEnd) s.broken=true;
      if(now>a && now<b && !s.hasEnd) {
        s.last=e;
        s.quality=s.hasStart&&!s.broken?ACTIVE:PARTIAL;
      }
      if(!s.hasEnd && s.hasStart && (now==b || (continuous && prevTime<b && now>b))) {
        s.end=now==b?e:at(b); s.last=s.end; s.hasEnd=true;
        s.quality=!s.broken&&s.end>=s.start?COMPLETE:PARTIAL;
      }
      // Starting late is partial, never silently a complete half-hour.
      if(!s.hasStart && now>a && now<b) { s.last=e; s.broken=true; s.quality=PARTIAL; }
    }
    prevTime=now; prevEnergy=e; previous=true; tick(now); return true;
  }
};
inline bool validFrame(const uint8_t *p,size_t n,uint8_t id,uint8_t words) {
  if(n!=size_t(5+2*words)||p[0]!=id||p[1]!=3||p[2]!=2*words) return false;
  uint16_t c=crc16(p,n-2); return p[n-2]==uint8_t(c)&&p[n-1]==uint8_t(c>>8);
}
}
