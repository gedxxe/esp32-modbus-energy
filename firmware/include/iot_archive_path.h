// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Pure helpers shared by the HTTPS uploader and native regressions.
#include "checkpoint_log.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace fort { namespace iot {
inline bool archivePath(uint32_t day,bool backup,char *path,size_t capacity){
  if(!path||!capacity||!validCaptureDayNumber(day))return false;
  // Must match main.cpp capturePath(): /c<local-day>.bin[.bak].
  int count=snprintf(path,capacity,"/c%lu.bin%s",
                     (unsigned long)day,backup?".bak":"");
  return count>0&&size_t(count)<capacity;
}
inline bool checkpointWindowProtected(uint32_t utc){
  if(!validClockEpoch(utc,CAPTURE_OFFSET))return false;
  constexpr uint32_t margin=15;
  const uint32_t day=uint32_t((uint64_t(utc)+CAPTURE_OFFSET)/86400);
  for(unsigned i=0;i<CAPTURE_COUNT;++i){
    uint32_t scheduled=captureTarget(day,i);
    if(uint64_t(utc)+margin>=scheduled&&
       uint64_t(utc)<=uint64_t(scheduled)+CAPTURE_WINDOW+margin)
      return true;
  }
  return false;
}
} } // namespace fort::iot
