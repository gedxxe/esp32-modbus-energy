#include "screen_saver.h"
#include "oled_frames.h"
#include "oled_menu.h"
#include "network.h"
#include "energy.h"
#include "checkpoint_log.h"
#include "debounce.h"
#include "recovery.h"
#include "scan.h"
#include "led.h"
#include "trace.h"
#include "clock_guard.h"
#include "command_policy.h"
#include "bounded_write.h"
#include <set>
#include <assert.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <map>
#include <string>
#undef assert
#define assert(x) do { if(!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " << #x << "\n"; std::exit(1); } } while(0)
using namespace fort;
constexpr uint32_t DAY=20748;
constexpr uint32_t BASE=DAY*86400;
void near(double a,double b){assert(fabs(a-b)<1e-7);}
void poll(Tracker &t,uint32_t a,uint32_t b,uint32_t step=2){for(uint32_t s=a;s<=b;s+=step)t.sample(BASE+s,1000.0+double(s)/3600.0*12.0);}
struct FakeFS {
  std::map<std::string,std::string> files;
  std::string failFrom,failTo;
  bool exists(const std::string &p){return files.count(p)!=0;}
  bool remove(const std::string &p){return files.erase(p)==1;}
  bool rename(const std::string &a,const std::string &b){
    if(a==failFrom&&b==failTo)return false;
    if(!exists(a)||exists(b))return false;
    files[b]=files[a];files.erase(a);return true;
  }
};
void testBoundedHTTP(){
  char bytes[3000]={};uint32_t time=0;unsigned calls=0,feeds=0;
  auto clock=[&](){return time;};auto ready=[](){return true;};
  auto service=[&](){++feeds;++time;};
  auto partial=[&](const char *,size_t count){++calls;assert(count<=1024);return int(count>113?113:count);};
  assert(boundedWrite(bytes,sizeof(bytes),0,5000,clock,ready,partial,service)==sizeof(bytes));assert(calls==feeds&&calls>3);
  time=0;feeds=0;auto blocked=[](const char *,size_t){return 0;};
  assert(boundedWrite(bytes,sizeof(bytes),0,10,clock,ready,blocked,service)==0&&time==10&&feeds==10);
  time=0;auto slow=[](const char *,size_t){return 1;};
  assert(boundedWrite(bytes,sizeof(bytes),0,10,clock,ready,slow,service)==10&&time==10); // Progress never renews deadline.
  time=UINT32_MAX-4;uint32_t started=time;
  assert(boundedWrite(bytes,sizeof(bytes),started,10,clock,ready,blocked,service)==0&&uint32_t(time-started)==10);
  auto fatal=[](const char *,size_t){return -1;};
  assert(boundedWrite(bytes,sizeof(bytes),time,10,clock,ready,fatal,service)==0);
}
void testScreenSaver(){
 using namespace fort;
 DisplaySettings settings;assert(validDisplay(settings));uint8_t record[DISPLAY_RECORD_SIZE];encodeDisplay(settings,record);DisplaySettings decoded;assert(decodeDisplay(record,sizeof(record),decoded)&&decoded.idleSeconds==60);record[4]^=1;assert(!decodeDisplay(record,sizeof(record),decoded));assert(!decodeDisplay(record,2,decoded));settings.idleSeconds=9;assert(!validDisplay(settings));settings.idleSeconds=0;assert(validDisplay(settings));
 IdleScreen idle;assert(!idle.poll(100,0,60,true).sleeping);assert(!idle.poll(60099,0,60,true).sleeping);assert(idle.poll(60100,0,60,true).sleeping);assert(idle.frame(60142,772,42)==1);
 auto result=idle.poll(60200,1,60,true);assert(result.woke&&result.consume&&!result.sleeping);assert(idle.poll(61200,1,60,true).consume);assert(idle.poll(61210,0,60,true).consume);idle.poll(61310,0,60,true);assert(!idle.poll(61420,2,60,true).consume);
 assert(idle.poll(121420,0,60,true).sleeping);result=idle.poll(121421,0,0,true);assert(result.woke&&!result.sleeping);
 IdleScreen wrap;wrap.poll(0xFFFFFF00u,0,60,true);assert(wrap.poll(uint32_t(uint64_t(0xFFFFFF00u)+60000),0,60,true).sleeping);
 uint8_t output[1024];assert(loadOLEDFrame(0,output));assert(loadOLEDFrame(771,output));assert(!loadOLEDFrame(772,output));
 const uint8_t run[]={130,0x7f},literal[]={0,0x33},shortRun[]={128};assert(decodeOLEDRun(run,2,output,3)&&output[2]==0x7f);assert(!decodeOLEDRun(run,2,output,2));assert(decodeOLEDRun(literal,2,output,1)&&output[0]==0x33);assert(!decodeOLEDRun(shortRun,1,output,1));
 std::cout<<"PASS: screensaver timeout/wake consumption/release/disable/rollover, display record CRC and actual compressed animation frames\n";
}
void testNetwork(){
  using namespace fort;
  NetworkSettings s{};assert(validNetwork(s));s.mode=NETWORK_CLIENT;assert(!validNetwork(s));
  strcpy(s.ssid,"Plant WiFi");assert(validNetwork(s));strcpy(s.password,"short");assert(!validNetwork(s));
  strcpy(s.password,"plant-key-123");assert(validNetwork(s));
  uint32_t parsed=0;assert(parseAPAddress("192.168.8.1",parsed)&&parsed==0xC0A80801);assert(!parseAPAddress("192.168.8.0",parsed));assert(!parseAPAddress("8.8.8.8",parsed));assert(!parseAPAddress("192.168.1.256",parsed));
  s.apIP=0xC0A80801;
  uint8_t bytes[NETWORK_RECORD_SIZE];encodeNetwork(s,bytes);NetworkSettings restored{};
  assert(decodeNetwork(bytes,sizeof(bytes),restored)&&!strcmp(restored.password,s.password)&&restored.mode==NETWORK_CLIENT);
  assert(restored.apIP==s.apIP);
  uint8_t legacy[104];memcpy(legacy,bytes,102);legacy[2]=1;uint16_t legacyCRC=networkCRC(legacy,102);legacy[102]=uint8_t(legacyCRC);legacy[103]=uint8_t(legacyCRC>>8);
  assert(decodeNetwork(legacy,sizeof(legacy),restored)&&restored.apIP==0xC0A80401);assert(!decodeNetwork(legacy,2,restored));
  assert(chooseAPAddress(0,0,0xC0A80801)==0xC0A80801);
  {OledNetworkMenu menu;NetworkSettings current;menu.enter();assert(menu.screen==OledNetworkMenu::MAIN);
    menu.handle(2,1,current,0);assert(menu.screen==OledNetworkMenu::MODE);menu.handle(1,1,current,0);assert(menu.handle(2,1,current,0)==OledNetworkMenu::SAVE_MODE&&menu.selected==1);
    menu.enter();menu.handle(1,1,current,0);assert(menu.handle(2,1,current,0)==OledNetworkMenu::SCAN);assert(menu.handle(2,1,current,0)==OledNetworkMenu::NONE);assert(menu.handle(2,1,current,1)==OledNetworkMenu::SELECT_SSID);
    menu.enterKey();for(int i=0;i<70;++i)menu.handle(2,1,current,0);assert(menu.length==63&&strlen(menu.key)==63);menu.handle(2,2,current,0);menu.handle(1,1,current,0);menu.handle(2,1,current,0);assert(menu.length==62);
    menu.enter();menu.handle(1,1,current,0);menu.handle(1,1,current,0);menu.handle(2,1,current,0);assert(menu.screen==OledNetworkMenu::IP);menu.handle(1,1,current,0);assert(menu.address()==0xC1A80401);menu.handle(0,1,current,0);menu.handle(2,2,current,0);assert(menu.handle(2,1,current,0)==OledNetworkMenu::SAVE_IP);
  }
  bytes[42]^=1;assert(!decodeNetwork(bytes,sizeof(bytes),restored));assert(!decodeNetwork(bytes,20,restored));
  memset(s.password,'a',64);s.password[64]=0;assert(validNetwork(s));s.password[10]='z';assert(!validNetwork(s));
  memset(s.ssid,'x',33);assert(!validNetwork(s));
  NetworkPolicy policy;policy.begin(NETWORK_CLIENT,0);assert(policy.apWanted(0));
  assert(!policy.update(29999,false)&&!policy.fallback);assert(policy.update(30000,false)&&policy.fallback);
  assert(!policy.update(30001,false));assert(!policy.update(31000,true)&&!policy.fallback);
  assert(policy.apWanted(45999)&&!policy.apWanted(46000));
  policy.update(46001,false);assert(policy.apWanted(46001));assert(!policy.fallback);
  policy.begin(NETWORK_DUAL,0);policy.update(1,true);assert(policy.apWanted(90000));
  policy.begin(NETWORK_AP,0);assert(!policy.update(60000,false)&&policy.apWanted(60000)&&!policy.fallback);
  policy.begin(NETWORK_CLIENT,0xFFFFFF00UL);assert(policy.update(uint32_t(uint64_t(0xFFFFFF00UL)+30000),false));
  assert(chooseAPAddress(0,0)==0xC0A80401);assert(chooseAPAddress(0xC0A80428,0)==0);assert(chooseAPAddress(0xC0A80428,0xFFFFFFF0)==0xC0A80501);assert(chooseAPAddress(0xC0A80428,0xFFFFFF00)==0xC0A80501);
  assert(chooseAPAddress(0xC0A80428,0xFFFF0000)==0x0A2A0001);
  std::cout<<"PASS: Wi-Fi settings/CRC/key validation, AP/client/dual retry/grace/loss/rollover and LAN overlap policy\n";
  assert(commandModeAllowed(SET_NETWORK,false,false,false));assert(!commandModeAllowed(SET_NETWORK,true,false,false));assert(!commandModeAllowed(SET_NETWORK,false,true,false));
}

