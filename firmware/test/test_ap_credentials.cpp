// SPDX-License-Identifier: GPL-3.0-only
#include "ap_credentials.h"
#include <assert.h>
#include <map>
#include <string>
#include <vector>
#include <iostream>
using namespace fort;
struct Store {
  std::map<std::string, std::vector<uint8_t>> data;
  bool writeOK=true;
  size_t getBytesLength(const char *key) { return data.count(key)?data[key].size():0; }
  size_t getBytes(const char *key, void *dest, size_t n) {
    if (!data.count(key) || data[key].size()!=n) return 0;
    memcpy(dest,data[key].data(),n);return n;
  }
  size_t putBytes(const char *key,const void *src,size_t n) {
    if (!writeOK) return 0;
    auto bytes=static_cast<const uint8_t*>(src);data[key]=std::vector<uint8_t>(bytes,bytes+n);return n;
  }
};
int main() {
  Store store;char first[AP_PASSWORD_BUFFER]={},second[AP_PASSWORD_BUFFER]={};
  unsigned entropyCalls=0;
  auto fill=[&](uint8_t *dest,size_t n){++entropyCalls;for(size_t i=0;i<n;++i)dest[i]=uint8_t(i+entropyCalls-1);};
  assert(loadOrCreateAPPassword(store,fill,first));
  assert(validAPPassword(first));
  assert(std::string(first)=="000102030405060708090a0b0c0d0e0f");
  assert(loadOrCreateAPPassword(store,fill,second));
  assert(entropyCalls==1&&!strcmp(first,second)); // reboot must retain password
  store.data["psk"]={1,2,3}; // corrupted / old record forces rotation
  assert(loadOrCreateAPPassword(store,fill,second));
  assert(entropyCalls==2&&strcmp(first,second));
  store.writeOK=false;store.data["psk"]={9};char ephemeral[AP_PASSWORD_BUFFER]={};
  assert(!loadOrCreateAPPassword(store,fill,ephemeral));
  assert(validAPPassword(ephemeral)&&strcmp(ephemeral,second));
  ephemeral[5]='z';assert(!validAPPassword(ephemeral));
  std::cout<<"PASS: unique AP PSK generation, persistent readback, corruption rotation, fail-closed ephemeral fallback\n";
}
