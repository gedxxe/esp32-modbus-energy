// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "trace.h"
namespace fort {
constexpr uint8_t FLOW_CAPACITY=32;
struct FlowEvent {
  uint32_t seq=0,atUs=0,baud=0;
  uint16_t address=0;
  uint8_t slave=0,words=0,kind=0,size=0;
  int8_t rxPin=-1,txPin=-1,dirPin=-1;char parity='N';bool diagnostic=false,overflow=false;
  TraceResult result=TRACE_INVALID;
  uint8_t bytes[16]={};
};
struct UartFlow {
  FlowEvent events[FLOW_CAPACITY]{};
  uint32_t sequence=0,boot=0;
  uint8_t head=0,count=0;
  void append(uint8_t kind,uint32_t at,uint32_t baud,char parity,uint8_t id,uint16_t address,uint8_t words,const uint8_t *bytes,size_t size,bool overflow,bool diagnostic,TraceResult result,int8_t rxPin=-1,int8_t txPin=-1,int8_t dirPin=-1){
    FlowEvent &e=events[head];e=FlowEvent{};e.seq=++sequence;e.atUs=at;e.baud=baud;e.parity=parity;e.slave=id;e.address=address;e.words=words;e.kind=kind;e.diagnostic=diagnostic;e.overflow=overflow||size>16;e.result=result;e.rxPin=rxPin;e.txPin=txPin;e.dirPin=dirPin;
    e.size=size>16?16:uint8_t(size);if(e.size&&bytes)memcpy(e.bytes,bytes,e.size);
    head=(head+1)%FLOW_CAPACITY;if(count<FLOW_CAPACITY)++count;
  }
  const FlowEvent &at(uint8_t index)const{return events[(head+FLOW_CAPACITY-count+index)%FLOW_CAPACITY];}
};
} // namespace fort