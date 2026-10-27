// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace fort {
struct DisplaySettings {uint16_t idleSeconds=60;};
inline bool validDisplay(const DisplaySettings &s){return s.idleSeconds==0||(s.idleSeconds>=10&&s.idleSeconds<=3600);}
constexpr size_t DISPLAY_RECORD_SIZE=8;
inline uint16_t displayCRC(const uint8_t *p,size_t n){uint16_t c=0xFFFF;while(n--){c^=*p++;for(uint8_t bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xA001:0);}return c;}
inline void encodeDisplay(const DisplaySettings &s,uint8_t *out){out[0]='O';out[1]='S';out[2]=1;out[3]=0;out[4]=uint8_t(s.idleSeconds);out[5]=uint8_t(s.idleSeconds>>8);uint16_t c=displayCRC(out,6);out[6]=uint8_t(c);out[7]=uint8_t(c>>8);}
inline bool decodeDisplay(const uint8_t *p,size_t size,DisplaySettings &out){
 if(size!=DISPLAY_RECORD_SIZE||p[0]!='O'||p[1]!='S'||p[2]!=1||p[3]!=0||displayCRC(p,6)!=(uint16_t(p[6])|(uint16_t(p[7])<<8)))return false;
 DisplaySettings value;value.idleSeconds=uint16_t(p[4])|(uint16_t(p[5])<<8);if(!validDisplay(value))return false;out=value;return true;
}
inline bool decodeOLEDRun(const uint8_t *source,size_t size,uint8_t *destination,size_t expected){
 size_t at=0,written=0;
 while(at<size){uint8_t token=source[at++];size_t count=token<128?size_t(token)+1:size_t(token)-127;
  if(count>expected-written)return false;
  if(token<128){if(count>size-at)return false;for(size_t i=0;i<count;++i)destination[written++]=source[at++];}
  else{if(at>=size)return false;uint8_t value=source[at++];for(size_t i=0;i<count;++i)destination[written++]=value;}
 }
 return written==expected;
}
struct IdleResult {bool sleeping=false,woke=false,consume=false;};
class IdleScreen {
 public:
  IdleResult poll(uint32_t now,uint8_t pressed,uint16_t timeout,bool available){
   IdleResult result;
   if(!initialized||timeout!=seconds){result.woke=sleeping;if(sleeping&&pressed){guard=true;releasedAt=now;}initialized=true;seconds=timeout;lastInput=now;sleeping=false;}
   if(pressed){lastInput=now;if(sleeping){sleeping=false;guard=true;result.woke=true;}if(guard)releasedAt=now;}
   if(guard){if(!pressed&&uint32_t(now-releasedAt)>=100)guard=false;result.consume=true;return result;}
   if(!available||!timeout){result.woke=result.woke||sleeping;lastInput=now;sleeping=false;}
   else if(!sleeping&&uint32_t(now-lastInput)>=uint32_t(timeout)*1000){sleeping=true;started=now;}
   result.sleeping=sleeping;return result;
  }
  bool active()const{return sleeping;}
  uint16_t frame(uint32_t now,uint16_t count,uint32_t period)const{return count&&period?uint16_t((uint32_t(now-started)/period)%count):0;}
 private:bool initialized=false,sleeping=false,guard=false;uint16_t seconds=60;uint32_t lastInput=0,releasedAt=0,started=0;
};
} // namespace fort
