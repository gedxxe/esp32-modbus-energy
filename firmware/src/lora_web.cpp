// SPDX-License-Identifier: GPL-3.0-only
#include "lora_web.h"
namespace fort {
namespace {
void response(WebServer &s,int status,const char *code){JsonDocument d;d["code"]=code;d["message"]=code;String out;serializeJson(d,out);s.send(status,"application/json",out);}
bool number(const String &s,uint32_t &out){if(!s.length()||s.length()>10)return false;uint64_t n=0;for(char c:s){if(c<'0'||c>'9')return false;n=n*10+unsigned(c-'0');if(n>UINT32_MAX)return false;}out=uint32_t(n);return true;}
int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
}
void addLoRaJSON(JsonObject d){LoRaView v=readLoRaView();const auto &s=v.settings;
  d["revision"]=v.revision;d["ready"]=v.configured;d["enabled"]=s.enabled;d["stored"]=v.stored;d["available"]=v.available;d["paired"]=v.paired;d["phase"]=v.phase;d["code"]=v.code;
  d["frequencyHz"]=s.frequencyHz;d["bandwidthKHz"]=s.bandwidthKHz;d["sf"]=s.sf;d["cr"]=s.cr;d["powerDbm"]=s.powerDbm;d["preamble"]=s.preamble;d["syncWord"]=s.syncWord;d["nodeId"]=s.nodeId;d["gatewayId"]=s.gatewayId;d["networkId"]=s.networkId;d["intervalMs"]=s.intervalMs;d["ackMs"]=s.ackMs;d["retries"]=s.retries;d["hasKey"]=lora::hasKey(s);
  d["authErrors"]=v.authErrors;d["addressErrors"]=v.addressErrors;d["replayErrors"]=v.replayErrors;d["lastTxSequence"]=v.lastTxSequence;d["lastRxSequence"]=v.lastRxSequence;
  d["tx"]=v.tx;d["rx"]=v.rx;d["acks"]=v.acks;d["timeouts"]=v.timeouts;d["rejected"]=v.rejected;d["crcErrors"]=v.crcErrors;d["drops"]=v.drops;d["lastRxAgeMs"]=v.lastRxMs?uint32_t(millis()-v.lastRxMs):0;d["rssi"]=v.lastRxMs?v.rssi:0;d["snr"]=v.lastRxMs?v.snr:0;d["waitMs"]=v.waitMs;d["airtimeMs"]=v.lastAirMs;
}
void setupLoRaWeb(WebServer &server,bool (*authorized)(),void (*save)(const lora::Settings &,uint32_t)){
  server.on("/api/lora/config",HTTP_POST,[&server,authorized,save](){
    if(!authorized())return;LoRaView current=readLoRaView();if(!current.configured){response(server,503,"lora_unavailable");return;}
    uint32_t revision;if(!number(server.arg("revision"),revision)||revision!=current.revision){response(server,409,"lora_settings_changed");return;}
    lora::Settings s=current.settings;s.enabled=server.arg("enabled")=="1";
    uint32_t n=0;auto field=[&](const char *name,uint32_t low,uint32_t high,uint32_t &target){if(!number(server.arg(name),n)||n<low||n>high)return false;target=n;return true;};
    uint32_t bw,sf,cr,power,preamble,sync,node,gateway,network,retries;
    if(!field("frequencyHz",803000000,930000000,s.frequencyHz)||!field("bandwidthKHz",125,500,bw)||!field("sf",7,12,sf)||!field("cr",5,8,cr)||!field("powerDbm",0,22,power)||!field("preamble",8,64,preamble)||!field("syncWord",0,255,sync)||!field("nodeId",1,65534,node)||!field("gatewayId",1,65534,gateway)||!field("networkId",1,65535,network)||!field("intervalMs",30000,3600000,s.intervalMs)||!field("ackMs",1000,60000,s.ackMs)||!field("retries",0,4,retries)){response(server,400,"invalid_lora");return;}
    s.bandwidthKHz=bw;s.sf=sf;s.cr=cr;s.powerDbm=power;s.preamble=preamble;s.syncWord=sync;s.nodeId=node;s.gatewayId=gateway;s.networkId=network;s.retries=retries;
    String key=server.arg("key");if(key.length()){
      if(key.length()!=lora::KEY_SIZE*2||strlen(key.c_str())!=key.length()){response(server,400,"invalid_lora_key");return;}
      for(unsigned i=0;i<lora::KEY_SIZE;++i){int hi=nibble(key[i*2]),lo=nibble(key[i*2+1]);if(hi<0||lo<0){response(server,400,"invalid_lora_key");return;}s.key[i]=uint8_t(hi*16+lo);}
    }else if(server.arg("keepKey")!="1")memset(s.key,0,sizeof(s.key));
    if(!lora::valid(s)){response(server,400,"invalid_lora");return;}save(s,revision);
  });
  server.on("/api/lora/control",HTTP_POST,[&server,authorized](){
    if(!authorized())return;String action=server.arg("action");
    if(action!="pair"&&action!="sample"){response(server,400,"invalid_lora");return;}
    if(!controlLoRa(action=="pair"?1:2)){response(server,409,"lora_unavailable");return;}response(server,202,"lora_control_queued");
  });
}
} // namespace fort
