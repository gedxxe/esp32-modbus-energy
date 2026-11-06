// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Pure filename/retention policy. Never delete diagnostic, quarantined, or
// damaged records just because their filename begins with a day prefix.
#include "checkpoint_log.h"
#include <cstdint>
#include <cstring>

namespace fort {
constexpr uint32_t ARCHIVE_HISTORY_DAYS=31; // today plus 30 previous local days

enum class ArchiveNameKind : uint8_t { LegacyCSV, CaptureBinary };

// Exact filenames produced by saveArchive() and writeCaptureFile().
// Leading zeroes, alternate extensions, staging, and forensic copies are
// intentionally excluded. An optional leading slash matches LittleFS listings.
inline bool productionArchiveDay(const char *name,ArchiveNameKind kind,uint32_t &day){
  if(!name)return false;
  if(name[0]=='/')++name;
  const char prefix=kind==ArchiveNameKind::LegacyCSV?'d':'c';
  if(*name!=prefix)return false;
  ++name;
  if(*name<'1'||*name>'9')return false; // no zero-padded or empty day number
  uint32_t parsed=0;
  while(*name>='0'&&*name<='9'){
    const uint32_t digit=unsigned(*name-'0');
    if(parsed>(UINT32_MAX-digit)/10)return false;
    parsed=parsed*10+digit;++name;
  }
  const char *suffix=kind==ArchiveNameKind::LegacyCSV?".csv":".bin";
  if(std::strcmp(name,suffix)!=0){
    // Writer's verified backup. Never touch ".bad", ".tmp" or other variants.
    if(std::strncmp(name,suffix,std::strlen(suffix))!=0||
       std::strcmp(name+std::strlen(suffix),".bak")!=0)return false;
  }
  if(!validCaptureDayNumber(parsed))return false;
  day=parsed;return true;
}

// Deliberately return a single expiry day, never an age range. A corrupt RTC
// moving years forward cannot turn one rollover into a mass archive deletion.
inline uint32_t expiredProductionDay(uint32_t currentDay){
  if(!validCaptureDayNumber(currentDay)||currentDay<=ARCHIVE_HISTORY_DAYS)return 0;
  const uint32_t expired=currentDay-ARCHIVE_HISTORY_DAYS;
  return validCaptureDayNumber(expired)?expired:0;
}

// Only generated canonical paths are passed here (never directory entries).
// A failing removal leaves an old copy intact for review and future retries.
template<class FS,class Path>
void retireProductionCopies(FS &fs,const Path &canonical){
  if(fs.exists(canonical))fs.remove(canonical);
  const Path backup=canonical+".bak";
  if(fs.exists(backup))fs.remove(backup);
}
} // namespace fort
