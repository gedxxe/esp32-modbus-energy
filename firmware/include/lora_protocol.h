// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace fort { namespace lora {
constexpr size_t KEY_SIZE=32,HEADER_SIZE=24,TAG_SIZE=16,MAX_PAYLOAD=112,MAX_PACKET=HEADER_SIZE+MAX_PAYLOAD+TAG_SIZE,SETTINGS_SIZE=80;
enum Type:uint8_t {HELLO=1,ACK=2,TELEMETRY=3,POLL=4,CHECKPOINT=5};
inline void put16(uint8_t *p,uint16_t n){p[0]=uint8_t(n);p[1]=uint8_t(n>>8);}
inline uint16_t get16(const uint8_t *p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
inline void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=uint8_t(n>>(8*i));}
inline uint32_t get32(const uint8_t *p){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(p[i])<<(8*i);return n;}
inline void put64(uint8_t *p,uint64_t n){for(unsigned i=0;i<8;++i)p[i]=uint8_t(n>>(8*i));}
inline uint64_t get64(const uint8_t *p){uint64_t n=0;for(unsigned i=0;i<8;++i)n|=uint64_t(p[i])<<(8*i);return n;}
inline uint16_t checksum(const uint8_t *p,size_t n){uint16_t c=0xffff;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xa001:0);}return c;}
struct Settings {
  bool enabled=false;uint32_t frequencyHz=915000000,intervalMs=60000,ackMs=10000;
  uint16_t bandwidthKHz=125,preamble=8,nodeId=2,gatewayId=1,networkId=1;
  uint8_t sf=7,cr=5,syncWord=0x12,retries=2;int8_t powerDbm=14;uint8_t key[KEY_SIZE]={};
};
inline bool hasKey(const Settings &s){for(uint8_t n:s.key)if(n)return true;return false;}
inline bool valid(const Settings &s){return s.frequencyHz>=803000000&&s.frequencyHz<=930000000&&
  (s.bandwidthKHz==125||s.bandwidthKHz==250||s.bandwidthKHz==500)&&s.sf>=7&&s.sf<=12&&s.cr>=5&&s.cr<=8&&
  s.powerDbm>=0&&s.powerDbm<=22&&s.preamble>=8&&s.preamble<=64&&s.nodeId&&s.nodeId<65535&&s.gatewayId&&s.gatewayId<65535&&s.nodeId!=s.gatewayId&&s.networkId&&
  s.intervalMs>=30000&&s.intervalMs<=3600000&&s.ackMs>=1000&&s.ackMs<=60000&&s.retries<=4&&(!s.enabled||hasKey(s));}
