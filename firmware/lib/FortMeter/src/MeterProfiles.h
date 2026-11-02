// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
namespace fort {
enum ReadingField:uint8_t { FIELD_PF=1,FIELD_FREQUENCY=2,FIELD_ENERGY=4,FIELD_REACTIVE=8 };
namespace profiles {
// IDs are stored in settings, checkpoints and the named-device registry.
enum Id : uint8_t { FORT=0, ACREL_ADL400=1, HABUER=2, ACREL_ACR=3, ACREL_APM=4, COUNT=5 };
enum RatioLayout : uint8_t { PT_FRACTION_CT, PT_CT, NO_RATIO };
enum Encoding : uint8_t { U16, U32, I32, F32, I16 };
struct Register { uint16_t address; uint8_t words; Encoding encoding; double scale; };
struct Profile {
  uint8_t id; const char *key,*name,*terminals,*manual;
  Register current,voltage,power,frequency,energy;
  uint16_t ratios,primaryFloat; uint8_t ratioWords; RatioLayout ratioLayout;
  uint32_t maxBaud;
  Register probe,powerFactor,reactiveEnergy;
  uint8_t requiredReadings; bool commissioningRequired;
};
inline bool valid(uint8_t id){return id<COUNT;}
inline bool available(const Register &reg){return reg.words==1||reg.words==2;}
inline bool coherentSnapshot(const Profile &profile){return profile.requiredReadings!=FIELD_ENERGY;}
inline bool directEnergy(const Profile &profile){return profile.ratioLayout==NO_RATIO;}
inline const Profile &get(uint8_t id){
  // PDU starting addresses. Habuer parameters require separate two-word requests.
  static const Profile table[]={
    {FORT,"fort","Fort","58 / A · 59 / B","Fort supplied manual",
      {0x0058,2,U32,.0001},{0x0042,2,U32,.0001},{0x006A,2,I32,.0001},
      {0x0080,1,U16,.01},{0x0082,2,U32,.001},0x002C,0x008A,3,PT_FRACTION_CT,38400,
      {0x0058,2,U32,.0001},{0,0,U16,0},{0,0,U16,0},FIELD_ENERGY,false},
    {ACREL_ADL400,"acrel-adl400","Acrel ADL400","21 / A1 · 22 / B1","ADL400 V1.3",
      {0x0064,1,U16,.01},{0x0061,1,U16,.1},{0x016A,2,I32,.0001},
      {0x0077,1,U16,.01},{0x000A,2,U32,.01},0x008D,0,2,PT_CT,19200,
      {0x0064,1,U16,.01},{0,0,U16,0},{0,0,U16,0},FIELD_ENERGY,false},
    {HABUER,"habuer","Habuer","A / B · use meter labels","User-tested Habuer reference sketch",
      {0,0,U16,0},{0,0,U16,0},{0,0,U16,0},
      {0x0032,2,F32,1},{0x0034,2,U32,.1},0,0,0,NO_RATIO,38400,
      {0x0030,2,F32,1},{0x0030,2,F32,1},{0x0035,2,U32,.1},FIELD_PF|FIELD_FREQUENCY|FIELD_ENERGY|FIELD_REACTIVE,false},
    {ACREL_ACR,"acrel-acr","Acrel ACR10R-DxxTE","A / B labels","ACR PDF p15: energy unit/order unverified",
      {0x64,1,U16,.01},{0x61,1,U16,.1},{0x67,1,I16,.001},
      {0x77,1,U16,.01},{0x47,2,F32,1},0,0x47,0,NO_RATIO,38400,
      {0x64,1,U16,.01},{0x73,1,U16,.01},{0x4B,2,F32,1},FIELD_PF|FIELD_FREQUENCY|FIELD_ENERGY|FIELD_REACTIVE,true},
    {ACREL_APM,"acrel-apm","Acrel APM800/801/810","A / B labels","APM PDF p63 table; p74 conflicting example",
      {1100,2,I32,.001},{1120,2,I32,.1},{1156,2,F32,.00001},
      {252,1,I16,.01},{3000,2,F32,.001},0,3000,0,NO_RATIO,38400,
      {252,1,I16,.01},{1183,1,I16,.001},{3004,2,F32,.001},FIELD_PF|FIELD_FREQUENCY|FIELD_ENERGY|FIELD_REACTIVE,true}
  };
  static_assert(sizeof(table)/sizeof(table[0])==COUNT,"Every profile ID needs a descriptor");
  static const Profile unknown={255,"unknown","Unconfigured meter","A / B","No verified register profile",
    {0,0,U16,0},{0,0,U16,0},{0,0,U16,0},{0,0,U16,0},{0,0,U16,0},0,0,0,NO_RATIO,38400,
    {0,0,U16,0},{0,0,U16,0},{0,0,U16,0},FIELD_ENERGY,false};
  return valid(id)?table[id]:unknown;
}
inline bool parse(const char *key,uint8_t &id){for(uint8_t i=0;i<COUNT;++i)if(!strcmp(key,get(i).key)){id=i;return true;}return false;}
inline uint16_t word(const uint8_t *p){return uint16_t(p[0])<<8|p[1];}
inline uint32_t raw32(const uint8_t *p){return uint32_t(word(p))<<16|word(p+2);}
inline double decode(const Register &reg,const uint8_t *p){
  if(!p||!available(reg))return NAN;
  uint32_t raw=reg.words==1?word(p):raw32(p);
  if(reg.encoding==F32){if(reg.words!=2)return NAN;float value;static_assert(sizeof(value)==sizeof(raw),"IEEE754 binary32 required");memcpy(&value,&raw,sizeof(value));return double(value)*reg.scale;}
  if(reg.encoding==I16)return double(raw&0x8000?int32_t(raw)-65536:int32_t(raw))*reg.scale;
  double value=reg.encoding==I32&&(raw&0x80000000UL)?double(int64_t(raw)-4294967296LL):double(raw);
  return value*reg.scale;
}
inline bool ratios(const Profile &profile,const uint8_t *p,uint16_t &num,uint16_t &den,uint16_t &ct){
  if(directEnergy(profile)){num=den=ct=1;return true;}
  if(!p||(profile.ratioLayout!=PT_FRACTION_CT&&profile.ratioLayout!=PT_CT))return false;
  num=word(p);den=profile.ratioLayout==PT_FRACTION_CT?word(p+2):1;ct=word(p+(profile.ratioLayout==PT_FRACTION_CT?4:2));
  return num>=1&&num<=9999&&den>=1&&den<=9999&&ct>=1&&ct<=9999;
}
inline double primaryEnergy(uint8_t id,uint32_t raw,uint16_t num,uint16_t den,uint16_t ct){if(!valid(id))return NAN;const auto &p=get(id);if(p.energy.encoding==F32){uint8_t bytes[]={uint8_t(raw>>24),uint8_t(raw>>16),uint8_t(raw>>8),uint8_t(raw)};double value=decode(p.energy,bytes);return !(raw&0x80000000UL)&&isfinite(value)?value:NAN;}
if(directEnergy(p)){return double(raw)*p.energy.scale;}return double(raw)*p.energy.scale*double(num)/den*ct;}
// Float32 counters retain their IEEE754 payload in the existing raw 32-bit slot.
inline double energyResolution(const Profile &p,uint32_t raw){
  if(p.energy.encoding!=F32)return p.energy.scale;
  float value;memcpy(&value,&raw,4);float next=nextafterf(value,INFINITY);
  return isfinite(value)&&isfinite(next)?double(next-value)*p.energy.scale:0;
}
// Shared bounded query construction for diagnostics and manufacturer-neutral surveys.
template<class Query> inline bool appendQuery(Query *out,size_t &count,size_t capacity,uint16_t address,uint8_t words){
  if(!words)return true;
  for(size_t i=0;i<count;++i)if(out[i].address==address&&out[i].words==words)return true;
  if(count==capacity)return false;
  out[count].address=address;out[count++].words=words;return true;
}
template<class Query> inline size_t discoveryQueries(const Profile &p,Query *out,size_t capacity){
  size_t n=0;appendQuery(out,n,capacity,p.probe.address,p.probe.words);appendQuery(out,n,capacity,p.energy.address,p.energy.words);appendQuery(out,n,capacity,p.ratios,p.ratioWords);return n;
}
template<class Query> inline size_t auditQueries(const Profile &p,Query *out,size_t capacity){
  size_t n=0;const Register registers[]={p.probe,p.current,p.voltage,p.power,p.powerFactor,p.frequency,p.energy,p.reactiveEnergy};
  for(const auto &r:registers)appendQuery(out,n,capacity,r.address,r.words);
  if(p.primaryFloat)appendQuery(out,n,capacity,p.primaryFloat,2);else appendQuery(out,n,capacity,p.ratios,p.ratioWords);
  return n;
}
} // namespace profiles
} // namespace fort
