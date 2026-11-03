// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace fort {
template<class Backend> bool verifiedPut(Backend &store,const char *key,const void *bytes,void *verify,size_t size){
  return store.putBytes(key,bytes,size)==size&&store.getBytesLength(key)==size&&
    store.getBytes(key,verify,size)==size&&!memcmp(bytes,verify,size);
}
} // namespace fort
