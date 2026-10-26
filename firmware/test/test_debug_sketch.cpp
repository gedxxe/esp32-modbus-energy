// Execute the real standalone INO against deterministic console/UART mocks.
#include <assert.h>
#include <iostream>
#include <fstream>
#include <iterator>
#include "../../examples/FortSerialDebug/FortSerialDebug.ino"
std::vector<uint8_t> frame(std::initializer_list<uint8_t> data) {
  std::vector<uint8_t> result(data);uint16_t crc=modbusCRC(result.data(),result.size());
  result.push_back(uint8_t(crc));result.push_back(uint8_t(crc>>8));return result;
}
void resetDebug() {
  scanning=false;polling=false;abortRequested=false;lastReply=REPLY_NONE;
  options=DebugOptions{};baudCount=6;parityCount=3;uint32_t rates[]={9600,19200,38400,4800,2400,1200};memcpy(BAUDS,rates,sizeof(rates));memcpy(PARITIES,"NEO",3);consoleHead=consoleTail=consoleCount=consoleUsed=0;consoleDiscard=false;
  auditCount=6;Query defaults[]={{0x34,1},{0x35,1},{0x2c,3},{0x80,1},{0x82,2},{0x8a,2}};memcpy(auditPlan,defaults,sizeof(defaults));
  current=Link{};openLink(current);meter.incoming.clear();meter.replies.clear();meter.transmitted.clear();Serial.input.clear();Serial.output.clear();
}
int main() {
  resetDebug();meter.incoming={0x12,0x34};listenOnly(1);
  assert(meter.transmitted.empty());assert(Serial.output.find("bytes=2")!=std::string::npos);
  resetDebug();meter.replies.push_back(frame({1,0x83,2}));
  assert(probeLink(current));assert(lastReply==REPLY_EXCEPTION);
  assert(Serial.output.find("COMM_VALID_EXCEPTION")!=std::string::npos);
  resetDebug();meter.replies.push_back(frame({1,3,2,0,1}));assert(probeLink(current));assert(lastReply==REPLY_DATA);
  resetDebug();meter.replies.push_back(frame({2,3,2,0,1}));assert(!probeLink(current));assert(lastReply==REPLY_NONE);
  resetDebug();auto corrupted=frame({1,3,2,0,1});corrupted[3]^=1;meter.replies.push_back(corrupted);
  assert(!probeLink(current));assert(Serial.output.find("MISMATCH")!=std::string::npos);
  resetDebug();meter.replies.push_back(frame({1,3,0,0x34,0,1}));assert(!probeLink(current));
  assert(Serial.output.find("EXACT_TX_ECHO")!=std::string::npos);
  resetDebug();command("range 1 1");command("scan");meter.replies.push_back(frame({1,0x83,2}));loop();
  assert(!scanning&&meter.transmitted.size()==1&&current.id==1);
  assert(meter.transmitted[0][2]==0&&meter.transmitted[0][3]==0x34);
  resetDebug();meter.replies.push_back(frame({1,0x83,2}));assert(!readEnergy(current));
  assert(Serial.output.find("PRIMARY_IMPORT")==std::string::npos);
  resetDebug();meter.replies.push_back(frame({1,3,4,0,0,3,0xE8}));meter.replies.push_back(frame({1,3,6,0,1,0,1,0,1}));
  assert(readEnergy(current));assert(Serial.output.find("1.000000 kWh")!=std::string::npos);
  for(const auto &request:meter.transmitted){assert(request.size()==8&&request[1]==3&&request[0]>=1);assert(modbusCRC(request.data(),6)==(uint16_t(request[6])|(uint16_t(request[7])<<8)));}
  resetDebug();command("reg FFFF 3");assert(meter.transmitted.empty());
  resetDebug();Serial.input={'x','\n'};uint8_t data[2];assert(!readRegisters(current,0x34,1,data));assert(abortRequested);
  resetDebug();command("probe 0080 1");command("timeout 1200");command("interval 4000");command("pins 17 18");
  assert(options.probeAddress==0x80&&options.responseMs==1200&&options.pollMs==4000&&options.rx==17&&activeRx==17);
  command("pins 18 18");command("timeout 0");assert(options.rx==17&&options.responseMs==1200);
  meter.replies.push_back(frame({1,3,2,0x13,0x88}));assert(probeLink(current));assert(meter.transmitted.back()[3]==0x80);
  resetDebug();command("range 3 4");command("bauds 19200 9600");command("parities EO");assert(scanTotal()==8);
  command("scanmode current");assert(scanTotal()==2);command("bauds 9600 9600");assert(baudCount==2&&BAUDS[0]==19200);
  command("plan clear");for(int i=0;i<10;++i)command("plan add 0080 1");assert(auditCount==8);
  command("plan add FFFF 2");assert(auditCount==8);
  resetDebug();command("energy 0092");command("ratios 002D");command("scale 0.01");command("order swap");command("decode off");
  assert(options.energyAddress==0x92&&options.ratioAddress==0x2d&&options.energyScale==0.01&&options.swappedWords&&!options.decodeEnergy);
  command("scale nan");command("scale -1");assert(options.energyScale==0.01);assert(!readEnergy(current)&&meter.transmitted.empty());
  resetDebug();Serial.input={'s','t','a','t','u','s','\n'};uint8_t queuedData[2];readRegisters(current,0x34,1,queuedData);
  assert(consoleCount==1);serialInput();assert(consoleCount==0&&Serial.output.find("SETTINGS")!=std::string::npos);
  resetDebug();Serial.input={'#','a','\n','s','t','a','t','u','s','\n','x','\n'};pumpConsole();assert(consoleCount==0&&abortRequested);
  resetDebug();Serial.input={'r','e','g',' ','0','x','0','0','8','0',' ','1','\n'};pumpConsole();assert(consoleCount==1&&!abortRequested);
  resetDebug();command("energy 0092");command("ratios 002D");command("scale 0.01");command("order swap");
  meter.replies.push_back(frame({1,3,4,0,0,3,0xE8}));meter.replies.push_back(frame({1,3,6,0,1,0,1,0,1}));assert(readEnergy(current));
  assert(meter.transmitted[0][3]==0x92&&meter.transmitted[1][3]==0x2d&&Serial.output.find("655360.000000 kWh")!=std::string::npos);
  resetDebug();for(int i=0;i<17;++i)for(char ch:std::string("status\n"))Serial.input.push_back(ch);while(Serial.available())pumpConsole();
  assert(consoleCount==16&&Serial.output.find("CONSOLE_FULL")!=std::string::npos);Serial.input={'x'};pumpConsole();assert(consoleCount==0&&abortRequested);
  command("stats clear");uartError(UART_PARITY_ERROR);uartError(UART_FRAME_ERROR);assert(parityErrors.load()==1&&frameErrors.load()==1);
  const char *recipes[]={"01-passive-rx.txt","02-echo-or-no-reply.txt","03-serial-matrix.txt","04-register-exception.txt","05-raw-map-audit.txt","06-configured-kwh.txt","07-timeout-comparison.txt","08-word-order-comparison.txt"};
  std::string sourcePath=__FILE__;std::replace(sourcePath.begin(),sourcePath.end(),'\\','/');
  std::string sourceRoot=sourcePath.substr(0,sourcePath.rfind("firmware/test/"));
  for(const char *recipe:recipes){
    resetDebug();std::ifstream stream(sourceRoot+"examples/FortSerialDebug/cases/"+recipe);assert(stream);
    std::string content((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
    for(char ch:content)Serial.input.push_back(ch);
    for(unsigned turns=0;turns<200&&(Serial.available()||consoleCount||scanning);++turns)loop();
    assert(!Serial.available()&&!consoleCount&&!scanning&&!polling);
    assert(Serial.output.find("Unknown/invalid")==std::string::npos&&Serial.output.find("CONSOLE_FULL")==std::string::npos);
    for(const auto &request:meter.transmitted){assert(request.size()==8&&request[1]==3&&request[0]>=1&&request[0]<=247);assert(modbusCRC(request.data(),6)==(uint16_t(request[6])|(uint16_t(request[7])<<8)));}
  }
  std::cout<<"PASS: real diagnostic INO: passive no-TX, valid exception/data contact, wrong-ID/CRC/echo rejection, scan stops on transport, no false kWh, valid kWh, FC03-only requests, runtime pins/timeout/profile/probe/scan sets/plans, invalid-input bounds, queued commands/comments/abort, all 8 pasted case recipes, UART events\n";
}
