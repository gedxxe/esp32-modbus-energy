// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace fort {
constexpr uint32_t NETWORK_RETRY_MS=30000,NETWORK_AP_GRACE_MS=15000,NETWORK_APPLY_DELAY_MS=8000;
enum NetworkMode:uint8_t { NETWORK_AP,NETWORK_CLIENT,NETWORK_DUAL };
inline const char *networkModeName(uint8_t mode){return mode==NETWORK_CLIENT?"client":mode==NETWORK_DUAL?"dual":"ap";}
struct NetworkSettings { uint8_t mode=NETWORK_AP;char ssid[33]={},password[65]={}; };
inline bool validNetwork(const NetworkSettings &s){
  const char *ssidEnd=static_cast<const char*>(memchr(s.ssid,0,sizeof(s.ssid)));
  const char *keyEnd=static_cast<const char*>(memchr(s.password,0,sizeof(s.password)));
  if(s.mode>NETWORK_DUAL||!ssidEnd||!keyEnd)return false;
  size_t ssidSize=ssidEnd-s.ssid,keySize=keyEnd-s.password;
  if(s.mode!=NETWORK_AP&&!ssidSize)return false;
  for(size_t i=0;i<ssidSize;++i)if(uint8_t(s.ssid[i])<32||uint8_t(s.ssid[i])==127)return false;
  if(!keySize)return true; // Deliberate open network, never inferred from an empty web field.
  if(keySize<8)return false;
  for(size_t i=0;i<keySize;++i){unsigned c=uint8_t(s.password[i]);
    if(keySize==64){if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}
    else if(c<32||c>126)return false;
  }
  return true;
}
constexpr size_t NETWORK_RECORD_SIZE=104;
inline uint16_t networkCRC(const uint8_t *p,size_t n){uint16_t c=0xFFFF;while(n--){c^=*p++;for(uint8_t bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xA001:0);}return c;}
inline void encodeNetwork(const NetworkSettings &s,uint8_t *bytes){
  memset(bytes,0,NETWORK_RECORD_SIZE);bytes[0]='F';bytes[1]='W';bytes[2]=1;bytes[3]=s.mode;
  memcpy(bytes+4,s.ssid,33);memcpy(bytes+37,s.password,65);
  uint16_t c=networkCRC(bytes,102);bytes[102]=uint8_t(c);bytes[103]=uint8_t(c>>8);
}
inline bool decodeNetwork(const uint8_t *bytes,size_t size,NetworkSettings &out){
  if(size!=NETWORK_RECORD_SIZE||bytes[0]!='F'||bytes[1]!='W'||bytes[2]!=1||networkCRC(bytes,102)!=(uint16_t(bytes[102])|(uint16_t(bytes[103])<<8)))return false;
  NetworkSettings s{};s.mode=bytes[3];memcpy(s.ssid,bytes+4,33);memcpy(s.password,bytes+37,65);
  if(!validNetwork(s))return false;
  out=s;return true;
}
// Millisecond decisions are independent of the driver and use unsigned elapsed time.
// AP stays available during every connection attempt and after a lost client link.
class NetworkPolicy {
 public:
  uint8_t mode=NETWORK_AP;bool connected=false,fallback=false;
  void begin(uint8_t value,uint32_t now){mode=value;connected=false;fallback=false;started=lastAttempt=now;connectedAt=0;}
  bool update(uint32_t now,bool ready){
    if(mode==NETWORK_AP){connected=false;fallback=false;return false;}
    if(ready){if(!connected)connectedAt=now;connected=true;fallback=false;started=now;return false;}
    if(connected)started=now;
    connected=false;fallback=uint32_t(now-started)>=NETWORK_RETRY_MS;
    if(uint32_t(now-lastAttempt)>=NETWORK_RETRY_MS){lastAttempt=now;return true;}
    return false;
  }
  bool apWanted(uint32_t now)const{return mode!=NETWORK_CLIENT||!connected||uint32_t(now-connectedAt)<NETWORK_AP_GRACE_MS;}
 private:uint32_t started=0,lastAttempt=0,connectedAt=0;
};
inline uint32_t chooseAPAddress(uint32_t station,uint32_t mask){
  if(station&&!mask)return 0;
  const uint32_t candidates[]={0xC0A80401,0xC0A80501,0x0A2A0001,0xAC172A01};
  for(uint32_t ip:candidates)if(!station||(ip&(mask&0xFFFFFF00UL))!=(station&(mask&0xFFFFFF00UL)))return ip;
  return 0; // Invalid/overbroad LAN mask cannot silently create overlapping routes.
}
} // namespace fort
