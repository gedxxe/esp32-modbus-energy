// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "meter_registry.h"
namespace fort {
// Only the core-1 owner calls this adapter. A failed write leaves the previous bank recoverable.
class MeterRegistryStore {
 public:
  MeterRegistry registry{};bool healthy=true,available=false;int8_t activeBank=-1;
  template<class Backend> bool load(Backend &prefs,bool nvs){
    registry=MeterRegistry{};activeBank=-1;healthy=true;available=nvs;if(!nvs){healthy=false;return false;}
    bool any=false;uint32_t best=0;MeterRegistry candidate;
    for(unsigned i=0;i<2;++i){const char *key=i?"meters_b":"meters_a";if(!prefs.isKey(key))continue;any=true;
      if(prefs.getBytesLength(key)!=METER_REGISTRY_BYTES||prefs.getBytes(key,bytes,sizeof(bytes))!=sizeof(bytes)||!decodeMeterRegistry(bytes,sizeof(bytes),candidate))continue;
      if(activeBank<0||int32_t(candidate.generation-best)>0){registry=candidate;best=candidate.generation;activeBank=int8_t(i);}
    }
    healthy=!any||activeBank>=0;
    uint8_t sequence[6]={};
    if(prefs.isKey("meter_seq")){
      if(prefs.getBytesLength("meter_seq")!=sizeof(sequence)||prefs.getBytes("meter_seq",sequence,sizeof(sequence))!=sizeof(sequence)||meterGet16(sequence+2)!=uint16_t(~meterGet16(sequence))||meterGet16(sequence+4)!=crc16(sequence,4)){healthy=false;available=false;return false;}
      uint16_t next=meterGet16(sequence);if(!next||(registry.nextId&&next>registry.nextId))registry.nextId=next;
    }else if(any){healthy=false;available=false;}
    return healthy;
  }
  template<class Backend> bool commit(Backend &prefs,MeterRegistry &next){
    if(!available||(!healthy&&activeBank<0))return false;
    if((!registry.nextId&&next.nextId)||(registry.nextId&&next.nextId&&next.nextId<registry.nextId))return false;
    next.generation=registry.generation+1;if(!next.generation)next.generation=1;
    encodeMeterRegistry(next,bytes);if(!validMeterRegistryBytes(bytes,sizeof(bytes)))return false;
    if(next.nextId!=registry.nextId||activeBank<0){
      uint8_t sequence[6],readback[6];meterPut16(sequence,next.nextId);meterPut16(sequence+2,uint16_t(~next.nextId));meterPut16(sequence+4,crc16(sequence,4));
      if(prefs.putBytes("meter_seq",sequence,sizeof(sequence))!=sizeof(sequence)){healthy=false;return false;}
      registry.nextId=next.nextId;
      if(prefs.getBytes("meter_seq",readback,sizeof(readback))!=sizeof(readback)||memcmp(sequence,readback,sizeof(sequence))){healthy=false;return false;}
    }
    const char *key=activeBank==0?"meters_b":"meters_a";
    if(prefs.putBytes(key,bytes,sizeof(bytes))!=sizeof(bytes)){healthy=false;return false;}
    if(prefs.getBytesLength(key)!=sizeof(bytes)||prefs.getBytes(key,verify,sizeof(verify))!=sizeof(verify)||memcmp(bytes,verify,sizeof(bytes))){healthy=false;return false;}
    registry=next;activeBank=activeBank==0?1:0;healthy=true;return true;
  }
 private:
  uint8_t bytes[METER_REGISTRY_BYTES]={},verify[METER_REGISTRY_BYTES]={};
};
} // namespace fort
