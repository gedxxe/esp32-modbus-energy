// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace fort {
constexpr uint32_t NETWORK_RETRY_MS=30000,NETWORK_AP_GRACE_MS=15000,NETWORK_APPLY_DELAY_MS=8000;
enum NetworkMode:uint8_t { NETWORK_AP,NETWORK_CLIENT,NETWORK_DUAL };
inline const char *networkModeName(uint8_t mode){return mode==NETWORK_CLIENT?"client":mode==NETWORK_DUAL?"dual":"ap";}
enum NetworkAuth:uint8_t { NETWORK_OPEN,NETWORK_PERSONAL,NETWORK_PEAP };
inline const char *networkAuthName(uint8_t auth){return auth==NETWORK_PEAP?"peap":auth==NETWORK_OPEN?"open":"personal";}
struct NetworkSettings {
  uint8_t mode=NETWORK_AP,auth=NETWORK_PERSONAL;
  char ssid[33]={},password[65]={};uint32_t apIP=0xC0A80401;
  char identity[128]={},username[128]={},eapPassword[128]={},serverName[254]={};
  uint8_t caHash[32]={};
};
inline void clearEnterprise(NetworkSettings &s){memset(s.identity,0,sizeof(s.identity));memset(s.username,0,sizeof(s.username));memset(s.eapPassword,0,sizeof(s.eapPassword));memset(s.serverName,0,sizeof(s.serverName));memset(s.caHash,0,sizeof(s.caHash));}
inline bool networkText(const char *p,size_t capacity,bool required=true){
  const char *end=static_cast<const char*>(memchr(p,0,capacity));if(!end||(required&&end==p))return false;
  for(;p<end;++p)if(uint8_t(*p)<32||uint8_t(*p)==127)return false;
  return true;
}
inline bool validRadiusName(const char *p){
  if(!networkText(p,254)||strlen(p)>253||!strchr(p,'.'))return false;
  unsigned label=0;char previous=0;
  for(;*p;++p){char c=*p;if(c=='.'){if(!label||label>63||previous=='-')return false;label=0;}
    else {if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')||(!label&&c=='-'))return false;++label;}previous=c;}
  return label&&label<=63&&previous!='-';
}
inline bool validAPAddress(uint32_t ip){
  uint8_t a=uint8_t(ip>>24),b=uint8_t(ip>>16),host=uint8_t(ip);
  return host>0&&host<255&&(a==10||(a==172&&b>=16&&b<=31)||(a==192&&b==168));
}
inline bool parseAPAddress(const char *text,uint32_t &out){
  uint32_t ip=0;const char *p=text;
  for(unsigned i=0;i<4;++i){unsigned n=0,digits=0;while(*p>='0'&&*p<='9'){n=n*10+unsigned(*p++-'0');if(++digits>3||n>255)return false;}if(!digits)return false;ip=(ip<<8)|n;if(i<3){if(*p++!='.')return false;}else if(*p)return false;}
  if(!validAPAddress(ip))return false;
  out=ip;return true;
}
inline bool validNetwork(const NetworkSettings &s){
  const char *ssidEnd=static_cast<const char*>(memchr(s.ssid,0,sizeof(s.ssid)));
  const char *keyEnd=static_cast<const char*>(memchr(s.password,0,sizeof(s.password)));
  if(!validAPAddress(s.apIP)||s.mode>NETWORK_DUAL||!ssidEnd||!keyEnd)return false;
  size_t ssidSize=ssidEnd-s.ssid,keySize=keyEnd-s.password;
  if(s.mode!=NETWORK_AP&&!ssidSize)return false;
  for(size_t i=0;i<ssidSize;++i)if(uint8_t(s.ssid[i])<32||uint8_t(s.ssid[i])==127)return false;
  if(s.auth>NETWORK_PEAP)return false;
  if(s.auth==NETWORK_PEAP){
    bool hash=false;for(uint8_t c:s.caHash)hash|=c!=0;
    return !keySize&&hash&&networkText(s.identity,sizeof(s.identity),false)&&networkText(s.username,sizeof(s.username))&&networkText(s.eapPassword,sizeof(s.eapPassword))&&networkText(s.serverName,sizeof(s.serverName),false)&&(!s.serverName[0]||validRadiusName(s.serverName));
  }
  if(!networkText(s.identity,sizeof(s.identity),false)||!networkText(s.username,sizeof(s.username),false)||!networkText(s.eapPassword,sizeof(s.eapPassword),false)||!networkText(s.serverName,sizeof(s.serverName),false))return false;
  if(!keySize)return s.auth==NETWORK_OPEN||s.mode==NETWORK_AP; // Legacy records infer OPEN during decoding.
  if(s.auth==NETWORK_OPEN)return false;
  if(keySize<8)return false;
  for(size_t i=0;i<keySize;++i){unsigned c=uint8_t(s.password[i]);
    if(keySize==64){if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}
    else if(c<32||c>126)return false;
  }
  return true;
}
inline bool sameNetwork(const NetworkSettings &a,const NetworkSettings &b){return a.mode==b.mode&&a.auth==b.auth&&a.apIP==b.apIP&&!strcmp(a.ssid,b.ssid)&&!strcmp(a.password,b.password)&&!strcmp(a.identity,b.identity)&&!strcmp(a.username,b.username)&&!strcmp(a.eapPassword,b.eapPassword)&&!strcmp(a.serverName,b.serverName)&&!memcmp(a.caHash,b.caHash,32);}
inline bool hasEnterpriseLogin(const NetworkSettings &s){return s.auth==NETWORK_PEAP&&s.username[0]&&s.eapPassword[0];}
inline bool canReuseEnterprise(const NetworkSettings &saved,const NetworkSettings &next){return saved.auth==NETWORK_PEAP&&next.auth==NETWORK_PEAP&&!strcmp(saved.ssid,next.ssid)&&!strcmp(saved.serverName,next.serverName)&&!memcmp(saved.caHash,next.caHash,32);}
constexpr size_t NETWORK_RECORD_SIZE=779,NETWORK_CA_MAX=4096;
inline size_t networkLength(const char *value,size_t capacity){size_t length=0;while(length<capacity&&value[length])++length;return length;}
inline uint16_t networkCRC(const uint8_t *p,size_t n){uint16_t c=0xFFFF;while(n--){c^=*p++;for(uint8_t bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xA001:0);}return c;}
inline void encodeNetwork(const NetworkSettings &s,uint8_t *bytes){
  memset(bytes,0,NETWORK_RECORD_SIZE);bytes[0]='F';bytes[1]='W';bytes[2]=3;bytes[3]=s.mode;
  memcpy(bytes+4,s.ssid,networkLength(s.ssid,33));memcpy(bytes+37,s.password,networkLength(s.password,65));
  for(unsigned i=0;i<4;++i)bytes[102+i]=uint8_t(s.apIP>>(24-8*i));
  bytes[106]=s.auth;memcpy(bytes+107,s.identity,networkLength(s.identity,128));memcpy(bytes+235,s.username,networkLength(s.username,128));memcpy(bytes+363,s.eapPassword,networkLength(s.eapPassword,128));memcpy(bytes+491,s.serverName,networkLength(s.serverName,254));memcpy(bytes+745,s.caHash,32);
  uint16_t c=networkCRC(bytes,777);bytes[777]=uint8_t(c);bytes[778]=uint8_t(c>>8);
}
inline bool decodeNetwork(const uint8_t *bytes,size_t size,NetworkSettings &out){
  if(size<4||bytes[0]!='F'||bytes[1]!='W')return false;
  bool v1=size==104&&bytes[2]==1,v2=size==108&&bytes[2]==2,v3=size==NETWORK_RECORD_SIZE&&bytes[2]==3;
  if(!v1&&!v2&&!v3)return false;
  size_t crcAt=size-2;if(networkCRC(bytes,crcAt)!=(uint16_t(bytes[crcAt])|(uint16_t(bytes[crcAt+1])<<8)))return false;
  NetworkSettings s{};s.mode=bytes[3];memcpy(s.ssid,bytes+4,33);memcpy(s.password,bytes+37,65);
  if(!v1)s.apIP=(uint32_t(bytes[102])<<24)|(uint32_t(bytes[103])<<16)|(uint32_t(bytes[104])<<8)|bytes[105];
  if(v3){s.auth=bytes[106];memcpy(s.identity,bytes+107,128);memcpy(s.username,bytes+235,128);memcpy(s.eapPassword,bytes+363,128);memcpy(s.serverName,bytes+491,254);memcpy(s.caHash,bytes+745,32);}
  else s.auth=s.password[0]?NETWORK_PERSONAL:NETWORK_OPEN;
  if(!validNetwork(s))return false;
  out=s;return true;
}
inline bool pauseEnterpriseRetry(uint8_t auth,uint16_t reason){return auth==NETWORK_PEAP&&(reason==23||reason==202);}
inline const char *networkDisconnectHint(uint16_t reason){
  switch(reason){case 23:case 202:return "authentication_failed";case 201:return "ssid_unavailable";case 15:case 204:return "handshake_timeout";case 200:return "beacon_timeout";case 203:return "association_failed";default:return "link_lost";}
}
// Millisecond decisions are independent of the driver and use unsigned elapsed time.
// AP stays available during every connection attempt and after a lost client link.
class NetworkPolicy {
 public:
  uint8_t mode=NETWORK_AP;bool connected=false,fallback=false;
  void begin(uint8_t value,uint32_t now,uint32_t retry=NETWORK_RETRY_MS){retryMs=retry;mode=value;connected=false;fallback=false;started=lastAttempt=now;connectedAt=0;}
  bool update(uint32_t now,bool ready){
    if(mode==NETWORK_AP){connected=false;fallback=false;return false;}
    if(ready){if(!connected)connectedAt=now;connected=true;fallback=false;started=now;return false;}
    if(connected)started=now;
    connected=false;fallback=uint32_t(now-started)>=retryMs;
    if(uint32_t(now-lastAttempt)>=retryMs){lastAttempt=now;return true;}
    return false;
  }
  bool apWanted(uint32_t now)const{return mode!=NETWORK_CLIENT||!connected||uint32_t(now-connectedAt)<NETWORK_AP_GRACE_MS;}
 private:uint32_t started=0,lastAttempt=0,connectedAt=0,retryMs=NETWORK_RETRY_MS;
};
// A scan owns the station radio until it completes. Association retries must wait.
class WiFiScanPolicy {
 public:
  static constexpr uint32_t PREPARE_MS=300,RETRY_MS=750,RADIO_MS=12000,TOTAL_MS=20000;
  bool running=false,radio=false,failed=false;uint8_t attempts=0;
  uint32_t started=0,changed=0,finished=0;const char *error="";
  bool begin(uint32_t now){
    if(running||(attempts&&uint32_t(now-finished)<1500))return false;
    running=true;radio=false;failed=false;attempts=0;started=changed=now;error="";return true;
  }
  bool due(uint32_t now)const{return running&&!radio&&uint32_t(now-changed)>=(attempts?RETRY_MS:PREPARE_MS);}
  void attempt(uint32_t now,bool accepted){++attempts;changed=now;radio=accepted;if(!accepted)retry(now,"radio_start");}
  void retry(uint32_t now,const char *why){
    radio=false;changed=now;error=why;
    if(attempts>=3||uint32_t(now-started)>=TOTAL_MS)finish(now,false,why);
  }
  void finish(uint32_t now,bool ok,const char *why=""){
    running=radio=false;failed=!ok;finished=now;error=ok?"":why;
  }
  bool expired(uint32_t now)const{return running&&(uint32_t(now-started)>=TOTAL_MS||(radio&&uint32_t(now-changed)>=RADIO_MS));}
};
inline uint32_t chooseAPAddress(uint32_t station,uint32_t mask,uint32_t preferred=0xC0A80401){
  if(station&&!mask)return 0;
  const uint32_t candidates[]={preferred,0xC0A80401,0xC0A80501,0x0A2A0001,0xAC172A01};
  for(uint32_t ip:candidates)if(!station||(ip&(mask&0xFFFFFF00UL))!=(station&(mask&0xFFFFFF00UL)))return ip;
  return 0; // Invalid/overbroad LAN mask cannot silently create overlapping routes.
}
} // namespace fort
