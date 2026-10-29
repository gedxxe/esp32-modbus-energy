// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "meter_config.h"
#include "trace.h"
#include "../lib/FortMeter/src/MeterDiscovery.h"
namespace fort {
constexpr uint8_t SAVED_METER_LIMIT=4;
constexpr size_t DEVICE_EVIDENCE_BYTES=560,SAVED_METER_BYTES=700,METER_REGISTRY_BYTES=16+SAVED_METER_LIMIT*SAVED_METER_BYTES+2;
struct SavedMeter {
  uint16_t id=0;uint8_t profile=UNKNOWN_PROFILE;bool verified=false;
  char name[32]={},model[40]={},notes[64]={};DiscoveryDevice evidence{};
};
struct MeterRegistry {
  uint32_t generation=0;uint16_t nextId=1;uint8_t count=0;SavedMeter entries[SAVED_METER_LIMIT];
  const SavedMeter *find(uint16_t id)const{for(unsigned i=0;i<count;++i)if(entries[i].id==id)return &entries[i];return nullptr;}
};
inline bool meterText(const char *s,size_t size,bool required){size_t n=0;bool visible=false;for(;n<size&&s[n];++n){if(uint8_t(s[n])<32||uint8_t(s[n])>126)return false;if(s[n]!=' ')visible=true;}return n<size&&(!required||visible);}
inline bool validDeviceEvidence(const DiscoveryDevice &d){
  if(!d.run||!d.count||d.count>DISCOVERY_PROBES||d.direction>1||!d.link.slave||d.link.slave>247||d.link.baud<1200||d.link.baud>1000000||
     (d.link.parity!='N'&&d.link.parity!='E'&&d.link.parity!='O')||(d.link.stops!=1&&d.link.stops!=2)||d.requests<d.count||d.requests>2*d.count)return false;
  for(unsigned i=0;i<d.count;++i){const auto &p=d.probes[i];
    if(!p.words||p.words>3||uint32_t(p.address)+p.words>65536UL||!p.attempts||p.attempts>2||p.validMask>3||(p.validMask==2)||p.flags>3||p.rxSize[0]>16||p.rxSize[1]>16||(p.attempts==1&&(p.validMask>1||p.rxSize[1])))return false;
    if(p.txSize[0]>8||p.txSize[1]>8)return false;
    for(unsigned k=0;k<p.attempts;++k)if(p.validMask&(1<<k)){
      const uint8_t *tx=p.tx[k];if(p.txSize[k]!=8||tx[0]!=d.link.slave||tx[1]!=3||tx[2]!=uint8_t(p.address>>8)||tx[3]!=uint8_t(p.address)||tx[4]||tx[5]!=p.words||crc16(tx,6)!=(uint16_t(tx[6])|uint16_t(tx[7])<<8))return false;
      auto result=inspectTrace(tx,p.rx[k],p.rxSize[k],p.flags&(1<<k),d.link.slave,p.words);
      if(p.status[k]==0){if(result!=TRACE_OK)return false;}
      else if(result!=TRACE_EXCEPTION||p.rx[k][2]!=p.status[k])return false;
    }
  }
  return d.confirmed();
}
inline bool validSavedMeter(const SavedMeter &m){return m.id&&meterText(m.name,sizeof(m.name),true)&&meterText(m.model,sizeof(m.model),true)&&meterText(m.notes,sizeof(m.notes),false)&&
  (m.profile==UNKNOWN_PROFILE||profiles::valid(m.profile))&&(!m.verified||profiles::valid(m.profile))&&validDeviceEvidence(m.evidence);}
inline void encodeDeviceEvidence(const DiscoveryDevice &d,uint8_t *p){
  memset(p,0,DEVICE_EVIDENCE_BYTES);meterPut32(p,d.link.baud);p[4]=d.link.slave;p[5]=uint8_t(d.link.parity);p[6]=d.link.stops;p[7]=d.direction;p[8]=d.count;
  const uint32_t values[]={d.boot,d.run,d.utc,d.elapsedMs,d.requests,d.timeouts,d.crcErrors,d.rejected,d.exceptions};for(unsigned i=0;i<9;++i)meterPut32(p+12+i*4,values[i]);
  for(unsigned i=0;i<DISCOVERY_PROBES;++i){const auto &e=d.probes[i];uint8_t *q=p+48+i*64;meterPut16(q,e.address);q[2]=e.words;q[3]=e.attempts;q[4]=e.status[0];q[5]=e.status[1];q[6]=e.validMask;q[7]=e.flags;q[8]=e.rxSize[0];q[9]=e.rxSize[1];q[10]=e.txSize[0];q[11]=e.txSize[1];meterPut16(q+12,e.durationMs[0]);meterPut16(q+14,e.durationMs[1]);memcpy(q+16,e.tx,16);memcpy(q+32,e.rx,32);}
}
inline void decodeDeviceEvidence(const uint8_t *p,DiscoveryDevice &d){
  d=DiscoveryDevice{};d.link.baud=meterGet32(p);d.link.slave=p[4];d.link.parity=char(p[5]);d.link.stops=p[6];d.direction=p[7];d.count=p[8];
  d.boot=meterGet32(p+12);d.run=meterGet32(p+16);d.utc=meterGet32(p+20);d.elapsedMs=meterGet32(p+24);d.requests=meterGet32(p+28);d.timeouts=meterGet32(p+32);d.crcErrors=meterGet32(p+36);d.rejected=meterGet32(p+40);d.exceptions=meterGet32(p+44);
  for(unsigned i=0;i<DISCOVERY_PROBES;++i){auto &e=d.probes[i];const uint8_t *q=p+48+i*64;e.address=meterGet16(q);e.words=q[2];e.attempts=q[3];e.status[0]=q[4];e.status[1]=q[5];e.validMask=q[6];e.flags=q[7];e.rxSize[0]=q[8];e.rxSize[1]=q[9];e.txSize[0]=q[10];e.txSize[1]=q[11];e.durationMs[0]=meterGet16(q+12);e.durationMs[1]=meterGet16(q+14);memcpy(e.tx,q+16,16);memcpy(e.rx,q+32,32);}
}
inline void encodeMeterRegistry(const MeterRegistry &r,uint8_t *p){
  memset(p,0,METER_REGISTRY_BYTES);meterPut32(p,0x3147524D);meterPut16(p+4,1);p[6]=r.count;meterPut16(p+8,r.nextId);meterPut32(p+12,r.generation);
  for(unsigned i=0;i<r.count;++i){const auto &e=r.entries[i];uint8_t *q=p+16+i*SAVED_METER_BYTES;meterPut16(q,e.id);q[2]=e.profile;q[3]=e.verified;memcpy(q+4,e.name,32);memcpy(q+36,e.model,40);memcpy(q+76,e.notes,64);encodeDeviceEvidence(e.evidence,q+140);}
  meterPut16(p+METER_REGISTRY_BYTES-2,crc16(p,METER_REGISTRY_BYTES-2));
}
inline void decodeSavedMeter(const uint8_t *q,SavedMeter &e){
  e.id=meterGet16(q);e.profile=q[2];e.verified=q[3];memcpy(e.name,q+4,32);memcpy(e.model,q+36,40);memcpy(e.notes,q+76,64);decodeDeviceEvidence(q+140,e.evidence);
}
inline bool validMeterRegistryBytes(const uint8_t *p,size_t size){
  if(!p||size!=METER_REGISTRY_BYTES||meterGet32(p)!=0x3147524D||meterGet16(p+4)!=1||p[6]>SAVED_METER_LIMIT||p[7]||p[10]||p[11]||!meterGet32(p+12)||meterGet16(p+size-2)!=crc16(p,size-2))return false;
  const uint16_t nextId=meterGet16(p+8);
  for(unsigned i=0;i<p[6];++i){SavedMeter entry;const uint8_t *q=p+16+i*SAVED_METER_BYTES;if(q[3]>1)return false;decodeSavedMeter(q,entry);
    if(!validSavedMeter(entry)||(nextId&&entry.id>=nextId))return false;
    for(unsigned j=0;j<i;++j)if(meterGet16(p+16+j*SAVED_METER_BYTES)==entry.id)return false;
  }
  return true;
}
inline bool decodeMeterRegistry(const uint8_t *p,size_t size,MeterRegistry &out){
  // Validate before mutation without putting a second full registry on the RTOS stack.
  if(!validMeterRegistryBytes(p,size))return false;
  out.count=p[6];out.nextId=meterGet16(p+8);out.generation=meterGet32(p+12);
  for(unsigned i=0;i<SAVED_METER_LIMIT;++i){if(i<out.count)decodeSavedMeter(p+16+i*SAVED_METER_BYTES,out.entries[i]);else out.entries[i]=SavedMeter{};}
  return true;
}

inline bool canActivateSaved(const SavedMeter &m,Settings &settings){
  if(!validSavedMeter(m)||!m.verified||!profiles::valid(m.profile)||m.evidence.link.stops!=1)return false;
  const auto &profile=profiles::get(m.profile);
  DiscoveryQuery required[3];size_t count=profiles::discoveryQueries(profile,required,3);
  for(size_t i=0;i<count;++i)if(!m.evidence.dataAt(required[i].address,required[i].words))return false;
  Settings s;s.profile=m.profile;s.slave=m.evidence.link.slave;s.baud=m.evidence.link.baud;s.parity=m.evidence.link.parity;s.direction=m.evidence.direction;
  if(!validSettings(s))return false;
  settings=s;return true;
}
} // namespace fort
