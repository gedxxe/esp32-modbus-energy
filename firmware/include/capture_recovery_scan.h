// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "archive_retention.h"
#include <cstdint>

namespace fort {
// Preserve up to 64 newest unique days regardless of directory listing order.
// An exhausted directory scan must be marked incomplete rather than silently
// presented as a safe and complete recovery.
struct CaptureRecoveryIndex {
  static constexpr unsigned MaxEntries=4096;
  static constexpr unsigned MaxDays=64;
  uint32_t days[MaxDays]{};
  unsigned count=0,inspected=0;
  bool complete=true;
  bool observe(const char *name){
    if(inspected>=MaxEntries){complete=false;return false;}
    ++inspected;
    uint32_t day=0;
    if(!productionArchiveDay(name,ArchiveNameKind::CaptureBinary,day))return true;
    for(unsigned i=0;i<count;++i)if(days[i]==day)return true;
    unsigned pos=0;
    while(pos<count&&days[pos]>day)++pos;
    if(pos>=MaxDays)return true;
    if(count<MaxDays)++count;
    for(unsigned i=count-1;i>pos;--i)days[i]=days[i-1];
    days[pos]=day;
    return true;
  }
};
} // namespace fort
