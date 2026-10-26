// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace fort {
// Nonblocking writer: positive=progress, zero=retry, negative=fatal error.
// The start belongs to the entire HTTP request, so partial progress never extends
// the deadline. Service yields/feed watchdog between every bounded chunk.
template<class Clock,class Ready,class Writer,class Service>
size_t boundedWrite(const char *data,size_t size,uint32_t started,uint32_t budgetMs,
                    Clock now,Ready ready,Writer write,Service service){
  size_t sent=0;
  while(sent<size&&uint32_t(now()-started)<budgetMs&&ready()){
    size_t chunk=size-sent;if(chunk>1024)chunk=1024;
    int count=write(data+sent,chunk);
    if(count<0||size_t(count)>chunk)break;
    sent+=size_t(count);service();
  }
  return sent;
}
} // namespace fort
