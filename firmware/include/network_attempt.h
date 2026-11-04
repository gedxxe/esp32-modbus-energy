// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
namespace fort {
// Core-0 connection lifecycle. Deadlines use unsigned differences across millis rollover.
class NetworkAttempt {
 public:
  enum Stage:uint8_t { IDLE, PREPARING, CONNECTING, ADDRESSING, ONLINE, FAILED };
  Stage stage=IDLE;
  uint32_t id=0,started=0,changed=0,failedAt=0,lastFailedId=0;
  const char *error="",*lastError="";
  uint16_t reason=0,lastReason=0;
  int32_t apiCode=0,lastApiCode=0;
  bool active()const{return stage==PREPARING||stage==CONNECTING||stage==ADDRESSING;}
  bool accepts(uint32_t at)const{return (stage==CONNECTING||stage==ADDRESSING||stage==ONLINE)&&int32_t(at-changed)>=0;}
  void idle(){stage=IDLE;error="";reason=0;apiCode=0;}
  void prepare(uint32_t now,bool enterprise){
    ++id;if(!id)++id;stage=PREPARING;started=changed=now;
    budget=enterprise?60000:30000;error="";reason=0;apiCode=0;
  }
  void connecting(uint32_t now){stage=CONNECTING;started=changed=now;}
  void associated(uint32_t now){if(stage==CONNECTING){stage=ADDRESSING;changed=now;}}
  void online(){if(stage==ADDRESSING){stage=ONLINE;error="";reason=0;apiCode=0;}}
  void fail(uint32_t now,const char *why,uint16_t disconnect=0,int32_t code=0){
    stage=FAILED;error=lastError=why;reason=lastReason=disconnect;apiCode=lastApiCode=code;
    failedAt=now;lastFailedId=id;
  }
  const char *expired(uint32_t now)const{
    if(stage==PREPARING&&uint32_t(now-changed)>=2000)return "station_disconnect_timeout";
    if(stage==ADDRESSING&&uint32_t(now-changed)>=15000)return "dhcp_timeout";
    if((stage==CONNECTING||stage==ADDRESSING)&&uint32_t(now-started)>=budget)return "connection_timeout";
    return "";
  }
  bool retryAllowed(uint32_t now)const{return !active()&&(stage!=FAILED||uint32_t(now-failedAt)>=30000);}
  uint32_t elapsed(uint32_t now)const{return active()?uint32_t(now-started):0;}
  const char *name()const{
    switch(stage){case PREPARING:return "preparing";case CONNECTING:return "connecting";case ADDRESSING:return "addressing";case ONLINE:return "online";case FAILED:return "failed";default:return "idle";}
  }
 private:uint32_t budget=30000;
};
} // namespace fort
