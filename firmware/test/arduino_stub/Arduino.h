#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
constexpr uint32_t SERIAL_8N1=0, SERIAL_8E1=1, SERIAL_8O1=2;
enum hardwareSerial_error_t { UART_NO_ERROR,UART_BREAK_ERROR,UART_BUFFER_FULL_ERROR,UART_FIFO_OVF_ERROR,UART_FRAME_ERROR,UART_PARITY_ERROR };
static uint64_t testMicros=0;
inline uint32_t millis(){return uint32_t(testMicros/1000);}
inline uint32_t micros(){return uint32_t(testMicros);}
inline void delay(uint32_t ms){testMicros+=uint64_t(ms)*1000;}
constexpr int INPUT=0, INPUT_PULLUP=1, INPUT_PULLDOWN=2;
static int testPinMode=INPUT,testRxDrive=1; // -1: follows weak pulls; 2: toggles
inline void pinMode(int,int mode){testPinMode=mode;}
inline int digitalRead(int){return testRxDrive==-1?(testPinMode==INPUT_PULLUP):testRxDrive==2?int(millis()%2):testRxDrive;}
struct TestConsole {
  std::deque<char> input; std::string output;
  explicit operator bool() const{return true;}
  void begin(uint32_t){}
  int available()const{return int(input.size());}
  int read(){char c=input.front();input.pop_front();return c;}
  void println(){output+='\n';}
  void printf(const char *format,...) {char buffer[1024];va_list args;va_start(args,format);vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);output+=buffer;}
};
static TestConsole Serial;
struct HardwareSerial {
  std::deque<uint8_t> incoming;
  std::deque<std::vector<uint8_t>> replies;
  std::vector<std::vector<uint8_t>> transmitted;
  void (*errorCallback)(hardwareSerial_error_t)=nullptr;
  struct TimedFrame {uint64_t at;std::vector<uint8_t> bytes;};
  std::deque<TimedFrame> scheduled;
  bool ready=true;size_t writeLimit=256;
  explicit operator bool()const{return ready;}
  explicit HardwareSerial(int){}
  void end(){incoming.clear();}
  void setRxBufferSize(size_t){}
  void begin(uint32_t,uint32_t,int,int){}
  void onReceiveError(void(*cb)(hardwareSerial_error_t)){errorCallback=cb;}
  int available(){
    if(!scheduled.empty()&&testMicros>=scheduled.front().at){for(auto byte:scheduled.front().bytes)incoming.push_back(byte);scheduled.pop_front();}
    return int(incoming.size());
  }
  int read(){int value=incoming.front();incoming.pop_front();return value;}
  size_t write(const uint8_t *data,size_t size){
    transmitted.emplace_back(data,data+size);
    if(!replies.empty()) {for(auto byte:replies.front())incoming.push_back(byte);replies.pop_front();}
    return std::min(size,writeLimit);
  }
  void flush(){}
};
