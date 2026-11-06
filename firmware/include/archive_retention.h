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

// Delete only the *exact* expired production day after the next day is saved.
// A bogus RTC jump by years may not mass-delete older archives.
inline bool productionArchiveExpired(const char *name,ArchiveNameKind kind,
                                     uint32_t currentDay){
  uint32_t archiveDay=0;
  return validCaptureDayNumber(currentDay)&&
         productionArchiveDay(name,kind,archiveDay)&&
         uint64_t(archiveDay)+ARCHIVE_HISTORY_DAYS==currentDay;
}
} // namespace fort
