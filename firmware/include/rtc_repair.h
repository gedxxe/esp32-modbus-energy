// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "energy.h"
#include "checkpoint_log.h"
#include "clock_guard.h"
#include <cstring>

namespace fort {
// Only a date record with NO measured or partially captured energy can be
// abandoned as the active day. Its archival copies are quarantined, not erased.
inline bool clockRecoveryEmptyLegacy(const Day &day,uint32_t wantedDay){
  if(!day.number||day.number<=wantedDay)return true;
  for(const Slot &s:day.slots){
    if(s.hasStart||s.hasEnd||s.broken||s.quality==ACTIVE||
       s.quality==COMPLETE||s.quality==PARTIAL||
       s.start!=0||s.end!=0||s.last!=0)return false;
  }
  return true;
}
inline bool clockRecoveryEmptyCapture(const CaptureDay &day,uint32_t wantedDay){
  if(!day.number||day.number<=wantedDay)return true;
  for(const MeterCapture &p:day.points){
    if(p.status!=CAPTURE_WAITING&&p.status!=CAPTURE_MISSED)return false;
    if(p.startedUTC||p.capturedUTC||p.raw||p.energy!=0)return false;
  }
  return true;
}
inline bool clockRecoveryAllowed(uint32_t epoch,uint32_t offset,
                                 const Day &legacy,const CaptureDay &current,
                                 const CaptureDay &pending,uint32_t cloudAckDay){
  if(!validClockEpoch(epoch,offset)||pending.number)return false;
  const uint32_t wantedDay=uint32_t((uint64_t(epoch)+offset)/86400);
  if(cloudAckDay>wantedDay)return false;
  const bool future=(legacy.number>wantedDay||current.number>wantedDay);
  return future&&clockRecoveryEmptyLegacy(legacy,wantedDay)&&
         clockRecoveryEmptyCapture(current,wantedDay);
}
// Recognize ONLY the producer's actual archive names (/cNNNN.bin[.bak]).
inline bool clockRecoveryCaptureFilename(const char *name,uint32_t &day){
  if(!name)return false;
  if(name[0]=='/')++name;
  if(*name++!='c'||*name<'0'||*name>'9')return false;
  uint32_t n=0;
  while(*name>='0'&&*name<='9'){
    if(n>100000)return false;
    n=n*10+unsigned(*name++-'0');
  }
  if(std::strcmp(name,".bin")&&std::strcmp(name,".bin.bak"))return false;
  if(!validCaptureDayNumber(n))return false;
  day=n;return true;
}
} // namespace fort