void testMeterCheckpoints(){
  CaptureDay older,newer;older.number=DAY;newer.number=DAY+1;assert(newerCaptureDay(newer,older));
  newer.number=DAY;newer.revision=1;assert(newerCaptureDay(newer,older));
  assert(canRetainCaptureBeforeRollover(DAY,0,false,true));
  assert(!canRetainCaptureBeforeRollover(DAY,DAY-1,false,true));
  assert(!canRetainCaptureBeforeRollover(DAY,0,false,false));
  assert(canRetainCaptureBeforeRollover(DAY,DAY-1,true,false));
  CaptureTracker log;log.newDay(DAY);CaptureSource source;source.slave=1;source.baud=9600;source.parity='N';source.direction=0;source.ptNum=1;source.ptDen=1;source.ct=80;
  CaptureTracker quantized;quantized.newDay(DAY);uint32_t boundary=captureTarget(DAY,0);
  assert(!quantized.sample(boundary-1,boundary,1000000,source));uint32_t generation=quantized.day.generation;
  assert(quantized.sample(boundary,boundary,1000000,source));assert(quantized.day.generation==generation);
  uint32_t target=captureTarget(DAY,0);assert(CAPTURE_MINUTES[0]==360&&CAPTURE_MINUTES[4]==480&&CAPTURE_MINUTES[5]==1020&&CAPTURE_MINUTES[9]==1140);
  assert(!log.due(target-1)&&log.urgent(target-2)&&log.due(target));
  assert(!log.sample(target-1,target,1000000,source)); // Never label a pre-target read as a checkpoint.
  assert(log.sample(target,target+1,1000000,source));near(log.day.points[0].energy,80000);assert(!log.sample(target+1,target+2,1000001,source));near(log.day.points[0].energy,80000);
  for(uint32_t at=target+3;at<captureTarget(DAY,1);at+=2)log.sample(at,at,1000001,source);
  target=captureTarget(DAY,1);assert(log.sample(target+3,target+5,1000100,source));double difference=0;assert(captureDifference(log.day,0,difference));near(difference,8);
  target=captureTarget(DAY,2);assert(!log.sample(target+5,target+6,1000200,source));assert(log.tick(target+6)&&log.day.points[2].status==CAPTURE_MISSED);assert(!log.sample(target,target+1,1000200,source));
  log.interrupt();target=captureTarget(DAY,3);assert(log.sample(target,target,1000300,source));assert(!captureDifference(log.day,2,difference));
  uint8_t bytes[CAPTURE_RECORD_SIZE];encodeCaptureDay(log.day,bytes);CaptureDay restored;assert(decodeCaptureDay(bytes,sizeof(bytes),restored));near(restored.points[0].energy,80000);
  for(size_t i=0;i<sizeof(bytes);++i){bytes[i]^=1;assert(!decodeCaptureDay(bytes,sizeof(bytes),restored));bytes[i]^=1;}
  assert(!decodeCaptureDay(bytes,sizeof(bytes)-1,restored));
  CaptureTracker reboot;reboot.restore(log.day);assert(reboot.day.points[0].status==CAPTURE_RECORDED&&reboot.day.points[2].status==CAPTURE_MISSED);
  target=captureTarget(DAY,4);assert(reboot.sample(target,target+1,1000400,source));assert(!captureDifference(reboot.day,3,difference));assert(!captureDifference(reboot.day,4,difference));
  CaptureTracker clean;clean.newDay(DAY);for(uint32_t at=captureTarget(DAY,0);at<=captureTarget(DAY,9)+1;++at)clean.sample(at,at,1000000+(at-captureTarget(DAY,0))/18,source);
  for(unsigned i=0;i<CAPTURE_COUNT-1;++i){bool valid=captureDifference(clean.day,i,difference);assert(valid==(i!=4));if(valid)near(difference,8);}
  source.ct=0;assert(!log.sample(captureTarget(DAY,5),captureTarget(DAY,5),1000000,source));
  clean.newDay(DAY+1);assert(clean.tick(captureTarget(DAY+1,9)+6));for(auto &point:clean.day.points)assert(point.status==CAPTURE_MISSED);
  std::cout<<"PASS: ten raw kWh checkpoints, exact schedule, request-time guard, capture deadline, immutable rows, source/continuity deltas, reboot and CRC codec\n";
}

