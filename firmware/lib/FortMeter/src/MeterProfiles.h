// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace fort {
namespace profiles {
enum Id : uint8_t { FORT=0, ACREL_ADL400=1, COUNT=2 };
enum RatioLayout : uint8_t { PT_FRACTION_CT, PT_CT };
enum Encoding : uint8_t { U16, U32, I32 };
struct Register { uint16_t address; uint8_t words; Encoding encoding; double scale; };
struct Profile {
  uint8_t id; const char *key,*name,*terminals,*manual;
  Register current,voltage,power,frequency,energy;
  uint16_t ratios,primaryFloat; uint8_t ratioWords; RatioLayout ratioLayout;
  uint32_t maxBaud;
};
inline bool valid(uint8_t id){return id<COUNT;}
inline const Profile &get(uint8_t id){
  // PDU addresses, never 40001-style reference numbers. See docs/METER_PROFILES.md.
  static const Profile table[]={
    {FORT,"fort","Fort","58 / A · 59 / B","Fort supplied manual",
      {0x0058,2,U32,.0001},{0x0042,2,U32,.0001},{0x006A,2,I32,.0001},
      {0x0080,1,U16,.01},{0x0082,2,U32,.001},0x002C,0x008A,3,PT_FRACTION_CT,38400},
    {ACREL_ADL400,"acrel-adl400","Acrel ADL400","21 / A1 · 22 / B1","ADL400 V1.3",
      {0x0064,1,U16,.01},{0x0061,1,U16,.1},{0x016A,2,I32,.0001},
      {0x0077,1,U16,.01},{0x000A,2,U32,.01},0x008D,0,2,PT_CT,19200}
  };
  static_assert(sizeof(table)/sizeof(table[0])==COUNT,"Every profile ID needs a descriptor");
  return table[valid(id)?id:0];
}
inline bool parse(const char *key,uint8_t &id){for(uint8_t i=0;i<COUNT;++i)if(!strcmp(key,get(i).key)){id=i;return true;}return false;}
inline uint16_t word(const uint8_t *p){return uint16_t(p[0])<<8|p[1];}
inline uint32_t raw32(const uint8_t *p){return uint32_t(word(p))<<16|word(p+2);}
inline double decode(const Register &reg,const uint8_t *p){
  uint32_t raw=reg.words==1?word(p):raw32(p);
  double value=reg.encoding==I32&&(raw&0x80000000UL)?double(int64_t(raw)-4294967296LL):double(raw);
  return value*reg.scale;
}
inline bool ratios(const Profile &profile,const uint8_t *p,uint16_t &num,uint16_t &den,uint16_t &ct){
  if(profile.ratioLayout!=PT_FRACTION_CT&&profile.ratioLayout!=PT_CT)return false;
  num=word(p);den=profile.ratioLayout==PT_FRACTION_CT?word(p+2):1;ct=word(p+(profile.ratioLayout==PT_FRACTION_CT?4:2));
  return num>=1&&num<=9999&&den>=1&&den<=9999&&ct>=1&&ct<=9999;
}
inline double primaryEnergy(uint8_t id,uint32_t raw,uint16_t num,uint16_t den,uint16_t ct){return double(raw)*get(id).energy.scale*double(num)/den*ct;}
} // namespace profiles
} // namespace fort
