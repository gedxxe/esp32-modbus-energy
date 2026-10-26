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
  evidence=Evidence{};connectedCapture=Capture{};isolatedCapture=Capture{};
  meter.onWrite=nullptr;meter.scheduled.clear();meter.ready=true;meter.writeLimit=256;testRxDrive=1;firstID=1;lastID=247;
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
  // Reports distinguish measured evidence from unknowable external conditions.
  resetDebug();command("diagnose");
  assert(evidence.requests==8&&evidence.timeouts==8&&evidence.energyBad==1);
  assert(Serial.output.find("NO_RX_UNRESOLVED")!=std::string::npos);
  assert(Serial.output.find("TRANSPORT_DATA_CONFIRMED")==std::string::npos);
  // Silence at the first configured address does not prevent another raw probe.
  resetDebug();command("decode off");command("plan clear");command("plan add 0080 1");
  meter.replies.push_back({});meter.replies.push_back(frame({1,3,2,0x13,0x88}));command("diagnose");
  assert(meter.transmitted.size()==2&&meter.transmitted[1][3]==0x80&&evidence.data==1);
  assert(Serial.output.find("TRANSPORT_DATA_CONFIRMED")!=std::string::npos);
  resetDebug();command("decode off");command("plan clear");meter.replies.push_back(frame({1,0x83,2}));command("diagnose");
  assert(evidence.exceptions==1&&Serial.output.find("TRANSPORT_EXCEPTION_ONLY")!=std::string::npos);
  assert(Serial.output.find("PRIMARY_IMPORT")==std::string::npos);
  // An independently delimited echo is rejected, but a later good frame succeeds.
  resetDebug();meter.replies.push_back(frame({1,3,0,0x34,0,1}));
  meter.scheduled.push_back({testMicros+100000,frame({1,3,2,0,1})});assert(probeLink(current));
  assert(evidence.echoes==1&&evidence.data==1);
  resetDebug();meter.ready=false;assert(!probeLink(current));reportEvidence();
  assert(meter.transmitted.empty()&&Serial.output.find("LOCAL_UART_ERROR")!=std::string::npos);
  resetDebug();meter.writeLimit=2;assert(!probeLink(current));assert(evidence.txErrors==1);
  resetDebug();testRxDrive=-1;command("rxcheck");assert(meter.transmitted.empty());
  assert(Serial.output.find("RX_WEAK_BIAS_FOLLOWING")!=std::string::npos&&activeBaud==current.baud&&testPinMode==INPUT);
  resetDebug();testRxDrive=1;command("rxcheck");assert(Serial.output.find("RX_HIGH_HELD")!=std::string::npos);
  resetDebug();testRxDrive=0;command("rxcheck");assert(Serial.output.find("RX_LOW_HELD")!=std::string::npos);
  resetDebug();testRxDrive=2;command("rxcheck");assert(Serial.output.find("RX_VARIABLE")!=std::string::npos);
  resetDebug();Serial.input={'x','\n'};command("rxcheck");assert(Serial.output.find("RX_CHECK_ABORTED")!=std::string::npos&&testPinMode==INPUT);
  resetDebug();meter.incoming={0x12,0x34};assert(!probeLink(current));reportEvidence();
  assert(evidence.staleBytes==2&&evidence.rxBytes==0&&Serial.output.find("RX_UNVALIDATED")!=std::string::npos);
  resetDebug();command("scan");Serial.input={'x','\n'};pumpConsole();assert(evidence.aborted&&!scanning);
  // Paired captures reject changed settings and aborted/contradictory states.
  resetDebug();command("decode off");command("plan clear");command("check connected");command("check isolated");
  assert(Serial.output.find("BOTH_SILENT_UNRESOLVED")!=std::string::npos);
  command("timeout 1000");command("check isolated");assert(Serial.output.find("COMPARE_CONFIG_CHANGED")!=std::string::npos);
  resetDebug();command("decode off");command("plan clear");meter.replies.push_back(frame({1,3,2,0,1}));
  command("check connected");command("check isolated");assert(Serial.output.find("REPLY_REQUIRES_CONNECTED_PATH")!=std::string::npos);
  meter.replies.push_back(frame({1,0x83,2}));command("check isolated");assert(Serial.output.find("ISOLATION_CONTRADICTED")!=std::string::npos);
  resetDebug();command("decode off");command("plan clear");
  meter.replies.push_back(frame({1,3,0,0x34,0,1}));command("check connected");
  meter.replies.push_back(frame({1,3,0,0x34,0,1}));command("check isolated");assert(Serial.output.find("LOCAL_ACTIVITY_REPRODUCED")!=std::string::npos);
  connectedCapture.result.txErrors=1;command("compare");assert(Serial.output.find("COMPARE_LOCAL_UART_ERROR")!=std::string::npos);connectedCapture.result.txErrors=0;
  connectedCapture.result.aborted=true;command("compare");assert(Serial.output.find("COMPARE_INCOMPLETE")!=std::string::npos);
  resetDebug();command("decode off");command("plan clear");
  meter.replies.push_back(frame({2,3,2,0,1}));command("check connected");command("check isolated");
  assert(Serial.output.find("CONNECTED_ACTIVITY_ONLY")!=std::string::npos);
  // Old replies arriving during the pre-TX guard must never satisfy the request.
  resetDebug();meter.scheduled.push_back({testMicros+3000,frame({1,3,2,0x12,0x34})});
  uint8_t delayedData[2]={};assert(!readRegisters(current,0x34,1,delayedData));
  assert(evidence.staleBytes==7&&evidence.data==0&&evidence.timeouts==1);
  // Continuous pre-TX traffic has a bounded no-transmit outcome, not a fake reply.
  resetDebug();options.responseMs=50;uint64_t noisyStart=testMicros;
  for(unsigned i=0;i<100;++i)meter.scheduled.push_back({testMicros+uint64_t(i)*1000,{0x55}});
  assert(!readRegisters(current,0x34,1,delayedData));
  assert(meter.transmitted.empty()&&evidence.busBusy==1&&evidence.requests==0&&testMicros-noisyStart<=51000);
  // Legal minimum turnaround: exact echo, idle t3.5, then byte-wise 8E1 reply.
  resetDebug();current.baud=19200;current.parity='E';
  meter.replies.push_back(frame({1,3,0,0x34,0,1}));
  meter.onWrite=[](){auto bytes=frame({1,3,2,0,7});for(size_t i=0;i<bytes.size();++i)meter.scheduled.push_back({testMicros+2579+573*i,{bytes[i]}});};
  assert(readRegisters(current,0x34,1,delayedData));assert(delayedData[1]==7&&evidence.echoes==1&&evidence.data==1&&evidence.echoPrefixes==0);
  // Failed UART begin is retried even for the same saved serial format.
  resetDebug();activeBaud=0;meter.ready=false;unsigned initialBegins=meter.beginCalls;
  assert(!readRegisters(current,0x34,1,delayedData)&&activeBaud==0);
  meter.ready=true;meter.replies.push_back(frame({1,3,2,0,7}));assert(readRegisters(current,0x34,1,delayedData));assert(meter.beginCalls==initialBegins+2);
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
  std::cout<<"PASS: real diagnostic INO: passive no-TX, valid exception/data contact, wrong-ID/CRC/echo rejection, scan stops on transport, no false kWh, valid kWh, FC03-only requests, runtime pins/timeout/profile/probe/scan sets/plans, invalid-input bounds, queued commands/comments/abort, all 8 pasted case recipes, UART events, adaptive multi-address diagnosis, evidence reports, weak-bias observations, paired isolation/config/abort guards\n";
}
