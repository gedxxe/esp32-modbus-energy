// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "scan.h"
#include "energy.h"
namespace fort {
constexpr size_t METER_CONFIG_BYTES=16;
inline void meterPut16(uint8_t *p,uint16_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);}
inline void meterPut32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
inline uint16_t meterGet16(const uint8_t *p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
inline uint32_t meterGet32(const uint8_t *p){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(p[i])<<(8*i);return n;}
inline bool sameMeterSettings(const Settings &a,const Settings &b){return a.profile==b.profile&&a.slave==b.slave&&a.baud==b.baud&&a.parity==b.parity&&a.direction==b.direction;}
inline void encodeMeterConfig(const Settings &s,uint16_t savedId,uint8_t *p){
  meterPut32(p,s.magic);meterPut32(p+4,s.baud);p[8]=s.slave;p[9]=uint8_t(s.parity);p[10]=s.direction;p[11]=s.profile;meterPut16(p+12,savedId);meterPut16(p+14,crc16(p,14));
}
inline bool decodeMeterConfig(const uint8_t *p,size_t size,Settings &out,uint16_t &savedId){
  if(!p||size!=METER_CONFIG_BYTES)return false;
  Settings s;s.magic=meterGet32(p);s.baud=meterGet32(p+4);s.slave=p[8];s.parity=char(p[9]);s.direction=p[10];s.profile=p[11];
  uint16_t id=0;
  if(s.magic==0x46525434){if(meterGet16(p+14)!=crc16(p,14)||!validSettings(s))return false;id=meterGet16(p+12);}
  else{if(meterGet16(p+12)!=crc16(p,12)||!migrateLegacySettings(s))return false;}
  out=s;savedId=id;return true;
}
} // namespace fort
