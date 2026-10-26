#include <assert.h>
#include <iostream>
#include <fstream>
#include <iterator>
#include "diagnostic_engine.h"
#include "flow.h"
using namespace fort;
std::vector<uint8_t> reply(std::initializer_list<uint8_t> bytes){std::vector<uint8_t> result(bytes);uint16_t c=crc16(result.data(),result.size());result.push_back(uint8_t(c));result.push_back(uint8_t(c>>8));return result;}
UartFlow flow;
void observe(uint8_t kind,const DiagnosticEngine::Link &link,uint16_t address,uint8_t words,const uint8_t *tx,const uint8_t *rx,size_t size,bool overflow){
  flow.append(kind,micros(),link.baud,link.parity,link.id,address,words,kind<3?tx:rx,kind<3?8:size,overflow,false,kind==4?inspectTrace(tx,rx,size,overflow,link.id,words):TRACE_INVALID);
}
int main(){
  HardwareSerial port(1);char log[512];DiagnosticEngine engine(port,log,sizeof(log));engine.event=observe;
  uint8_t data[6]={};
  port.replies.push_back(reply({1,3,0,0x34,0,1}));port.scheduled.push_back({testMicros+100000,reply({1,3,2,0,7})});
  assert(engine.readRegisters(engine.current,0x34,1,data));assert(data[1]==7&&engine.evidence.echoes==1&&engine.evidence.data==1);
  assert(flow.count==6&&flow.at(0).kind==1&&flow.at(1).kind==2&&flow.at(2).kind==3&&flow.at(3).result==TRACE_ECHO&&flow.at(5).result==TRACE_OK);
  assert(flow.at(0).seq<flow.at(5).seq&&flow.at(0).atUs<=flow.at(5).atUs);
  port.replies.push_back(reply({1,0x83,2}));assert(!engine.readRegisters(engine.current,0x82,2,data));assert(engine.lastReply==DiagnosticEngine::REPLY_EXCEPTION);
  assert(engine.evidence.energyOK==0);engine.reportEvidence();assert(engine.logDiscarded>0);
  char copy[513];engine.copyLog(copy,sizeof(copy));assert(strlen(copy)<=512&&std::string(copy).find("REPORT_END")!=std::string::npos);
  engine.serialOutput=true;for(unsigned n=0;n<10;++n)engine.drainSerial();assert(Serial.output.find("REPORT_END")!=std::string::npos);
  engine.clearLog();engine.evidence=DiagnosticEngine::Evidence{};engine.command("decode off");engine.command("plan clear");engine.command("plan add 0080 1");
  port.replies.push_back({});port.replies.push_back(reply({1,3,2,0x13,0x88}));engine.command("diagnose");assert(engine.evidence.data==1&&engine.evidence.requests==2);
  for(unsigned i=0;i<80;++i)flow.append(1,i,9600,'N',1,0x34,1,nullptr,0,false,false,TRACE_INVALID);
  assert(flow.count==32&&flow.at(31).seq==flow.sequence&&flow.at(0).seq==flow.sequence-31);
  flow.sequence=UINT32_MAX;flow.append(2,0,9600,'N',1,0,0,nullptr,0,false,false,TRACE_INVALID);assert(flow.sequence==0);
  // Production acceptance must reject every corrupt/unrelated/oversized form.
  for(unsigned scenario=0;scenario<6;++scenario){HardwareSerial badPort(1);DiagnosticEngine reader(badPort);uint8_t out[6]={};
    auto bytes=reply({1,3,2,0,7});
    if(scenario==0)bytes[3]^=1;
    if(scenario==1)bytes=reply({2,3,2,0,7});
    if(scenario==2)bytes=reply({1,4,2,0,7});
    if(scenario==3)bytes=reply({1,3,4,0,0,0,7});
    if(scenario==4)bytes={1,3};
    if(scenario==5){bytes.resize(90,0x55);}
    badPort.replies.push_back(bytes);assert(!reader.readRegisters(reader.current,0x34,1,out));assert(reader.lastReply==DiagnosticEngine::REPLY_NONE&&reader.evidence.data==0);
  }
  {HardwareSerial badPort(1);DiagnosticEngine reader(badPort);uint8_t out[6]={};auto exception=reply({1,0x83,2});exception[4]^=1;badPort.replies.push_back(exception);assert(!reader.readRegisters(reader.current,0x34,1,out));assert(reader.lastReply==DiagnosticEngine::REPLY_NONE);}
  {HardwareSerial partial(1);DiagnosticEngine reader(partial);reader.event=observe;partial.writeLimit=2;uint8_t out[2];assert(!reader.readRegisters(reader.current,0x34,1,out));assert(flow.at(flow.count-1).kind==8&&reader.evidence.txErrors==1);}
  {HardwareSerial stale(1);DiagnosticEngine reader(stale);uint8_t out[2];reader.openLink(reader.current);stale.incoming={1,3,2,0,7};assert(!reader.readRegisters(reader.current,0x34,1,out));assert(reader.evidence.staleBytes==5&&reader.evidence.data==0);}
  {HardwareSerial rejected(1);DiagnosticEngine reader(rejected);reader.command("energy FFFF");reader.command("probe FFFF 2");reader.command("id 0");assert(reader.options.energyAddress==0x82&&reader.options.probeAddress==0x34&&reader.current.id==1&&rejected.transmitted.empty());}
  {HardwareSerial merged(1);DiagnosticEngine reader(merged);uint8_t out[2];auto bytes=reply({1,3,0,0x34,0,1});auto following=reply({1,3,2,0,7});bytes.insert(bytes.end(),following.begin(),following.end());merged.replies.push_back(bytes);assert(!reader.readRegisters(reader.current,0x34,1,out));assert(reader.evidence.echoPrefixes==1&&reader.evidence.data==0);}
  {HardwareSerial late(1);DiagnosticEngine reader(late);reader.event=observe;reader.options.responseMs=50;late.scheduled.push_back({testMicros+53000,reply({1,3,2,0,7})});uint8_t out[2];assert(!reader.readRegisters(reader.current,0x34,1,out));assert(reader.evidence.data==0&&flow.at(flow.count-1).kind==5);}
  {HardwareSerial lateOld(1);DiagnosticEngine reader(lateOld);uint8_t out[2]={};lateOld.scheduled.push_back({testMicros+3000,reply({1,3,2,0x12,0x34})});
    assert(!reader.readRegisters(reader.current,0x34,1,out));assert(reader.evidence.staleBytes==7&&reader.evidence.data==0&&reader.evidence.timeouts==1);}
  {HardwareSerial noisy(1);DiagnosticEngine reader(noisy);reader.options.responseMs=50;uint8_t out[2];uint64_t started=testMicros;
    for(unsigned i=0;i<100;++i)noisy.scheduled.push_back({testMicros+uint64_t(i)*1000,{0x55}});
    assert(!reader.readRegisters(reader.current,0x34,1,out));assert(noisy.transmitted.empty()&&reader.evidence.busBusy==1&&reader.evidence.requests==0&&testMicros-started<=51000);}
  {HardwareSerial turnaround(1);DiagnosticEngine reader(turnaround);reader.current.baud=19200;reader.current.parity='E';uint8_t out[2];
    turnaround.replies.push_back(reply({1,3,0,0x34,0,1}));
    turnaround.onWrite=[&turnaround](){auto bytes=reply({1,3,2,0,7});for(size_t i=0;i<bytes.size();++i)turnaround.scheduled.push_back({testMicros+2579+573*i,{bytes[i]}});};
    assert(reader.readRegisters(reader.current,0x34,1,out));assert(out[1]==7&&reader.evidence.echoes==1&&reader.evidence.data==1&&reader.evidence.echoPrefixes==0);}
  {HardwareSerial retry(1);DiagnosticEngine reader(retry);uint8_t out[2];retry.ready=false;
    assert(!reader.readRegisters(reader.current,0x34,1,out)&&reader.activeBaud==0);retry.ready=true;retry.replies.push_back(reply({1,3,2,0,7}));
    assert(reader.readRegisters(reader.current,0x34,1,out)&&retry.beginCalls==2);}
  engine.consoleUsed=3;strcpy(engine.consoleLine,"reg");assert(engine.submit("x"));assert(engine.consoleUsed==0&&!engine.consoleCount);
  // Execute all field recipes through the generated engine, not a duplicate mock parser.
  const char *recipes[]={"01-passive-rx.txt","02-echo-or-no-reply.txt","03-serial-matrix.txt","04-register-exception.txt","05-raw-map-audit.txt","06-configured-kwh.txt","07-timeout-comparison.txt","08-word-order-comparison.txt"};
  std::string root=__FILE__;std::replace(root.begin(),root.end(),'\\','/');root=root.substr(0,root.rfind("firmware/test/"));
  for(const char *recipe:recipes){
    HardwareSerial serial(1);char bytes[8192];DiagnosticEngine diagnostic(serial,bytes,sizeof(bytes));
    std::ifstream stream(root+"examples/FortSerialDebug/cases/"+recipe);assert(stream);std::string line;
    while(std::getline(stream,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;assert(diagnostic.submit(line.c_str()));for(unsigned step=0;step<5000&&diagnostic.busy();++step)diagnostic.tick();assert(!diagnostic.busy());}
    char transcript[8193];diagnostic.copyLog(transcript,sizeof(transcript));assert(std::string(transcript).find("Unknown/invalid")==std::string::npos);
    for(const auto &request:serial.transmitted){assert(request.size()==8&&request[1]==3&&request[0]>=1&&request[0]<=247);assert(crc16(request.data(),6)==(uint16_t(request[6])|uint16_t(request[7])<<8));}
  }
  std::cout<<"PASS: generated production transport echo+later reply, exception safety, multi-address fallback, log ring truncation, real event order/cursor wrap, all eight standalone recipes in main engine\n";
}