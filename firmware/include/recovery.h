#pragma once
#include <stdint.h>
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
inline bool ratioPollDue(bool valid,uint32_t now,uint32_t lastSuccess,uint32_t lastAttempt) {
  return (!valid || uint32_t(now-lastSuccess)>=60000) &&
         (lastAttempt==0 || uint32_t(now-lastAttempt)>=5000);
}
}
