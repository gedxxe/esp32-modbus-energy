#include "energy.h"
#include "debounce.h"
#include "recovery.h"
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
int main(){
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
  std::cout<<"PASS: archive fault recovery, ratio retry/backoff, button bounce/long press/millis wrap, CRC/manual frame, all 9 windows, interpolation, gaps, reboot, RTC, reset, missing data, date rollover\n";
}