int main(){
  {Settings a;a.profile=profiles::ACREL_ADL400;assert(validSettings(a));a.baud=38400;assert(!validSettings(a));a.baud=9600;
   ScanController scan;assert(scan.begin(1,16,a,0));assert(scan.progress.total==240);
   while(scan.running()){assert(scan.progress.candidate.profile==profiles::ACREL_ADL400);assert(scan.progress.candidate.baud<=19200);scan.completeProbe(false,1);}
   Settings legacy;legacy.magic=0x46525432;legacy.slave=7;legacy.direction=DIRECTION_AUTO;assert(migrateLegacySettings(legacy));assert(legacy.profile==profiles::FORT&&legacy.slave==7&&legacy.direction==DIRECTION_AUTO);
   CaptureTracker captures;const uint32_t day=20754;captures.newDay(day);
   CaptureSource source;source.slave=1;source.baud=9600;source.parity='N';source.ptNum=2;source.ptDen=1;source.ct=40;source.profile=profiles::ACREL_ADL400;
   const uint32_t at=captureTarget(day,0);assert(captures.sample(at,at+1,123456,source));near(captures.day.points[0].energy,98764.8);
   uint8_t bytes[CAPTURE_RECORD_SIZE];encodeCaptureDay(captures.day,bytes);CaptureDay restored;assert(decodeCaptureDay(bytes,sizeof(bytes),restored));assert(restored.points[0].source.profile==profiles::ACREL_ADL400);
   auto saved=restored;bytes[24+38]=255;capturePut16(bytes+sizeof(bytes)-2,crc16(bytes,sizeof(bytes)-2));assert(!decodeCaptureDay(bytes,sizeof(bytes),restored));
   assert(restored.points[0].source.profile==saved.points[0].source.profile);
   // Schema 1 contains Fort captures only. Keep the old byte layout readable.
   source.profile=profiles::FORT;captures.newDay(day);assert(captures.sample(at,at,123456,source));encodeCaptureDay(captures.day,bytes);
   capturePut16(bytes+4,1);capturePut16(bytes+sizeof(bytes)-2,crc16(bytes,sizeof(bytes)-2));assert(decodeCaptureDay(bytes,sizeof(bytes),restored));near(restored.points[0].energy,9876.48);
   auto changed=source;changed.profile=profiles::ACREL_ADL400;assert(!sameCaptureSource(source,changed));
  }

  testMeterCheckpoints();
  testScreenSaver();
  testNetwork();
  testBoundedHTTP();
  {Settings legacy;legacy.magic=0x46525431;legacy.slave=7;legacy.baud=19200;legacy.parity='E';legacy.direction=255;
   assert(migrateLegacySettings(legacy)&&validSettings(legacy)&&legacy.direction==DIRECTION_MANUAL&&legacy.slave==7&&legacy.baud==19200&&legacy.parity=='E');
   Settings automatic=legacy;automatic.direction=DIRECTION_AUTO;ScanController scan;assert(scan.begin(7,7,automatic,0,true));assert(scan.progress.candidate.direction==DIRECTION_AUTO);}

  const uint8_t traceTx[]={1,3,0,0x82,0,2,0x64,0x23};
  uint8_t traceQuery[8];memcpy(traceQuery,traceTx,8);uint16_t qc=crc16(traceQuery,6);traceQuery[6]=qc;traceQuery[7]=qc>>8;
  uint8_t goodTrace[]={1,3,4,0,0,0xc3,0x17,0xea,0xcd};
  assert(inspectTrace(traceQuery,traceQuery,8,false,1,2)==TRACE_ECHO);
  assert(!validFrame(traceQuery,8,1,2));
  assert(inspectTrace(traceQuery,goodTrace,9,false,1,2)==TRACE_OK);
  assert(inspectTrace(traceQuery,goodTrace,0,false,1,2)==TRACE_TIMEOUT);
  assert(inspectTrace(traceQuery,goodTrace,3,false,1,2)==TRACE_SHORT);
  assert(inspectTrace(traceQuery,traceQuery,8,true,1,2)==TRACE_OVERFLOW);
  assert(inspectTrace(traceQuery,goodTrace,9,false,2,2)==TRACE_INVALID);
  uint8_t exception[]={1,0x83,2,0,0};uint16_t ec=crc16(exception,3);exception[3]=ec;exception[4]=ec>>8;
  assert(inspectTrace(traceQuery,exception,5,false,1,2)==TRACE_EXCEPTION);
  goodTrace[3]^=1;assert(inspectTrace(traceQuery,goodTrace,9,false,1,2)==TRACE_CRC);goodTrace[3]^=1;
  TraceCapture capture;capture.append(traceQuery,goodTrace,9,false,1,9600,'N',1,0x82,2,1,false);assert(capture.count==0);
  capture.begin();for(int n=0;n<TRACE_CAPACITY+3;++n)capture.append(traceQuery,traceQuery,8,false,n,9600,'N',1,0x82,2,1,true);
  assert(capture.count==TRACE_CAPACITY&&!capture.active&&capture.frames[0].atMs==0&&capture.frames[11].atMs==11);
  assert(capture.frames[0].scan&&capture.frames[0].result==TRACE_ECHO&&capture.frames[0].address==0x82);
  capture.begin();assert(capture.count==0&&capture.generation==2);capture.stop();
  capture.append(traceQuery,goodTrace,9,false,99,9600,'N',1,0x82,2,1,false);assert(capture.count==0);

  LedSettings light{};assert(validLed(light));assert(light.brightness==128);
  uint8_t bytes[LED_RECORD_SIZE];encodeLed(light,bytes);LedSettings restored{};
  assert(decodeLed(bytes,sizeof(bytes),restored)&&sameLed(light,restored));
  for(size_t i=0;i<sizeof(bytes);++i){bytes[i]^=1;assert(!decodeLed(bytes,sizeof(bytes),restored));bytes[i]^=1;}
  assert(!decodeLed(bytes,sizeof(bytes)-1,restored));
  light.periodMs=499;assert(!validLed(light));light.periodMs=30001;assert(!validLed(light));light.periodMs=6000;
  light.mode=LedMode(255);assert(!validLed(light));light.mode=LED_HUE;
  light.brightness=255;auto color=ledColor(light,0);assert(color.red==255&&color.green==0&&color.blue==0);
  color=ledColor(light,2000);assert(color.red==0&&color.green==255&&color.blue==0);
  color=ledColor(light,4000);assert(color.red==0&&color.green==0&&color.blue==255);
  light.mode=LED_BLINK;assert(ledColor(light,2999).green==184);assert(ledColor(light,3000).green==0);
  light.mode=LED_BREATHE;assert(ledColor(light,0).green==0);assert(ledColor(light,3000).green==184);assert(ledColor(light,6000).green==0);
  light.mode=LED_SOLID;light.brightness=0;assert(ledColor(light,100).green==0);
  light.brightness=128;assert(ledColor(light,100).green==92);
  light.mode=LED_OFF;assert(ledColor(light,100).green==0);
  LedMode parsed=LED_OFF;assert(parseLedMode("hue",parsed)&&parsed==LED_HUE);assert(!parseLedMode("rainbow",parsed));
  light.mode=LED_HUE;LedAnimation animation;animation.sample(light,UINT32_MAX-999);
  color=animation.sample(light,1000);assert(color.red==0&&color.green==128&&color.blue==0);
  light.mode=LED_SOLID;color=animation.sample(light,1001);assert(color.green==92);

  assert(std::string(qualityCode(COMPLETE))=="complete_estimated");
  assert(std::string(qualityCode(MISSING))=="missing");
  assert(std::string(qualityName(COMPLETE))=="lengkap-estimasi");
  assert(std::string(qualityCode(255))=="invalid");
  ScanController scan;Settings preferred;preferred.slave=7;preferred.baud=19200;preferred.parity='E';
  assert(!scan.begin(0,10,preferred,0));assert(!scan.begin(10,9,preferred,0));assert(!scan.begin(1,248,preferred,0));
  Settings bad=preferred;bad.baud=115200;assert(!scan.begin(1,247,bad,0));
  assert(scan.begin(1,247,preferred,UINT32_MAX-99));assert(scan.progress.total==4446);
  assert(scan.progress.candidate.slave==7&&scan.progress.candidate.baud==19200&&scan.progress.candidate.parity=='E');
  assert(!scan.begin(1,16,preferred,0));
  std::set<std::string> candidates;
  while(scan.running()){
    Settings c=scan.progress.candidate;assert(validSettings(c));
    if(scan.progress.tested<18)assert(c.slave==preferred.slave);
    if(scan.progress.tested==18)assert(c.slave==1&&c.baud==preferred.baud&&c.parity==preferred.parity);
    assert(candidates.insert(std::to_string(c.baud)+c.parity+std::to_string(c.slave)).second);
    scan.completeProbe(false,100);
  }
  assert(scan.progress.phase==SCAN_EXHAUSTED&&scan.progress.tested==4446&&candidates.size()==4446);
  assert(scan.progress.elapsedMs==200); // Monotonic millis rollover.
  assert(!scan.cancel(101));scan.completeProbe(true,101);assert(scan.progress.phase==SCAN_EXHAUSTED);
  assert(scan.begin(20,20,preferred,10));assert(scan.progress.candidate.slave==20&&scan.progress.total==18);
  scan.completeProbe(false,11);Settings found=scan.progress.candidate;scan.completeProbe(true,12);
  assert(scan.progress.phase==SCAN_FOUND&&scan.progress.tested==2&&scan.progress.result.slave==found.slave&&scan.progress.result.parity==found.parity);
  assert(!scan.cancel(13));assert(scan.begin(1,2,preferred,15));assert(scan.progress.result.slave==1);assert(scan.cancel(16));
  assert(scan.progress.phase==SCAN_CANCELLED&&scan.progress.tested==0);scan.completeProbe(true,17);assert(scan.progress.phase==SCAN_CANCELLED);
  assert(scan.begin(1,2,preferred,20));scan.completeProbe(false,21);assert(scan.cancel(22));assert(scan.progress.tested==1);
  assert(scan.begin(7,7,preferred,30));while(scan.running())scan.completeProbe(false,31);
  assert(scan.progress.phase==SCAN_EXHAUSTED&&scan.progress.tested==18);
  ScanController quick;assert(quick.begin(1,247,preferred,0,true));assert(quick.progress.total==247&&quick.progress.currentOnly);
  std::set<unsigned> quickIDs;while(quick.running()){assert(quick.progress.candidate.baud==preferred.baud&&quick.progress.candidate.parity==preferred.parity);assert(quickIDs.insert(quick.progress.candidate.slave).second);quick.completeProbe(false,100);}assert(quickIDs.size()==247);
  assert(quick.begin(7,7,preferred,0,true));quick.completeProbe(false,1);assert(quick.progress.tested==1&&quick.progress.phase==SCAN_EXHAUSTED);
  uint8_t ratios[]={0,1,0,1,0x27,0x0f};assert(validScanRatios(ratios));
  ratios[5]=0x10;assert(!validScanRatios(ratios));ratios[4]=0;ratios[5]=0;assert(!validScanRatios(ratios));

  const std::string tmp="pending",path="day",backup="day.bak";
  FakeFS disk;disk.files[tmp]="new";disk.files[path]="old";
  assert(installArchive(disk,tmp,path,backup));assert(disk.files[path]=="new");assert(!disk.exists(backup));
  disk=FakeFS{};disk.files[tmp]="new";disk.files[backup]="recoverable";disk.failFrom=backup;disk.failTo=path;
  assert(!installArchive(disk,tmp,path,backup));assert(disk.files[backup]=="recoverable");assert(disk.files[tmp]=="new");
  disk=FakeFS{};disk.files[tmp]="new";disk.files[path]="old";disk.failFrom=tmp;disk.failTo=path;
  assert(!installArchive(disk,tmp,path,backup));assert(disk.files[backup]=="old");assert(disk.files[tmp]=="new");
  disk=FakeFS{};disk.files[tmp]="new";disk.files[backup]="old";
  assert(installArchive(disk,tmp,path,backup));assert(disk.files[path]=="new");
  disk=FakeFS{};disk.files[tmp]="new";disk.files[path]="old";disk.failFrom=path;disk.failTo=backup;
  assert(!installArchive(disk,tmp,path,backup));assert(disk.files[path]=="old");
  assert(ratioPollDue(false,100,0,0));assert(!ratioPollDue(true,59999,0,0));
  assert(!ratioPollDue(true,61000,0,60000));assert(ratioPollDue(true,65000,0,60000));
  assert(!ratioPollDue(false,100,0,UINT32_MAX-1000));assert(ratioPollDue(false,6000,0,UINT32_MAX-1000));
  constexpr uint32_t WIB=7*3600;
  assert(validClockEpoch(CLOCK_MIN_UTC,WIB));
  assert(!validClockEpoch(CLOCK_MIN_UTC-1,WIB));
  assert(validClockEpoch(CLOCK_MAX_UTC-WIB,WIB));
  assert(!validClockEpoch(CLOCK_MAX_UTC-WIB+1,WIB));
  assert(!validClockEpoch(UINT32_MAX,WIB));
  assert(!validClockEpoch(CLOCK_MIN_UTC,UINT32_MAX));
  // Protection depends on retained records, not on an available/valid RTC flag.
  assert(protectsRecordedDay(BASE-WIB,WIB,DAY));
  assert(!protectsRecordedDay(BASE-WIB-1,WIB,DAY));
  assert(protectsRecordedDay(BASE-WIB+86400,WIB,DAY));
  assert(protectsRecordedDay(CLOCK_MIN_UTC,WIB,0));
  uint32_t effective=123;
  assert(resolveQueuedClockTime(BASE-WIB,1000,3500,WIB,DAY,effective)==CLOCK_ACCEPTED);
  assert(effective==BASE-WIB+2);
  assert(resolveQueuedClockTime(BASE-WIB,UINT32_MAX-499,1500,WIB,DAY,effective)==CLOCK_ACCEPTED);
  assert(effective==BASE-WIB+2); // Queue age compensates a millis() rollover.
  assert(resolveQueuedClockTime(BASE-WIB,1000,11000,WIB,DAY,effective)==CLOCK_ACCEPTED);
  assert(effective==BASE-WIB+10);
  effective=123;
  assert(resolveQueuedClockTime(BASE-WIB,1000,11001,WIB,DAY,effective)==CLOCK_STALE_REQUEST);
  assert(effective==123); // Refusal never publishes an unchecked replacement value.
  assert(resolveQueuedClockTime(BASE-WIB-100,1000,2000,WIB,DAY,effective)==CLOCK_BACKWARD_DAY);
  assert(effective==123);
  assert(resolveQueuedClockTime(CLOCK_MIN_UTC-1,1000,1000,WIB,0,effective)==CLOCK_INVALID_EPOCH);
  assert(resolveQueuedClockTime(CLOCK_MAX_UTC-WIB,1000,2000,WIB,0,effective)==CLOCK_INVALID_EPOCH);
  // A date crossing is evaluated at execution, after queue-age compensation.
  assert(resolveQueuedClockTime(BASE-WIB-1,1000,2000,WIB,DAY,effective)==CLOCK_ACCEPTED);
  assert(effective==BASE-WIB);

  const CommandType harmless[]={TOGGLE_MUTE,SET_LED,START_TRACE,STOP_TRACE};
  for(CommandType kind:harmless) {
    assert(safeDuringMaintenance(kind));
    for(unsigned mask=0;mask<8;++mask) {
      assert(commandModeAllowed(kind,mask&1,mask&2,mask&4));
      assert(commandContextValid(kind,1,99,1,90000,mask&1,mask&2,mask&4));
    }
  }
  const CommandType normalOnly[]={SET_TIME,SET_CONFIG,START_SCAN};
  for(CommandType kind:normalOnly) {
    assert(!safeDuringMaintenance(kind));
    assert(commandModeAllowed(kind,false,false,false));
    assert(commandModeAllowed(kind,false,false,true));
    assert(!commandModeAllowed(kind,true,false,false));
    assert(!commandModeAllowed(kind,false,true,false));
    assert(commandContextValid(kind,7,7,1000,11000,false,false,false));
    assert(!commandContextValid(kind,7,7,1000,11001,false,false,false));
    assert(!commandContextValid(kind,7,8,1000,1000,false,false,false));
    assert(!commandContextValid(kind,7,7,1000,1000,true,false,false));
    assert(!commandContextValid(kind,7,7,1000,1000,false,true,false));
    assert(commandContextValid(kind,7,7,UINT32_MAX-499,1500,false,false,false));
    assert(!commandContextValid(kind,7,7,UINT32_MAX-499,10000,false,false,false));
  }
  assert(!commandModeAllowed(CANCEL_SCAN,false,false,false));
  assert(commandModeAllowed(CANCEL_SCAN,false,true,false));
  assert(!commandModeAllowed(CANCEL_SCAN,true,true,false));
  assert(!commandContextValid(CANCEL_SCAN,3,4,1000,1001,false,true,false));
  assert(!commandModeAllowed(APPLY_SCAN,false,false,false));
  assert(commandModeAllowed(APPLY_SCAN,false,false,true));
  assert(!commandModeAllowed(APPLY_SCAN,false,true,true));
  assert(!commandModeAllowed(APPLY_SCAN,true,false,true));
  assert(!commandModeAllowed(CommandType(255),false,false,false));
  assert(!commandContextValid(CommandType(255),0,0,0,0,false,false,false));

  WiFiScanPolicy wifi;
  assert(wifi.begin(0));assert(!wifi.due(299));assert(wifi.due(300));
  wifi.attempt(300,false);assert(wifi.running&&!wifi.radio&&wifi.attempts==1);
  assert(!wifi.due(1049));assert(wifi.due(1050));wifi.attempt(1050,true);
  assert(wifi.radio&&!wifi.expired(13049));assert(wifi.expired(13050));
  wifi.finish(13050,false,"radio_timeout");assert(wifi.failed&&!wifi.running);
  assert(!wifi.begin(14000));assert(wifi.begin(14550));
  wifi.attempt(14850,false);wifi.attempt(15600,false);wifi.attempt(16350,false);
  assert(wifi.failed&&!wifi.running&&wifi.attempts==3);
  assert(wifi.begin(18000));wifi.attempt(18300,true);wifi.finish(22000,true);
  assert(!wifi.failed&&!wifi.running&&!strcmp(wifi.error,""));
  assert(wifi.begin(UINT32_MAX-100));assert(wifi.due(199));wifi.attempt(199,true);
  assert(!wifi.expired(12198));assert(wifi.expired(12199));
  wifi.finish(12199,true);
  assert(wifi.begin(14000));wifi.attempt(14300,true);wifi.retry(26000,"radio_failed");
  assert(wifi.running);assert(wifi.expired(34000));

  Debouncer button;
  assert(button.update(false,100)==0);assert(button.update(true,105)==0);
  assert(button.update(false,110)==0);assert(button.update(false,145)==0);
  assert(button.update(true,200)==0);assert(button.update(false,205)==0);
  assert(button.update(true,210)==0);assert(button.update(true,244)==0);
  assert(button.update(true,245)==1);assert(button.update(true,300)==0);
  assert(button.update(false,400)==0);assert(button.update(false,435)==0);
  assert(button.update(false,1334)==0);assert(button.update(false,1335)==2);
  assert(button.update(false,2335)==0);assert(button.update(true,2400)==0);assert(button.update(true,2435)==0);
  Debouncer wrap;assert(wrap.update(false,UINT32_MAX-20)==0);assert(wrap.update(false,15)==0);
  assert(wrap.update(true,30)==0);assert(wrap.update(true,65)==1);
  // Published Fort manual page 24, Table 13 and Table 14.
  const uint8_t query[]={1,3,0,0x58,0,2};assert(crc16(query,6)==0xd845);
  const uint8_t manualTypo[]={1,3,4,0,0,0xc3,0x17,0x72,0xe5};
  assert(!validFrame(manualTypo,9,1,2)); // Manual's response CRC is inconsistent.
  uint8_t response[]={1,3,4,0,0,0xc3,0x17,0xea,0xcd};
  assert(validFrame(response,9,1,2));near(be32(response+3)*.0001,4.9943);
  for(size_t i=0;i<sizeof(response);++i){response[i]^=1;assert(!validFrame(response,9,1,2));response[i]^=1;}
  assert(!validFrame(response,8,1,2));assert(!validFrame(response,9,2,2));assert(!validFrame(response,9,1,3));
  Tracker t;t.newDay(DAY);poll(t,21596,32404);
  for(int i=0;i<6;++i){assert(t.day.slots[i].quality==COMPLETE);near(t.day.slots[i].end-t.day.slots[i].start,6);}
  poll(t,59396,64804);
  for(int i=6;i<9;++i){assert(t.day.slots[i].quality==COMPLETE);near(t.day.slots[i].end-t.day.slots[i].start,6);}
  // Boundaries between samples: linear interpolation conserves energy.
  t.newDay(DAY);poll(t,21599,23403);assert(t.day.slots[0].quality==COMPLETE);near(t.day.slots[0].end-t.day.slots[0].start,6);
  // Late startup cannot produce a full interval; following slot recovers.
  t.newDay(DAY);poll(t,22000,25204);assert(t.day.slots[0].quality==PARTIAL);assert(t.day.slots[1].quality==COMPLETE);
  // Gap in middle; both endpoints alone are insufficient for quality assurance.
  t.newDay(DAY);poll(t,21596,22200);poll(t,22220,23404);assert(t.day.slots[0].quality==PARTIAL);
  // Lost boundary cannot be allocated retroactively across half hours.
  t.newDay(DAY);poll(t,21596,23390);poll(t,23420,25204);assert(t.day.slots[0].quality==PARTIAL);assert(t.day.slots[1].quality==PARTIAL);
  // Reboot: restore data but break continuity. Finished rows survive.
  t.newDay(DAY);poll(t,21596,24000);Day saved=t.day;Tracker reboot;reboot.day=saved;reboot.interrupt();poll(reboot,24002,25204);assert(reboot.day.slots[0].quality==COMPLETE);assert(reboot.day.slots[1].quality==PARTIAL);
  // RTC backwards and duplicate timestamps do not advance energy.
  t.newDay(DAY);t.sample(BASE+21600,100);assert(!t.sample(BASE+21599,101));assert(!t.sample(BASE+21600,101));near(t.prevEnergy,100);
  t.interrupt();t.sample(BASE+21601,100);t.sample(BASE+23400,101);assert(t.day.slots[0].quality==PARTIAL);
  // Counter reset / NaN / negative / wrong day.
  t.newDay(DAY);t.sample(BASE+21600,100);assert(!t.sample(BASE+21602,NAN));assert(!t.sample(BASE+21602,-1));assert(!t.sample(BASE+86400,101));t.sample(BASE+21602,0);t.sample(BASE+23400,1);assert(t.day.slots[0].quality==PARTIAL);
  // Missing window finalized even if no meter readings ever arrive.
  t.newDay(DAY);t.tick(BASE+32416);for(int i=0;i<6;++i)assert(t.day.slots[i].quality==MISSING);for(int i=6;i<9;++i)assert(t.day.slots[i].quality==WAITING);
  // Exact maximum gap remains acceptable; over-limit boundary does not.
  t.newDay(DAY);poll(t,21600,23400,15);assert(t.day.slots[0].quality==COMPLETE);
  t.newDay(DAY);poll(t,21600,23392,16);t.sample(BASE+23400,107);assert(t.day.slots[0].quality==PARTIAL);
  // Midnight does not silently mix counters from different dates.
  assert(!t.sample(BASE+86400,108));t.newDay(DAY+1);assert(!t.previous);assert(t.day.slots[0].quality==WAITING);
  std::cout<<"PASS: raw capture bounds/freeze/stop/echo/CRC/exception/frame validation, RGB modes/brightness/CRC storage/millis wrap, scan enumeration/ranges/cancel/restart/found/millis wrap/ratio validation, archive fault recovery, ratio retry/backoff, protected record dates/queued clock age/calendar bounds, command mode/revision/age arbitration, button bounce/long press/millis wrap, CRC/manual frame, all 9 windows, interpolation, gaps, reboot, RTC, reset, missing data, date rollover\n";
}
