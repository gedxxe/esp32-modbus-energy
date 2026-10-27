// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <Arduino.h>
#include "ModbusMaster.h"
#include <string.h>
#include <math.h>

namespace fort {
namespace meter {
constexpr uint16_t CURRENT_A=0x0058,VOLTAGE_A=0x0042,POWER_TOTAL=0x006A;
constexpr uint16_t FREQUENCY=0x0080,SECONDARY_ENERGY=0x0082,PRIMARY_ENERGY=0x008A,RATIOS=0x002C;
constexpr uint8_t HANDSHAKE_WORDS=2;
inline uint32_t u32(uint16_t hi,uint16_t lo){return (uint32_t(hi)<<16)|lo;}
inline double signed32(uint32_t raw){return (raw&0x80000000UL)?double(int64_t(raw)-4294967296LL):double(raw);}
inline float float32(uint16_t hi,uint16_t lo){uint32_t raw=u32(hi,lo);float value;static_assert(sizeof(value)==4,"IEEE754 float32 required");memcpy(&value,&raw,4);return value;}
}
inline uint16_t meterCRC(const uint8_t *data,size_t size){uint16_t crc=0xFFFF;while(size--)crc=crc16_update(crc,*data++);return crc;}
struct MeterLink {uint32_t baud=9600;uint8_t id=1;char parity='N';};
struct TransportOptions {
  int rx=18,tx=17,dir=16;
  bool manual=true;
  uint16_t responseMs=2000,settleUs=100,gapMs=200;
};
struct Transaction {
  uint8_t tx[8]={},rx[64]={};
  uint8_t txSize=0,rxSize=0,status=0xE2,libraryStatus=0xE2;
  uint16_t staleBytes=0;uint8_t stale[16]={};
  bool overflow=false,writeError=false,aborted=false,exception=false,echo=false;
};
// ModbusMaster owns request construction and response-length parsing. The stream
// adapter records actual I/O, bounds pathological input, and adds strict FC03
// post-validation. It never strips an echo or searches for a plausible subframe.
class FortTransport {
 public:
  using Event=void(*)(void*,uint8_t,const MeterLink&,uint16_t,uint8_t,const uint8_t*,const uint8_t*,size_t,bool);
  using Service=bool(*)(void*);
  explicit FortTransport(HardwareSerial &uart):uart(uart),observed(*this){}
  TransportOptions options{};Transaction last{};
  Event event=nullptr;Service service=nullptr;void *context=nullptr;
  uint32_t activeBaud=0;char activeParity=0;int activeRx=-1,activeTx=-1;
  int configuredDir=-1;bool configuredManual=false;
  void invalidate(){activeBaud=0;}
  bool open(const MeterLink &link){
    if(options.manual&&(options.dir==options.rx||options.dir==options.tx))return false;
    if(configuredManual&&(!options.manual||configuredDir!=options.dir)){digitalWrite(configuredDir,LOW);}
    if(options.manual){pinMode(options.dir,OUTPUT);digitalWrite(options.dir,LOW);}
    configuredManual=options.manual;configuredDir=options.dir;
    if(activeBaud==link.baud&&activeParity==link.parity&&activeRx==options.rx&&activeTx==options.tx&&uart)return true;
    uart.end();uart.setRxBufferSize(256);
    uint32_t format=link.parity=='E'?SERIAL_8E1:link.parity=='O'?SERIAL_8O1:SERIAL_8N1;
    uart.begin(link.baud,format,options.rx,options.tx);
    if(!uart){activeBaud=0;return false;}
    activeBaud=link.baud;activeParity=link.parity;activeRx=options.rx;activeTx=options.tx;return true;
  }
  uint8_t read(const MeterLink &link,uint16_t address,uint8_t words,uint16_t *out){
    last=Transaction{};current=link;currentAddress=address;currentWords=words;
    if(link.id<1||link.id>247||words<1||words>3||uint32_t(address)+words>65536UL){last.status=0xE6;return last.status;}
    // Preserve the 200 ms spacing used by the field-proven reference. Buffered
    // RX is drained before TX, and noise cannot prevent a bounded probe.
    while(haveFinish&&uint32_t(millis()-lastFinish)<options.gapMs){
      if(cooperate()){last.aborted=true;last.status=0xE4;return last.status;}delay(1);
    }
    if(cooperate()){last.aborted=true;last.status=0xE4;return last.status;}
    if(!open(link)){last.status=0xE5;emit(8);return last.status;}
    planned[0]=link.id;planned[1]=3;planned[2]=uint8_t(address>>8);planned[3]=uint8_t(address);planned[4]=0;planned[5]=words;
    uint16_t crc=meterCRC(planned,6);planned[6]=uint8_t(crc);planned[7]=uint8_t(crc>>8);
    FortTransport *previous=active();active()=this;receiving=false;preReads=0;firstRX=true;
    master.begin(link.id,observed);master.clearResponseBuffer();master.setResponseTimeout(options.responseMs);
    master.preTransmission(beforeThunk);master.postTransmission(afterThunk);master.idle(idleThunk);master.abortCheck(abortThunk);
    last.libraryStatus=master.readHoldingRegisters(address,words);
    // Capture only bytes already buffered after the library finishes. No gap is
    // required to accept a complete response, and no trailing prefix is removed.
    for(unsigned n=0;last.txSize&&n<64&&uart.available();++n)capture(uint8_t(uart.read()));
    active()=previous;if(options.manual)digitalWrite(options.dir,LOW);
    haveFinish=true;lastFinish=millis();last.status=last.libraryStatus;
    if(last.writeError)last.status=0xE5;
    else if(last.overflow)last.status=0xE7;
    else if(last.aborted)last.status=0xE4;
    else if(last.rxSize==8&&!memcmp(last.tx,last.rx,8)){last.echo=true;last.status=0xE6;}
    else if(last.status==0||(last.status>=1&&last.status<=0x7F)){
      // Upstream 2.0.1 returns exceptions before checking their CRC. Check the
      // captured exception exactly as rigorously as a successful data response.
      if(last.rxSize<5)last.status=0xE6;
      else if(meterCRC(last.rx,last.rxSize-2)!=(uint16_t(last.rx[last.rxSize-2])|(uint16_t(last.rx[last.rxSize-1])<<8)))last.status=0xE3;
      else if(last.rx[0]!=link.id)last.status=0xE0;
      else if(last.rx[1]==0x83&&last.rxSize==5&&last.rx[2]>=1&&last.rx[2]<=0x7F){last.status=last.rx[2];last.exception=true;}
      else if(last.rx[1]!=3)last.status=0xE1;
      else if(last.libraryStatus!=0||last.rxSize!=5+2*words||last.rx[2]!=2*words)last.status=0xE6;
      else{for(uint8_t i=0;i<words;++i)out[i]=master.getResponseBuffer(i);last.status=0;}
    }
    if(last.txSize||last.writeError){
      uint8_t kind=last.writeError?8:(last.aborted||(last.status==0xE2&&last.rxSize))?5:last.rxSize?4:6;
      emit(kind,last.rx,last.rxSize,last.overflow);
    }
    return last.status;
  }
 private:
  HardwareSerial &uart;ModbusMaster master;
  MeterLink current{};uint16_t currentAddress=0;uint8_t currentWords=0,planned[8]={};
  bool receiving=false,firstRX=true,haveFinish=false;uint16_t preReads=0;uint32_t lastFinish=0;
  static FortTransport *&active(){static FortTransport *owner=nullptr;return owner;}
  bool cooperate(){bool stop=service&&service(context);if(stop)last.aborted=true;return stop;}
  void emit(uint8_t kind,const uint8_t *bytes=nullptr,size_t size=0,bool overflow=false){if(event)event(context,kind,current,currentAddress,currentWords,planned,bytes,size,overflow);}
  void capture(uint8_t byte){
    if(last.rxSize<sizeof(last.rx))last.rx[last.rxSize++]=byte;else last.overflow=true;
    if(firstRX){firstRX=false;emit(3,last.rx,last.rxSize,false);}
  }
  static void beforeThunk(){auto p=active();if(!p)return;
    if(p->last.staleBytes)p->emit(7,p->last.stale,p->last.staleBytes<16?p->last.staleBytes:16,p->last.staleBytes>16);
    if(p->options.manual){digitalWrite(p->options.dir,HIGH);delayMicroseconds(p->options.settleUs);}
    p->emit(1);}
  static void afterThunk(){auto p=active();if(!p)return;
    if(p->options.manual){digitalWrite(p->options.dir,LOW);delayMicroseconds(p->options.settleUs);}
    p->receiving=true;p->emit(2,p->last.tx,p->last.txSize);}
  static void idleThunk(){auto p=active();if(p)p->cooperate();delay(1);}
  static bool abortThunk(){auto p=active();return p&&(p->cooperate()||p->last.overflow||p->last.writeError);}
  class ObservedStream:public Stream {
   public:
    explicit ObservedStream(FortTransport &owner):owner(owner){}
    int available()override{
      if(owner.receiving&&owner.last.rxSize>=sizeof(owner.last.rx)){if(owner.uart.available())owner.last.overflow=true;return 0;}
      return owner.uart.available();
    }
    int read()override{
      owner.cooperate();
      if(!owner.receiving&&owner.preReads++>=256)return -1;
      if(owner.receiving&&owner.last.rxSize>=sizeof(owner.last.rx)){owner.last.overflow=true;return -1;}
      int byte=owner.uart.read();if(byte<0)return -1;
      if(owner.receiving)owner.capture(uint8_t(byte));
      else{if(owner.last.staleBytes<16)owner.last.stale[owner.last.staleBytes]=uint8_t(byte);++owner.last.staleBytes;}
      return byte;
    }
    int peek()override{return owner.uart.peek();}
    void flush()override{owner.uart.flush();}
    size_t write(uint8_t byte)override{
      size_t count=owner.uart.write(byte);
      if(count==1&&owner.last.txSize<8)owner.last.tx[owner.last.txSize++]=byte;
      if(count!=1)owner.last.writeError=true;
      return count;
    }
   private:FortTransport &owner;
  } observed;
};
} // namespace fort
