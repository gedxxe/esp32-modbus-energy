#include "iot_contract.h"
#include "iot_archive_path.h"
#include "iot_provision.h"
#include "iot_tls_diagnostics.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
using namespace fort;
int main(){
  iot::Binding b;strcpy(b.device,"dev_source");strcpy(b.meter,"meter_1");strcpy(b.profile,"fort");
  assert(iot::valid(b)&&iot::identifier("n-0_1"));
  // The same validator runs on ESP32 and native CI with synthetic credentials.
  assert(iot::tlsAllocationFailure(32512));
  assert(iot::tlsAllocationFailure(-32512));
  assert(!iot::tlsAllocationFailure(0));
  assert(!iot::tlsAllocationFailure(0x2700)); // X509 error is not a memory error.
  assert(!iot::tlsAllocationFailure(0x7200)); // Invalid record is not a memory error.
  const std::string fakeKey(43,'A');
  const char *validEndpoint="https://synthetic.invalid/api/iot-ingest";
  assert(iot::validateProvision(true,b,fakeKey.c_str(),validEndpoint,true)==nullptr);
  assert(!strcmp(iot::validateProvision(true,b,fakeKey.c_str(),validEndpoint,false),"iot_header_not_compiled"));
  assert(!strcmp(iot::validateProvision(true,b,"short",validEndpoint,true),"iot_bearer_length_invalid"));
  std::string badKey=fakeKey;badKey[8]='.';
  assert(!strcmp(iot::validateProvision(true,b,badKey.c_str(),validEndpoint,true),"iot_bearer_format_invalid"));
  assert(!strcmp(iot::validateProvision(true,b,fakeKey.c_str(),"http://synthetic.invalid/api/iot-ingest",true),"iot_endpoint_https_required"));
  assert(!strcmp(iot::validateProvision(true,b,fakeKey.c_str(),"https://synthetic.invalid/wrong",true),"iot_endpoint_path_invalid"));
  assert(!strcmp(iot::validateProvision(true,b,fakeKey.c_str(),"https://synthetic.invalid/api/iot-ingest?x=1",true),"iot_endpoint_path_invalid"));
  assert(!strcmp(iot::validateProvision(true,b,fakeKey.c_str(),"https://user@synthetic.invalid/api/iot-ingest",true),"iot_endpoint_url_invalid"));

  // Regression: main.cpp stores /c<day>.bin and /c<day>.bin.bak.
  char archive[40]={};
  assert(iot::archivePath(20762,false,archive,sizeof(archive)));
  assert(!strcmp(archive,"/c20762.bin"));
  assert(iot::archivePath(20762,true,archive,sizeof(archive)));
  assert(!strcmp(archive,"/c20762.bin.bak"));
  assert(!iot::archivePath(0,false,archive,sizeof(archive)));
  char tiny[5];
  assert(!iot::archivePath(20762,false,tiny,sizeof(tiny)));
  assert(!iot::identifier("_bad")&&!iot::identifier("space invalid"));
  char date[21];assert(iot::utcText(1793836800UL,date)&&!strcmp(date,"2026-11-05T00:00:00Z"));
  assert(iot::utcText(1793836802UL,date)&&!strcmp(date,"2026-11-05T00:00:02Z"));
  assert(!iot::utcText(0,date));
  char decimal[40];assert(iot::primaryText(7838.9,decimal)&&!strcmp(decimal,"7838.900000"));
  assert(!iot::primaryText(-1,decimal)&&!iot::primaryText(NAN,decimal));
  CaptureTracker tracker;tracker.newDay(20762);
  CaptureSource src;src.profile=profiles::FORT;src.meterId=1;src.slave=1;src.parity='N';src.baud=9600;src.ptNum=src.ptDen=src.ct=1;
  const uint32_t scheduled=captureTarget(20762,4);
  assert(scheduled==1793840400UL); // 08:00 WIB is 01:00 UTC.
  assert(!iot::checkpointWindowProtected(scheduled-16));
  assert(iot::checkpointWindowProtected(scheduled-15));
  assert(iot::checkpointWindowProtected(scheduled));
  assert(iot::checkpointWindowProtected(scheduled+CAPTURE_WINDOW+15));
  assert(!iot::checkpointWindowProtected(scheduled+CAPTURE_WINDOW+16));
  assert(tracker.sample(scheduled+1,scheduled+2,1000,src));
  iot::Event e;assert(iot::checkpoint(tracker.day,4,b,e));
  assert(!strcmp(e.id,"cp_20762_04")&&e.size==strlen(e.json));
  assert(std::string(e.json).find("\"active_energy_kwh\":\"1.000000\"")!=std::string::npos);
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
  // Website v0.11.47 accepts an optional one-channel current_a decimal string.
  iot::Event withCurrent{};
  assert(iot::telemetry(scheduled+2,7838.9,b,0x1234,2,withCurrent,true,12.345678));
  assert(std::string(withCurrent.json).find("\"current_a\":\"12.345678\"")!=std::string::npos);
  assert(std::string(withCurrent.json).find("current_l1_a")==std::string::npos);
  assert(std::string(withCurrent.json).find("\"active_energy_kwh\":\"7838.900000\"")!=std::string::npos);
  assert(withCurrent.size==strlen(withCurrent.json));
  const std::string frozen=withCurrent.json;
  // Retry keeps the already-serialized event, even if a later sample changes.
  iot::Event laterSample{};
  assert(iot::telemetry(scheduled+3,7839.0,b,0x1234,3,laterSample,true,40));
  assert(std::string(withCurrent.json)==frozen);
  assert(iot::telemetry(scheduled+2,7838.9,b,0x1234,2,laterSample,true,12.345678));
  assert(std::string(laterSample.json)==frozen); // exact duplicate body and event ID

  iot::Event zero{};
  assert(iot::telemetry(scheduled+2,7838.9,b,0x1234,4,zero,true,0));
  assert(std::string(zero.json).find("\"current_a\":\"0.000000\"")!=std::string::npos);
  iot::Event minusZero{};
  assert(iot::telemetry(scheduled+2,7838.9,b,0x1234,4,minusZero,true,-0.0));
  assert(!strcmp(minusZero.json,zero.json)); // site's decimal grammar rejects a -0 prefix
  const double invalids[]={-1.0,double(NAN),double(INFINITY)};
  for(double invalid:invalids){
    iot::Event absent{};
    assert(iot::telemetry(scheduled+2,7838.9,b,0x1234,5,absent,true,invalid));
    assert(std::string(absent.json).find("current_a")==std::string::npos);
  }
  iot::Event unused{};
  assert(iot::telemetry(scheduled+2,7838.9,b,0x1234,6,unused,false,10));
  assert(std::string(unused.json).find("current_a")==std::string::npos);
  assert(std::string(repeat.json).find("current_a")==std::string::npos); // historical checkpoint stays kWh-only
  assert(!iot::freshCurrent(false,profiles::FORT,10,900,1000));
  assert(!iot::freshCurrent(true,profiles::HABUER,10,900,1000)); // no current register
  assert(!iot::freshCurrent(true,profiles::FORT,10,0,1000));
  assert(!iot::freshCurrent(true,profiles::FORT,-1,900,1000));
  assert(!iot::freshCurrent(true,profiles::FORT,NAN,900,1000));
  assert(!iot::freshCurrent(true,profiles::FORT,10,900,16001));
  assert(iot::freshCurrent(true,profiles::FORT,0,900,1000)); // measured zero is valid
  assert(iot::freshCurrent(true,profiles::ACREL_ADL400_VERIFIED,3,900,1000));
  assert(iot::freshCurrent(true,profiles::ACREL_ACR,3,900,1000));
  assert(iot::freshCurrent(true,profiles::ACREL_APM,3,900,1000));
  assert(iot::freshCurrent(true,profiles::FORT,3,UINT32_MAX-1000,100)); // millis wraparound

  // Presence contract v0.11.48: an authenticated contact, NOT a reading.
  iot::Event pulse{};
  assert(iot::heartbeat(b,pulse));
  assert(pulse.id[0]==0&&pulse.size==strlen(pulse.json));
  const std::string heartbeatBody=pulse.json;
  assert(heartbeatBody=="{\"schema_version\":1,\"event_type\":\"heartbeat\",\"device_id\":\"dev_source\",\"meter_id\":\"meter_1\",\"meter_profile\":\"fort\"}");
  for(const char *forbidden:{"event_id","readings","captured_at","started_at","scheduled_at","timestamp","quality","active_energy_kwh"})
    assert(heartbeatBody.find(forbidden)==std::string::npos);
  iot::Binding missingHeartbeatBinding{};
  assert(!iot::heartbeat(missingHeartbeatBinding,pulse));
  assert(iot::heartbeat(b,pulse)); // invalid attempt did not retain a partial payload
  assert(iot::heartbeatAckFields(200,"heartbeat","dev_source",b));
  assert(!iot::heartbeatAckFields(201,"heartbeat","dev_source",b));
  assert(!iot::heartbeatAckFields(200,"telemetry","dev_source",b));
  assert(!iot::heartbeatAckFields(200,"heartbeat","other",b));
  assert(!iot::heartbeatAckFields(200,nullptr,"dev_source",b));
  assert(!iot::heartbeatAckFields(200,"heartbeat",nullptr,b));
  assert(iot::heartbeatDue(12345,0));
  assert(!iot::heartbeatDue(60999,1000));
  assert(iot::heartbeatDue(61000,1000));
  assert(!iot::heartbeatDue(1000,UINT32_MAX-1000)); // wraparound
  assert(iot::heartbeatDue(61000,UINT32_MAX-1000));
  // A heartbeat has no day/slot and cannot affect persisted checkpoint IDs.
  assert(pulse.day==0&&pulse.index==0);
  assert(std::string(repeat.json).find("\"event_type\":\"checkpoint\"")!=std::string::npos);

  // Maximum website identifiers and energy/current decimals still fit in fixed payload.
  iot::Binding maximum{};
  const std::string longest(64,'X');
  strcpy(maximum.device,longest.c_str());strcpy(maximum.meter,longest.c_str());
  strcpy(maximum.profile,longest.c_str());
  assert(iot::valid(maximum));
  char limit[40];assert(iot::primaryText(1000000000000.0,limit));
  assert(!strcmp(limit,"1000000000000.000000"));
  assert(!iot::primaryText(1000000000000.1,limit));
  iot::Event largest{};
  assert(iot::telemetry(scheduled+2,1000000000000.0,maximum,0xffffffff,0xffffffff,
                        largest,true,1000000000000.0));
  assert(largest.size==strlen(largest.json)&&largest.size<sizeof(largest.json));
  assert(strlen(largest.id)<=64);
  iot::Event largestHeartbeat{};
  assert(iot::heartbeat(maximum,largestHeartbeat));
  assert(largestHeartbeat.size==strlen(largestHeartbeat.json)&&
         largestHeartbeat.size<sizeof(largestHeartbeat.json));
  assert(largestHeartbeat.id[0]==0);
  std::cout<<"IoT website v1 wire-contract tests passed\n";
  return 0;
}
