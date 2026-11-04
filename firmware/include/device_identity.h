// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
namespace fort {
struct DeviceIdentity {
  char id[13]={},hostname[24]="energy-unconfigured",apSSID[33]={};
  bool valid=false,uniqueAP=false;
  bool initialize(const uint8_t mac[6],const char *prefix,const char *legacy,bool append){
    valid=false;uniqueAP=false;id[0]=0;
    snprintf(hostname,sizeof(hostname),"energy-unconfigured");
    snprintf(apSSID,sizeof(apSSID),"%s",legacy);
    unsigned nonzero=0,notff=0;for(unsigned i=0;i<6;++i){nonzero|=mac[i];notff|=mac[i]^0xff;}
    if(!nonzero||!notff||(mac[0]&1)||strlen(prefix)>19||strlen(legacy)>32)return false;
    snprintf(id,sizeof(id),"%02X%02X%02X%02X%02X%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    snprintf(hostname,sizeof(hostname),"energy-%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    if(append){snprintf(apSSID,sizeof(apSSID),"%s-%s",prefix,id);uniqueAP=true;}
    valid=true;return true;
  }
};
} // namespace fort
