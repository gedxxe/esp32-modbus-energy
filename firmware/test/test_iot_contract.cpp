#include "iot_contract.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
using namespace fort;
int main(){
  iot::Binding b;strcpy(b.device,"dev_source");strcpy(b.meter,"meter_1");strcpy(b.profile,"fort");
  assert(iot::valid(b)&&iot::identifier("n-0_1"));
  assert(!iot::identifier("_bad")&&!iot::identifier("space invalid"));
  char date[21];assert(iot::utcText(1793836800UL,date)&&!strcmp(date,"2026-11-05T00:00:00Z"));
  assert(iot::utcText(1793836802UL,date)&&!strcmp(date,"2026-11-05T00:00:02Z"));
  assert(!iot::utcText(0,date));
  char decimal[40];assert(iot::primaryText(7838.9,decimal)&&!strcmp(decimal,"7838.900000"));
  assert(!iot::primaryText(-1,decimal)||!iot::primaryText(NAN,decimal));
  CaptureTracker tracker;tracker.newDay(20762);
  CaptureSource src;src.profile=profiles::FORT;src.meterId=1;src.slave=1;src.parity='N';src.baud=9600;src.ptNum=src.ptDen=src.ct=1;
  const uint32_t scheduled=captureTarget(20762,4);
  assert(scheduled==1793869200UL); // 08:00 WIB is 01:00 UTC.
  assert(tracker.sample(scheduled+1,scheduled+2,1000,src));
  iot::Event e;assert(iot::checkpoint(tracker.day,4,b,e));
  assert(!strcmp(e.id,"cp_20762_04")&&e.size==strlen(e.json));
  assert(std::string(e.json).find("\"active_energy_kwh\":\"1000.000000\"")!=std::string::npos);
  assert(std::string(e.json).find("\"scheduled_at\":\"2026-11-05T01:00:00Z\"")!=std::string::npos);
  assert(std::string(e.json).find("\"started_at\":\"2026-11-05T01:00:01Z\"")!=std::string::npos);
  assert(std::string(e.json).find("\"captured_at\":\"2026-11-05T01:00:02Z\"")!=std::string::npos);
  assert(std::string(e.json).find("reactive_energy_kvarh")==std::string::npos);
  iot::Event repeat;assert(iot::checkpoint(tracker.day,4,b,repeat));
  assert(!strcmp(e.json,repeat.json)); // deterministic retries
  assert(!iot::checkpoint(tracker.day,3,b,e));
  iot::Binding mismatched=b;strcpy(mismatched.profile,"habuer");
  assert(!iot::checkpoint(tracker.day,4,mismatched,e)); // refuse mismatched meter binding
  auto bad=tracker.day;bad.points[4].capturedUTC=scheduled+7;
  assert(!iot::checkpoint(bad,4,b,e)); // cannot substitute a late sample
  bad=tracker.day;bad.points[4].startedUTC=scheduled-1;
  assert(!iot::checkpoint(bad,4,b,e));
  bad=tracker.day;bad.points[4].energy=NAN;
  assert(!iot::checkpoint(bad,4,b,e));
  assert(iot::later(20762,4,20762,3)&&!iot::later(20762,4,20762,4));
  assert(iot::later(20763,0,20762,9));
  iot::Event live;assert(iot::telemetry(scheduled+2,1000.123456,b,0x1234,1,live));
  assert(std::string(live.json).find("\"event_type\":\"telemetry\"")!=std::string::npos);
  assert(std::string(live.json).find("scheduled_at")==std::string::npos);
  assert(!iot::telemetry(scheduled,1,b,0,1,live));
  iot::Binding different=b;strcpy(different.meter,"meter_2");
  assert(iot::bindingFingerprint(different)!=iot::bindingFingerprint(b));
  std::cout<<"IoT website v1 wire-contract tests passed\n";
  return 0;
}
