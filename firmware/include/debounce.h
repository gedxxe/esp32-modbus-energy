#pragma once
#include <stdint.h>
namespace fort {
// Input is true when released (INPUT_PULLUP), false when pressed.
class Debouncer {
  bool raw=true,stable=true,longSent=false;
  uint32_t changed=0,pressed=0;
 public:
  uint8_t update(bool released,uint32_t now) {
    if(released!=raw){raw=released;changed=now;}
    uint8_t event=0;
    if(raw!=stable&&uint32_t(now-changed)>=35) {
      stable=raw;
      if(!stable){pressed=now;longSent=false;}
      else if(!longSent)event=1;
    }
    if(!stable&&!longSent&&uint32_t(now-pressed)>=900){longSent=true;event=2;}
    return event;
  }
};
}
