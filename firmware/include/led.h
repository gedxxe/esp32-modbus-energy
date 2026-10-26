#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "energy.h"

namespace fort {
enum LedMode : uint8_t { LED_OFF, LED_SOLID, LED_HUE, LED_BLINK, LED_BREATHE };
struct LedSettings {
  uint32_t periodMs=8000;
  LedMode mode=LED_HUE;
  uint8_t brightness=128,red=0,green=184,blue=169;
};
struct LedColor { uint8_t red=0,green=0,blue=0; };
inline bool validLed(const LedSettings &s) {
  return s.mode<=LED_BREATHE&&s.periodMs>=500&&s.periodMs<=30000;
}
inline const char *ledModeName(LedMode mode) {
  const char *names[]={"off","solid","hue","blink","breathe"};
  return mode<=LED_BREATHE?names[mode]:"off";
}
inline bool parseLedMode(const char *name,LedMode &mode) {
  for(uint8_t i=0;i<=LED_BREATHE;++i)if(strcmp(name,ledModeName(LedMode(i)))==0){mode=LedMode(i);return true;}
  return false;
}
// Fixed byte layout, versioned and CRC protected; no compiler padding in NVS.
constexpr uint8_t LED_RECORD_SIZE=12;
inline void encodeLed(const LedSettings &s,uint8_t *bytes) {
  bytes[0]=1;bytes[1]=s.mode;bytes[2]=s.brightness;
  bytes[3]=s.red;bytes[4]=s.green;bytes[5]=s.blue;
  for(uint8_t i=0;i<4;++i)bytes[6+i]=uint8_t(s.periodMs>>(i*8));
  uint16_t crc=crc16(bytes,10);bytes[10]=uint8_t(crc);bytes[11]=uint8_t(crc>>8);
}
inline bool decodeLed(const uint8_t *bytes,size_t size,LedSettings &out) {
  if(size!=LED_RECORD_SIZE||bytes[0]!=1||crc16(bytes,10)!=(uint16_t(bytes[10])|(uint16_t(bytes[11])<<8)))return false;
  LedSettings s{};s.mode=LedMode(bytes[1]);s.brightness=bytes[2];s.red=bytes[3];s.green=bytes[4];s.blue=bytes[5];s.periodMs=0;
  for(uint8_t i=0;i<4;++i)s.periodMs|=uint32_t(bytes[6+i])<<(i*8);
  if(!validLed(s))return false;
  out=s;return true;
}
inline bool sameLed(const LedSettings &a,const LedSettings &b) {
  return a.mode==b.mode&&a.periodMs==b.periodMs&&a.brightness==b.brightness&&a.red==b.red&&a.green==b.green&&a.blue==b.blue;
}
inline LedColor ledColor(const LedSettings &s,uint32_t phase) {
  LedColor c{};if(!validLed(s)||s.mode==LED_OFF)return c;
  phase%=s.periodMs;
  uint32_t intensity=s.brightness;
  if(s.mode==LED_BLINK&&phase>=s.periodMs/2)return c;
  if(s.mode==LED_BREATHE){
    uint32_t half=s.periodMs/2;
    uint32_t ramp=phase<half?phase:s.periodMs-phase;
    intensity=intensity*ramp/half;
    if(intensity>s.brightness)intensity=s.brightness;
  }
  if(s.mode==LED_HUE){
    uint32_t wheel=uint64_t(phase)*1536/s.periodMs,sector=wheel/256,part=wheel%256;
    switch(sector){
      case 0:c.red=255;c.green=part;break;
      case 1:c.red=255-part;c.green=255;break;
      case 2:c.green=255;c.blue=part;break;
      case 3:c.green=255-part;c.blue=255;break;
      case 4:c.red=part;c.blue=255;break;
      default:c.red=255;c.blue=255-part;break;
    }
  }else {c.red=s.red;c.green=s.green;c.blue=s.blue;}
  c.red=(uint32_t(c.red)*intensity+127)/255;
  c.green=(uint32_t(c.green)*intensity+127)/255;
  c.blue=(uint32_t(c.blue)*intensity+127)/255;
  return c;
}
// Unsigned deltas cross millis() rollover; changing mode/period restarts a cycle.
class LedAnimation {
  uint32_t previous=0,phase=0;
  bool started=false;
  LedSettings settings{};
public:
  LedColor sample(const LedSettings &next,uint32_t now) {
    if(!validLed(next))return LedColor{};
    if(!started||next.mode!=settings.mode||next.periodMs!=settings.periodMs){phase=0;previous=now;started=true;}
    else phase=uint32_t((uint64_t(phase)+uint32_t(now-previous))%next.periodMs);
    previous=now;settings=next;return ledColor(settings,phase);
  }
};
} // namespace fort

