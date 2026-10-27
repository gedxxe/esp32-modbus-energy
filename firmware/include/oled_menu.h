// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "network.h"
namespace fort {
class OledNetworkMenu {
 public:
  enum Screen:uint8_t {CLOSED,MAIN,MODE,NETWORKS,KEY,KEY_TOOLS,IP,IP_TOOLS,AP_PASSWORD};
  enum Action:uint8_t {NONE,SAVE_MODE,SCAN,SELECT_SSID,SAVE_KEY,SAVE_IP,EXIT};
  Screen screen=CLOSED;uint8_t selected=0,letter='a',length=0,octet=0,ip[4]={192,168,4,1};char key[64]={};
  void enter(){screen=MAIN;selected=0;}
  void enterKey(){screen=KEY;letter='a';length=0;key[0]=0;}
  bool repeating()const{return screen==KEY||screen==IP;}
  uint32_t address()const{return (uint32_t(ip[0])<<24)|(uint32_t(ip[1])<<16)|(uint32_t(ip[2])<<8)|ip[3];}
  void setAddress(uint32_t value){for(unsigned i=0;i<4;++i)ip[i]=uint8_t(value>>(24-8*i));octet=0;}
  Action handle(uint8_t button,uint8_t event,const NetworkSettings &current,uint8_t count){
    if(screen==CLOSED||button>2||!event)return NONE;
    if(button<2){int delta=button==0?-1:1;
      if(screen==KEY)letter=uint8_t(32+(int(letter)-32+delta+95)%95);
      else if(screen==IP)ip[octet]=uint8_t(int(ip[octet])+delta);
      else{unsigned max=screen==MAIN?5:screen==MODE?3:screen==NETWORKS?count:screen==KEY_TOOLS?4:screen==IP_TOOLS?3:1;if(max)selected=uint8_t((int(selected)+delta+int(max))%int(max));}
      return NONE;
    }
    if(event==2){
      if(screen==KEY){screen=KEY_TOOLS;selected=0;}
      else if(screen==IP){screen=IP_TOOLS;selected=0;}
      else{screen=MAIN;selected=0;}
      return NONE;
    }
    switch(screen){
      case MAIN:
        if(selected==0){screen=MODE;selected=current.mode;}
        else if(selected==1){screen=NETWORKS;selected=0;return SCAN;}
        else if(selected==2){screen=IP;setAddress(current.apIP);}
        else if(selected==3)screen=AP_PASSWORD;
        else{screen=CLOSED;return EXIT;}
        break;
      case MODE:return SAVE_MODE;
      case NETWORKS:if(count&&selected<count)return SELECT_SSID;break;
      case KEY:if(length<63){key[length++]=char(letter);key[length]=0;}break;
      case KEY_TOOLS:
        if(selected==0)return SAVE_KEY;
        if(selected==1&&length)key[--length]=0;
        if(selected==2){length=0;key[0]=0;}
        if(selected==3){memset(key,0,sizeof(key));length=0;}
        screen=selected==3?MAIN:KEY;selected=0;break;
      case IP:octet=(octet+1)%4;break;
      case IP_TOOLS:
        if(selected==0)return SAVE_IP;
        if(selected==1){setAddress(0xC0A80401);return SAVE_IP;}
        screen=MAIN;selected=0;break;
      case AP_PASSWORD:screen=MAIN;selected=0;break;
      default:break;
    }
    return NONE;
  }
};
} // namespace fort
