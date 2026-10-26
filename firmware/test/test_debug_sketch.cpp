// Execute the real standalone INO against deterministic console/UART mocks.
#include <assert.h>
#include <iostream>
#include "../../examples/FortSerialDebug/FortSerialDebug.ino"
std::vector<uint8_t> frame(std::initializer_list<uint8_t> data) {
  std::vector<uint8_t> result(data);uint16_t crc=modbusCRC(result.data(),result.size());
  result.push_back(uint8_t(crc));result.push_back(uint8_t(crc>>8));return result;
}
void resetDebug() {
  scanning=false;polling=false;abortRequested=false;lastReply=REPLY_NONE;
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
  uartError(UART_PARITY_ERROR);uartError(UART_FRAME_ERROR);assert(parityErrors.load()==1&&frameErrors.load()==1);
  std::cout<<"PASS: real diagnostic INO: passive no-TX, valid exception/data contact, wrong-ID/CRC/echo rejection, scan stops on transport, no false kWh, valid kWh, FC03-only requests, bounds/abort/UART events\n";
}
