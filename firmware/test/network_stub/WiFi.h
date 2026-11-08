#pragma once
#include <stddef.h>
#include <stdint.h>
struct TestSTA {bool begin(bool tryConnect=false);bool connect(const char*,const char*,int=0,const uint8_t* =nullptr,bool=true);};
struct TestWiFi {TestSTA STA;void setMinSecurity(int);};
extern TestWiFi WiFi;
