#pragma once
#include <stdint.h>
#include <initializer_list>
#include "energy.h"
#include "clock_guard.h"
namespace fort {
// Used by the real LittleFS writer and the host fault-injection tests.
template<class FS, class Path>
bool installArchive(FS &fs, const Path &temp, const Path &path, const Path &backup) {
  if(fs.exists(backup) && !fs.exists(path) && !fs.rename(backup,path)) return false;
  if(fs.exists(backup) && !fs.remove(backup)) return false;
  if(fs.exists(path) && !fs.rename(path,backup)) return false;
  if(!fs.rename(temp,path)) return false; // Keep backup recoverable on failure.
  if(fs.exists(backup)) fs.remove(backup); // Cleanup failure does not invalidate new data.
  return true;
}
// A corrupt canonical file must never replace the only valid backup.
// Keep rejected data under a bounded quarantine name for inspection.
template<class FS,class Path,class Validator>
bool installVerifiedArchive(FS &fs,const Path &temp,const Path &path,const Path &backup,Validator valid){
  if(!valid(temp))return false;
  for(const Path &candidate:{path,backup})if(fs.exists(candidate)&&!valid(candidate)){
    Path rejected=candidate+".bad";
    if(fs.exists(rejected)||!fs.rename(candidate,rejected))return false;
  }
  if(fs.exists(backup)&&!fs.exists(path)&&!fs.rename(backup,path))return false;
  if(fs.exists(backup)&&!fs.remove(backup))return false;
  if(fs.exists(path)&&!fs.rename(path,backup))return false;
  // Read the promoted record before removing the last validated backup.
  if(!fs.rename(temp,path)||!valid(path))return false;
  if(fs.exists(backup))fs.remove(backup);
  return true;
}
inline bool validLegacyDay(const Day &day){
  if(day.number<CLOCK_MIN_UTC/86400||day.number>CLOCK_MAX_UTC/86400)return false;
  for(const auto &slot:day.slots){
    const uint8_t *raw=reinterpret_cast<const uint8_t*>(&slot);
    if(raw[offsetof(Slot,hasStart)]>1||raw[offsetof(Slot,hasEnd)]>1||raw[offsetof(Slot,broken)]>1)return false;
    if(slot.quality>MISSING||!isfinite(slot.start)||!isfinite(slot.end)||!isfinite(slot.last)||slot.start<0||slot.end<0||slot.last<0)return false;
    if(slot.hasEnd&&!slot.hasStart)return false;
    if(slot.quality==COMPLETE&&(!slot.hasStart||!slot.hasEnd||slot.broken||slot.end<slot.start))return false;
    if(slot.quality==ACTIVE&&(!slot.hasStart||slot.hasEnd||slot.broken))return false;
    if((slot.quality==WAITING||slot.quality==MISSING)&&(slot.hasStart||slot.hasEnd))return false;
  }return true;
}
inline bool ratioPollDue(bool valid,uint32_t now,uint32_t lastSuccess,uint32_t lastAttempt) {
  return (!valid || uint32_t(now-lastSuccess)>=60000) &&
         (lastAttempt==0 || uint32_t(now-lastAttempt)>=5000);
}
}
