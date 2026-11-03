#include "lora_protocol.h"
#include "checkpoint_log.h"
#include "recovery.h"
#include "verified_record.h"
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>
#include <cstring>
#undef assert
#define assert(x) do{if(!(x)){std::cerr<<"FAIL power/LoRa line "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(0)
using namespace fort;
struct Store {
 std::map<std::string,std::vector<uint8_t>> data;int cut=-1;bool lie=false,wrongRead=false;
 bool isKey(const char *key){return data.count(key);}
 size_t getBytesLength(const char *key){return data[key].size();}
 size_t getBytes(const char *key,void *p,size_t n){size_t count=std::min(n,data[key].size());memcpy(p,data[key].data(),count);if(wrongRead&&count)static_cast<uint8_t*>(p)[0]^=1;return count;}
 size_t putBytes(const char *key,const void *p,size_t n){size_t count=cut<0?n:std::min(n,size_t(cut));const auto *bytes=static_cast<const uint8_t*>(p);data[key]=std::vector<uint8_t>(bytes,bytes+count);if(lie&&count)data[key][0]^=1;return lie?n:count;}
};
struct Disk {
 std::map<std::string,std::vector<uint8_t>> files;int remaining=-1;bool corruptPromotion=false;
 bool exists(const std::string &s){return files.count(s);}
 bool allowed(){if(remaining==0)return false;if(remaining>0)--remaining;return true;}
 bool remove(const std::string &s){return allowed()&&files.erase(s);}
 bool rename(const std::string &a,const std::string &b){if(!allowed()||!exists(a)||exists(b))return false;files[b]=files[a];files.erase(a);if(corruptPromotion&&a=="next"&&b=="current")files[b][20]^=1;return true;}
};
std::vector<uint8_t> record(uint32_t raw){CaptureTracker t;t.newDay(20760);CaptureSource src;src.slave=220;src.baud=9600;src.parity='N';src.ptNum=src.ptDen=1;src.ct=80;src.profile=profiles::ACREL_ADL400_VERIFIED;assert(t.sample(captureTarget(20760,0),captureTarget(20760,0)+1,raw,src));std::vector<uint8_t> bytes(CAPTURE_RECORD_SIZE);encodeCaptureDay(t.day,bytes.data());return bytes;}
int main(){
 using namespace fort::lora;
 Settings base;assert(base.frequencyHz==915000000&&!base.enabled&&valid(base));base.enabled=true;assert(!valid(base));for(unsigned i=0;i<KEY_SIZE;++i)base.key[i]=uint8_t(i+1);assert(valid(base));
 {Settings bad=base;bad.nodeId=bad.gatewayId;assert(!valid(bad));bad=base;bad.frequencyHz=802999999;assert(!valid(bad));bad.frequencyHz=921400000;assert(valid(bad));bad.bandwidthKHz=126;assert(!valid(bad));}
 Store disk;SettingsStore first;assert(first.load(disk));assert(first.save(disk,base));Settings next=base;next.frequencyHz=921400000;next.nodeId=77;
 for(int cut=0;cut<int(SETTINGS_SIZE);++cut){Store partial=disk;SettingsStore writer;assert(writer.load(partial));partial.cut=cut;assert(!writer.save(partial,next));SettingsStore reboot;assert(reboot.load(partial)&&same(reboot.settings,base));}
 {Store partial=disk;SettingsStore writer;writer.load(partial);partial.lie=true;assert(!writer.save(partial,next));SettingsStore reboot;assert(reboot.load(partial)&&same(reboot.settings,base));}
 {Store full=disk;SettingsStore writer;writer.load(full);assert(writer.save(full,next));SettingsStore reboot;assert(reboot.load(full)&&same(reboot.settings,next));}
 uint8_t bytes[SETTINGS_SIZE];encodeSettings(base,1,bytes);Settings decoded;uint32_t generation;
 for(size_t i=0;i<sizeof(bytes);++i){bytes[i]^=1;assert(!decodeSettings(bytes,sizeof(bytes),decoded,generation));bytes[i]^=1;}
 {Store s;uint8_t payload[8]={1,2,3,4},verify[8];assert(verifiedPut(s,"record",payload,verify,sizeof(payload)));s.lie=true;assert(!verifiedPut(s,"record",payload,verify,sizeof(payload)));s.lie=false;s.wrongRead=true;assert(!verifiedPut(s,"record",payload,verify,sizeof(payload)));}
 {Day day;day.number=20760;assert(validLegacyDay(day));day.slots[0].quality=COMPLETE;assert(!validLegacyDay(day));day.slots[0].hasStart=day.slots[0].hasEnd=true;day.slots[0].start=10;day.slots[0].end=20;assert(validLegacyDay(day));day.slots[0].end=NAN;assert(!validLegacyDay(day));}
 auto old=record(9798),fresh=record(9800);CaptureDay restored;
 for(size_t cut=0;cut<fresh.size();++cut)assert(!decodeCaptureDay(fresh.data(),cut,restored));
 // Faults between every promotion operation preserve a valid old or fully staged new record.
 for(bool corrupt:{false,true})for(int stop=0;stop<9;++stop){Disk fs;fs.files["next"]=fresh;fs.files["current"]=old;fs.files["backup"]=old;if(corrupt)fs.files["current"][20]^=1;fs.remaining=stop;
  auto validFile=[&](const std::string &p){auto it=fs.files.find(p);CaptureDay out;return it!=fs.files.end()&&decodeCaptureDay(it->second.data(),it->second.size(),out);};
  installVerifiedArchive(fs,std::string("next"),std::string("current"),std::string("backup"),validFile);
  bool recoverable=false;for(const auto &file:fs.files){CaptureDay out;if(decodeCaptureDay(file.second.data(),file.second.size(),out))recoverable=true;}assert(recoverable);
 }
 {Disk fs;fs.files["next"]=fresh;fs.files["current"]=old;fs.corruptPromotion=true;
  auto validFile=[&](const std::string &p){auto it=fs.files.find(p);CaptureDay out;return it!=fs.files.end()&&decodeCaptureDay(it->second.data(),it->second.size(),out);};
  assert(!installVerifiedArchive(fs,std::string("next"),std::string("current"),std::string("backup"),validFile));
  assert(!validFile("current")&&validFile("backup")&&fs.files["backup"]==old);
 }
 {CaptureDay a,b;assert(decodeCaptureDay(old.data(),old.size(),a));b=a;++b.generation;assert(newerCaptureDay(b,a));CaptureTracker reboot;reboot.restore(a);assert(!reboot.continuous&&reboot.day.points[0].status==CAPTURE_RECORDED);}
 Frame sent;sent.type=HELLO;sent.network=1;sent.source=2;sent.destination=1;sent.sequence=7;sent.session=0x0807060504030201ULL;sent.length=3;sent.payload[0]=3;sent.payload[1]=7;sent.payload[2]=0;
 uint8_t frame[MAX_PACKET]={};size_t size=encodeFrame(sent,frame);assert(size==27&&frame[0]=='E'&&frame[2]==1&&frame[12]==7&&frame[16]==1&&frame[23]==8);Frame parsed;assert(decodeFrame(frame,size,parsed)&&parsed.session==sent.session&&parsed.payload[1]==7);
 for(size_t n=0;n<size;++n)assert(!decodeFrame(frame,n,parsed));frame[10]=MAX_PAYLOAD+1;assert(!decodeFrame(frame,size,parsed));frame[10]=3;frame[11]=1;assert(!decodeFrame(frame,size,parsed));frame[11]=0;
 Frame ack=sent;ack.type=ACK;ack.source=1;ack.destination=2;ack.length=1;ack.payload[0]=HELLO;assert(addressed(ack,base,sent.session)&&acknowledges(ack,sent));++ack.sequence;assert(!acknowledges(ack,sent));--ack.sequence;++ack.session;assert(!addressed(ack,base,sent.session)&&!acknowledges(ack,sent));ack.session=sent.session;ack.payload[0]=TELEMETRY;assert(!acknowledges(ack,sent));ack.payload[0]=HELLO;ack.source=77;assert(!addressed(ack,base,sent.session));
 {Frame hello=sent;hello.length=24;memset(hello.payload,0,sizeof(hello.payload));hello.payload[0]=3;hello.payload[1]=7;strcpy(reinterpret_cast<char*>(hello.payload+2),"3.4.0");const uint8_t expected[]={0x45,0x53,0x01,0x01,0x01,0x00,0x02,0x00,0x01,0x00,0x18,0x00,0x07,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x03,0x07,0x33,0x2e,0x34,0x2e,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};assert(encodeFrame(hello,frame)==sizeof(expected)&&!memcmp(frame,expected,sizeof(expected)));}
 DutyGate duty;assert(duty.remaining(0)==0);duty.transmitted(0xfffffff0U,200);assert(duty.remaining(0xfffffff0U)==20000);assert(duty.remaining(uint32_t(uint64_t(0xfffffff0U)+19999))==1);assert(duty.remaining(uint32_t(uint64_t(0xfffffff0U)+20000))==0);
 std::cout<<"PASS: 80 interrupted LoRa settings writes, corrupt-bank fallback, verified readback, every truncated checkpoint, interrupted valid/corrupt archive promotion, recovery generation, exact LE frame bounds, ACK/session/address checks and airtime rollover\n";
}
