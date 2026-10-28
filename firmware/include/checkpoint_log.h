// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "energy.h"
#include "clock_guard.h"
#include <float.h>

namespace fort {
constexpr unsigned CAPTURE_COUNT=10;
constexpr uint16_t CAPTURE_MINUTES[CAPTURE_COUNT]={360,390,420,450,480,1020,1050,1080,1110,1140};
constexpr uint32_t CAPTURE_WINDOW=5; // First accepted read requested at/after the target, within five seconds.
constexpr uint32_t CAPTURE_OFFSET=7*3600;
constexpr size_t CAPTURE_RECORD_SIZE=24+CAPTURE_COUNT*40+2;
enum CaptureStatus : uint8_t { CAPTURE_WAITING,CAPTURE_RECORDED,CAPTURE_MISSED };
inline const char *captureStatusCode(uint8_t status) {
  return status==CAPTURE_WAITING?"waiting":status==CAPTURE_RECORDED?"recorded":status==CAPTURE_MISSED?"missed":"invalid";
}
struct CaptureSource {
  uint32_t baud=0;
  uint16_t ptNum=0,ptDen=0,ct=0;
  uint8_t slave=0;char parity=0;uint8_t direction=0;
};
inline bool validCaptureSource(const CaptureSource &s) {
  return s.slave>=1&&s.slave<=247&&s.baud>0&&s.baud<=1000000&&
    (s.parity=='N'||s.parity=='E'||s.parity=='O')&&s.direction<=1&&
    s.ptNum>=1&&s.ptNum<=9999&&s.ptDen>=1&&s.ptDen<=9999&&s.ct>=1&&s.ct<=9999;
}
inline bool sameCaptureSource(const CaptureSource &a,const CaptureSource &b) {
  return a.slave==b.slave&&a.baud==b.baud&&a.parity==b.parity&&a.direction==b.direction&&
    a.ptNum==b.ptNum&&a.ptDen==b.ptDen&&a.ct==b.ct;
}
struct MeterCapture {
  double energy=0;
  uint32_t startedUTC=0,capturedUTC=0,raw=0,generation=0;
  CaptureSource source{};
  uint8_t status=CAPTURE_WAITING;
};
struct CaptureDay {
  uint32_t number=0,revision=0,generation=1;
  MeterCapture points[CAPTURE_COUNT];
};
inline uint32_t captureTarget(uint32_t day,unsigned index) {
  return day*86400+uint32_t(CAPTURE_MINUTES[index])*60-CAPTURE_OFFSET;
}
inline bool newerCaptureDay(const CaptureDay &candidate,const CaptureDay &current) {
  return candidate.number>current.number||(candidate.number==current.number&&candidate.revision>current.revision);
}
inline bool validCaptureDayNumber(uint32_t day) {
  return day>=CLOCK_MIN_UTC/86400&&day<=CLOCK_MAX_UTC/86400;
}
class CaptureTracker {
 public:
  CaptureDay day{};
  bool continuous=false;
  uint32_t previousUTC=0,previousRaw=0;
  CaptureSource previousSource{};
  void interrupt() {
    if(continuous){if(++day.generation==0)day.generation=1;continuous=false;}
  }
  void restore(const CaptureDay &saved) {
    day=saved;continuous=false;if(++day.generation==0)day.generation=1;
  }
  void newDay(uint32_t number) {day=CaptureDay{};day.number=number;continuous=false;}
  bool tick(uint32_t utc) {
    if(!validClockEpoch(utc,CAPTURE_OFFSET)||!day.number)return false;
    bool changed=false;
    for(unsigned i=0;i<CAPTURE_COUNT;++i)if(day.points[i].status==CAPTURE_WAITING&&uint64_t(utc)>uint64_t(captureTarget(day.number,i))+CAPTURE_WINDOW){
      day.points[i].status=CAPTURE_MISSED;changed=true;
    }
    if(changed)++day.revision;
    return changed;
  }
  bool urgent(uint32_t utc) const {
    if(!day.number||!validClockEpoch(utc,CAPTURE_OFFSET))return false;
    for(unsigned i=0;i<CAPTURE_COUNT;++i)if(day.points[i].status==CAPTURE_WAITING){
      uint32_t target=captureTarget(day.number,i);
      if(uint64_t(utc)+2>=target&&utc<=target+CAPTURE_WINDOW)return true;
    }
    return false;
  }
  bool due(uint32_t utc) const {
    if(!day.number||!validClockEpoch(utc,CAPTURE_OFFSET))return false;
    for(unsigned i=0;i<CAPTURE_COUNT;++i)if(day.points[i].status==CAPTURE_WAITING){
      uint32_t target=captureTarget(day.number,i);
      if(utc>=target&&utc<=target+CAPTURE_WINDOW)return true;
    }
    return false;
  }
  bool sample(uint32_t beginUTC,uint32_t endUTC,uint32_t raw,const CaptureSource &source) {
    if(!validCaptureSource(source)||!validClockEpoch(beginUTC,CAPTURE_OFFSET)||!validClockEpoch(endUTC,CAPTURE_OFFSET)||
       endUTC<beginUTC||endUTC-beginUTC>2||(endUTC+CAPTURE_OFFSET)/86400!=day.number)return false;
    double energy=double(raw)*0.001*double(source.ptNum)/source.ptDen*source.ct;
    if(!isfinite(energy)||energy<0)return false;
    if(continuous&&(endUTC<previousUTC||endUTC-previousUTC>MAX_GAP||raw<previousRaw||!sameCaptureSource(source,previousSource)))interrupt();
    previousUTC=endUTC;previousRaw=raw;previousSource=source;continuous=true;
    for(unsigned i=0;i<CAPTURE_COUNT;++i){
      uint32_t target=captureTarget(day.number,i);auto &point=day.points[i];
      if(point.status!=CAPTURE_WAITING||beginUTC<target||endUTC>target+CAPTURE_WINDOW)continue;
      point.energy=energy;point.raw=raw;point.startedUTC=beginUTC;point.capturedUTC=endUTC;
      point.source=source;point.generation=day.generation;point.status=CAPTURE_RECORDED;++day.revision;return true;
    }
    return false;
  }
};
inline bool canRetainCaptureBeforeRollover(uint32_t currentDay,uint32_t pendingDay,bool archived,bool nvsAvailable) {
  return !currentDay||archived||(nvsAvailable&&(!pendingDay||pendingDay==currentDay));
}
inline bool captureDifference(const CaptureDay &day,unsigned index,double &difference) {
  if(index>=CAPTURE_COUNT-1||index==4)return false; // No overnight/morning-to-evening interval.
  const auto &a=day.points[index];const auto &b=day.points[index+1];
  if(a.status!=CAPTURE_RECORDED||b.status!=CAPTURE_RECORDED||a.generation!=b.generation||
     !sameCaptureSource(a.source,b.source)||b.raw<a.raw||b.energy<a.energy)return false;
  difference=b.energy-a.energy;return isfinite(difference);
}
inline void capturePut16(uint8_t *p,uint16_t value){p[0]=uint8_t(value);p[1]=uint8_t(value>>8);}
inline void capturePut32(uint8_t *p,uint32_t value){for(unsigned i=0;i<4;++i)p[i]=uint8_t(value>>(8*i));}
inline uint16_t captureGet16(const uint8_t *p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
inline uint32_t captureGet32(const uint8_t *p){uint32_t value=0;for(unsigned i=0;i<4;++i)value|=uint32_t(p[i])<<(8*i);return value;}
inline void encodeCaptureDay(const CaptureDay &day,uint8_t *bytes) {
  memset(bytes,0,CAPTURE_RECORD_SIZE);capturePut32(bytes,0x31545043);capturePut16(bytes+4,1);capturePut16(bytes+6,CAPTURE_COUNT);
  capturePut32(bytes+8,day.number);capturePut32(bytes+12,day.revision);capturePut32(bytes+16,day.generation);capturePut32(bytes+20,0x06001900);
  for(unsigned i=0;i<CAPTURE_COUNT;++i){const auto &x=day.points[i];uint8_t *p=bytes+24+i*40;
    p[0]=x.status;p[1]=x.source.slave;p[2]=uint8_t(x.source.parity);p[3]=x.source.direction;
    capturePut32(p+4,x.startedUTC);capturePut32(p+8,x.capturedUTC);capturePut32(p+12,x.raw);capturePut32(p+16,x.source.baud);
    capturePut16(p+20,x.source.ptNum);capturePut16(p+22,x.source.ptDen);capturePut16(p+24,x.source.ct);
    uint64_t bits=0;static_assert(sizeof(double)==8,"Checkpoint records require IEEE754 double");memcpy(&bits,&x.energy,8);
    for(unsigned j=0;j<8;++j)p[26+j]=uint8_t(bits>>(8*j));
    capturePut32(p+34,x.generation);
  }
  capturePut16(bytes+CAPTURE_RECORD_SIZE-2,crc16(bytes,CAPTURE_RECORD_SIZE-2));
}
inline bool decodeCaptureDay(const uint8_t *bytes,size_t size,CaptureDay &out) {
  if(!bytes||size!=CAPTURE_RECORD_SIZE||captureGet32(bytes)!=0x31545043||captureGet16(bytes+4)!=1||captureGet16(bytes+6)!=CAPTURE_COUNT||
     captureGet32(bytes+20)!=0x06001900||captureGet16(bytes+size-2)!=crc16(bytes,size-2))return false;
  CaptureDay day{};day.number=captureGet32(bytes+8);day.revision=captureGet32(bytes+12);day.generation=captureGet32(bytes+16);
  if(!validCaptureDayNumber(day.number)||!day.generation)return false;
  for(unsigned i=0;i<CAPTURE_COUNT;++i){const uint8_t *p=bytes+24+i*40;auto &x=day.points[i];
    x.status=p[0];x.source.slave=p[1];x.source.parity=char(p[2]);x.source.direction=p[3];
    x.startedUTC=captureGet32(p+4);x.capturedUTC=captureGet32(p+8);x.raw=captureGet32(p+12);x.source.baud=captureGet32(p+16);
    x.source.ptNum=captureGet16(p+20);x.source.ptDen=captureGet16(p+22);x.source.ct=captureGet16(p+24);x.generation=captureGet32(p+34);
    uint64_t bits=0;for(unsigned j=0;j<8;++j)bits|=uint64_t(p[26+j])<<(8*j);memcpy(&x.energy,&bits,8);
    if(x.status>CAPTURE_MISSED||p[38]||p[39])return false;
    if(x.status==CAPTURE_RECORDED){
      if(!validCaptureSource(x.source))return false;
      uint32_t target=captureTarget(day.number,i);double expected=double(x.raw)*.001*double(x.source.ptNum)/x.source.ptDen*x.source.ct;
      if(!validClockEpoch(x.startedUTC,CAPTURE_OFFSET)||!validClockEpoch(x.capturedUTC,CAPTURE_OFFSET)||!x.generation||!isfinite(x.energy)||x.energy<0||
         x.startedUTC<target||x.capturedUTC<x.startedUTC||x.capturedUTC>target+CAPTURE_WINDOW||x.capturedUTC-x.startedUTC>2||
         fabs(x.energy-expected)>16*DBL_EPSILON*fmax(1.0,expected))return false;
    }else{
      for(unsigned j=1;j<40;++j)if(p[j])return false;
    }
  }
  out=day;return true;
}
} // namespace fort
