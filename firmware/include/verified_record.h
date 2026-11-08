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
// A verified NVS record that already matches the requested bytes needs no
// rewrite. Read errors or mismatches still trigger the original readback-
// verified write path, so transient failures remain recoverable.
template<class Backend> bool verifiedPutIfChanged(Backend &store,const char *key,
                                                  const void *bytes,void *verify,size_t size){
  if(store.getBytesLength(key)==size&&store.getBytes(key,verify,size)==size&&
     !memcmp(bytes,verify,size))return true;
  return verifiedPut(store,key,bytes,verify,size);
}
// Use bounded reads to verify an unchanged CSV before skipping its flash
// rewrite. Short reads and mismatches must never count as verified storage.
template<class Reader> bool verifiedFileMatches(Reader &file,const char *expected,size_t size){
  if(!expected||file.size()!=size)return false;
  uint8_t chunk[64];
  for(size_t offset=0;offset<size;){
    const size_t n=size-offset<sizeof(chunk)?size-offset:sizeof(chunk);
    if(file.read(chunk,n)!=n||memcmp(chunk,expected+offset,n))return false;
    offset+=n;
  }
  return true;
}
} // namespace fort
