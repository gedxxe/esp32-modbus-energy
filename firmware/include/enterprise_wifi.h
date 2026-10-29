// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "network_settings.h"
#include <FS.h>
namespace fort {
bool validateEnterpriseCA(const char *pem,uint8_t hash[32]);
bool loadNetworkCA(fs::FS &fs,const uint8_t hash[32],char *pem);
bool storeNetworkCA(fs::FS &fs,const char *pem,const uint8_t hash[32]);
void formatCAHash(const uint8_t hash[32],char text[65]);
class EnterpriseWiFi {
 public:
  const char *error="";int32_t apiCode=0;uint16_t disconnectReason=0;uint32_t attempts=0;
  bool reset();
  bool begin(const NetworkSettings &settings,const char *ca,bool clockValid,uint32_t utc);
  void disconnected(uint16_t reason){if(!attempted||reason==8||authenticationPaused)return;disconnectReason=reason;authenticationPaused=pauseEnterpriseRetry(auth,reason);error=authenticationPaused?"enterprise_authentication_rejected":networkDisconnectHint(reason);}
  void online(){error="";disconnectReason=0;apiCode=0;authenticationPaused=false;}
 private:
  bool enabled=false,configured=false;
 public:bool attempted=false,authenticationPaused=false;
 private:uint8_t auth=NETWORK_PERSONAL;
 private:
  bool checked(int32_t result);
};
} // namespace fort
