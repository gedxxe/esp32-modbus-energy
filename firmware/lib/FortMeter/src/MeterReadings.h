// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "MeterProfiles.h"
namespace fort {
struct MeterReadings {
  uint32_t rawEnergy=0;
  double meterEnergy=NAN,powerFactor=NAN,frequency=NAN,reactiveEnergy=NAN;
};
struct ReadingQuality {uint8_t required=0,transportFailed=0,invalid=0;bool complete=false;};
// Reader receives independent parameter requests. Only a complete accepted candidate
// replaces the last valid snapshot, while quality preserves every failed field.
template<class Reader> bool readMeterReadings(const profiles::Profile &profile,Reader read,MeterReadings &out,ReadingQuality *quality=nullptr){
  ReadingQuality q;q.required=profile.requiredReadings;
  if(!(q.required&FIELD_ENERGY)||(q.required&~15)||profile.energy.encoding!=profiles::U32||profile.energy.words!=2){q.invalid=q.required;if(quality)*quality=q;return false;}
  MeterReadings next;uint8_t data[4]={};
  auto field=[&](const profiles::Register &reg,double &value,uint8_t bit){
    if(!profiles::available(reg)||!read(reg,data)){q.transportFailed|=bit;return false;}
    value=profiles::decode(reg,data);if(!isfinite(value)){q.invalid|=bit;return false;}return true;
  };
  if((q.required&FIELD_PF)&&field(profile.powerFactor,next.powerFactor,FIELD_PF)&&(next.powerFactor < -1.05||next.powerFactor > 1.05))q.invalid|=FIELD_PF;
  if((q.required&FIELD_FREQUENCY)&&field(profile.frequency,next.frequency,FIELD_FREQUENCY)&&(next.frequency < 40||next.frequency > 70))q.invalid|=FIELD_FREQUENCY;
  if(field(profile.energy,next.meterEnergy,FIELD_ENERGY)){next.rawEnergy=profiles::raw32(data);if(next.meterEnergy<0)q.invalid|=FIELD_ENERGY;}
  if((q.required&FIELD_REACTIVE)&&field(profile.reactiveEnergy,next.reactiveEnergy,FIELD_REACTIVE)&&next.reactiveEnergy<0)q.invalid|=FIELD_REACTIVE;
  q.complete=!(q.transportFailed|q.invalid);if(quality)*quality=q;
  if(q.complete)out=next;
  return q.complete;
}
}