inline bool same(const Settings &a,const Settings &b){return a.enabled==b.enabled&&a.frequencyHz==b.frequencyHz&&a.intervalMs==b.intervalMs&&a.ackMs==b.ackMs&&a.bandwidthKHz==b.bandwidthKHz&&a.preamble==b.preamble&&a.nodeId==b.nodeId&&a.gatewayId==b.gatewayId&&a.networkId==b.networkId&&a.sf==b.sf&&a.cr==b.cr&&a.syncWord==b.syncWord&&a.retries==b.retries&&a.powerDbm==b.powerDbm&&!memcmp(a.key,b.key,KEY_SIZE);}
inline void encodeSettings(const Settings &s,uint32_t generation,uint8_t *p){
  memset(p,0,SETTINGS_SIZE);memcpy(p,"LRC1",4);p[4]=1;p[5]=s.enabled;p[6]=s.sf;p[7]=s.cr;put16(p+8,s.bandwidthKHz);put16(p+10,s.preamble);put32(p+12,s.frequencyHz);put16(p+16,s.nodeId);put16(p+18,s.gatewayId);put16(p+20,s.networkId);p[22]=uint8_t(s.powerDbm);p[23]=s.syncWord;p[24]=s.retries;put32(p+26,s.intervalMs);put32(p+30,s.ackMs);memcpy(p+34,s.key,KEY_SIZE);put32(p+66,generation);put16(p+78,checksum(p,78));
}
inline bool decodeSettings(const uint8_t *p,size_t n,Settings &out,uint32_t &generation){
  if(n!=SETTINGS_SIZE||memcmp(p,"LRC1",4)||p[4]!=1||p[5]>1||p[25]||get16(p+78)!=checksum(p,78)||!get32(p+66))return false;
  for(unsigned i=70;i<78;++i)if(p[i])return false;
  Settings s;s.enabled=p[5];s.sf=p[6];s.cr=p[7];s.bandwidthKHz=get16(p+8);s.preamble=get16(p+10);s.frequencyHz=get32(p+12);s.nodeId=get16(p+16);s.gatewayId=get16(p+18);s.networkId=get16(p+20);s.powerDbm=int8_t(p[22]);s.syncWord=p[23];s.retries=p[24];s.intervalMs=get32(p+26);s.ackMs=get32(p+30);memcpy(s.key,p+34,KEY_SIZE);if(!valid(s))return false;out=s;generation=get32(p+66);return true;
}
class SettingsStore {
 public:Settings settings{};uint32_t generation=0;int8_t bank=-1;bool healthy=true;
  template<class Backend> bool load(Backend &b){settings=Settings{};generation=0;bank=-1;healthy=true;bool any=false;Settings s;uint32_t gen;
    for(unsigned i=0;i<2;++i){const char *key=i?"lora_b":"lora_a";if(!b.isKey(key))continue;any=true;
      if(b.getBytesLength(key)!=SETTINGS_SIZE||b.getBytes(key,bytes,sizeof(bytes))!=sizeof(bytes)||!decodeSettings(bytes,sizeof(bytes),s,gen))continue;
      if(bank<0||int32_t(gen-generation)>0){settings=s;generation=gen;bank=int8_t(i);}}
    healthy=!any||bank>=0;return healthy;
  }
  template<class Backend> bool save(Backend &b,const Settings &s){if(!valid(s))return false;uint32_t gen=generation+1;if(!gen)gen=1;
    encodeSettings(s,gen,bytes);const char *key=bank==0?"lora_b":"lora_a";
    if(b.putBytes(key,bytes,sizeof(bytes))!=sizeof(bytes)||b.getBytesLength(key)!=sizeof(bytes)||b.getBytes(key,verify,sizeof(verify))!=sizeof(verify)||memcmp(bytes,verify,sizeof(bytes))){healthy=false;return false;}
    settings=s;generation=gen;bank=bank==0?1:0;healthy=true;return true;
  }
 private:uint8_t bytes[SETTINGS_SIZE]={},verify[SETTINGS_SIZE]={};
};
struct Frame {uint8_t type=0,length=0;uint16_t network=0,source=0,destination=0;uint32_t sequence=0;uint64_t session=0;uint8_t payload[MAX_PAYLOAD]={};};
inline size_t encodeFrame(const Frame &f,uint8_t *p){if(f.type<HELLO||f.type>CHECKPOINT||f.length>MAX_PAYLOAD||!f.sequence||!f.session||!f.network||!f.source||f.source==65535||!f.destination||f.destination==65535||f.source==f.destination)return 0;
  p[0]='E';p[1]='S';p[2]=1;p[3]=f.type;put16(p+4,f.network);put16(p+6,f.source);put16(p+8,f.destination);p[10]=f.length;p[11]=0;put32(p+12,f.sequence);put64(p+16,f.session);memcpy(p+HEADER_SIZE,f.payload,f.length);return HEADER_SIZE+f.length;
}
// Call only after constant-time HMAC verification. No wire struct/padding is used.
inline bool decodeFrame(const uint8_t *p,size_t n,Frame &f){if(n<HEADER_SIZE||p[0]!='E'||p[1]!='S'||p[2]!=1||p[3]<HELLO||p[3]>CHECKPOINT||p[10]>MAX_PAYLOAD||p[11]||n!=HEADER_SIZE+p[10])return false;
  Frame next;next.type=p[3];next.network=get16(p+4);next.source=get16(p+6);next.destination=get16(p+8);next.length=p[10];next.sequence=get32(p+12);next.session=get64(p+16);
  if(!next.sequence||!next.session||!next.network||!next.source||next.source==65535||!next.destination||next.destination==65535||next.source==next.destination)return false;
  memcpy(next.payload,p+HEADER_SIZE,next.length);f=next;return true;
}
inline bool addressed(const Frame &f,const Settings &s,uint64_t session){return f.network==s.networkId&&f.source==s.gatewayId&&f.destination==s.nodeId&&f.session==session;}
inline bool acknowledges(const Frame &f,const Frame &sent){return f.type==ACK&&f.sequence==sent.sequence&&f.session==sent.session&&f.length==1&&f.payload[0]==sent.type;}
class DutyGate {
 public:bool used=false;uint32_t at=0,quietMs=0;
  void transmitted(uint32_t now,uint32_t airtimeMs){used=true;at=now;quietMs=airtimeMs>UINT32_MAX/100?UINT32_MAX:airtimeMs*100;}
  uint32_t remaining(uint32_t now)const{uint32_t elapsed=now-at;return used&&elapsed<quietMs?quietMs-elapsed:0;}
};
} } // namespace fort::lora
