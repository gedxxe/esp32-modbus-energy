// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "verified_record.h"
namespace fort {
// The inner payload keeps its existing codec/CRC. The envelope adds an explicit
// generation and a second verified bank without serializing C++ struct padding.
template<size_t PayloadSize> class SettingsBank {
 public:
  static constexpr size_t SIZE=PayloadSize+14;
  uint32_t generation=0;
  int8_t bank=-1;
  bool present=false,healthy=true,degraded=false;
  const uint8_t *payload()const{return saved+12;}
  template<class Backend,class Validator>
  bool load(Backend &store,const char *a,const char *b,Validator valid){
    generation=0;bank=-1;present=false;healthy=true;degraded=false;damaged=0;
    for(unsigned i=0;i<2;++i){
      const char *key=i?b:a;
      if(!store.isKey(key))continue;
      present=true;
      if(store.getBytesLength(key)!=SIZE||store.getBytes(key,next,SIZE)!=SIZE||
         memcmp(next,"SBK1",4)||next[4]!=1||next[5]||get16(next+6)!=PayloadSize||
         !get32(next+8)||crc(next,SIZE-2)!=get16(next+SIZE-2)||!valid(next+12,PayloadSize)){
        damaged|=uint8_t(1u<<i);degraded=true;continue;
      }
      uint32_t candidate=get32(next+8);
      if(bank>=0&&candidate==generation&&memcmp(saved,next,SIZE)){healthy=false;degraded=true;damaged=3;return false;}
      if(bank<0||int32_t(candidate-generation)>0){memcpy(saved,next,SIZE);generation=candidate;bank=int8_t(i);}
    }
    healthy=!present||bank>=0;return healthy;
  }
  template<class Backend>
  bool save(Backend &store,const char *a,const char *b,const uint8_t *bytes){
    uint32_t candidate=generation+1;if(!candidate)candidate=1;
    memset(next,0,SIZE);memcpy(next,"SBK1",4);next[4]=1;put16(next+6,PayloadSize);put32(next+8,candidate);
    memcpy(next+12,bytes,PayloadSize);put16(next+SIZE-2,crc(next,SIZE-2));
    const char *key=bank==0?b:a;
    unsigned target=bank==0?1:0;
    if(!verifiedPut(store,key,next,verify,SIZE)){healthy=false;damaged|=uint8_t(1u<<target);degraded=true;return false;}
    memcpy(saved,next,SIZE);generation=candidate;bank=int8_t(target);present=healthy=true;
    damaged&=uint8_t(~(1u<<target));degraded=damaged!=0;return true;
  }
 private:
  static_assert(PayloadSize<=65535,"Settings payload must fit the explicit length field");
  uint8_t saved[SIZE]={},next[SIZE]={},verify[SIZE]={};
  uint8_t damaged=0;
  static uint16_t get16(const uint8_t *p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
  static uint32_t get32(const uint8_t *p){uint32_t value=0;for(unsigned i=0;i<4;++i)value|=uint32_t(p[i])<<(8*i);return value;}
  static void put16(uint8_t *p,uint16_t value){p[0]=uint8_t(value);p[1]=uint8_t(value>>8);}
  static void put32(uint8_t *p,uint32_t value){for(unsigned i=0;i<4;++i)p[i]=uint8_t(value>>(8*i));}
  static uint16_t crc(const uint8_t *p,size_t size){uint16_t value=0xffff;while(size--){value^=*p++;for(unsigned i=0;i<8;++i)value=(value>>1)^((value&1)?0xa001:0);}return value;}
};
} // namespace fort
