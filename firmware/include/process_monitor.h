// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <esp_freertos_hooks.h>
static_assert(sizeof(uint32_t)==4,"Sampler requires aligned 32-bit counters");
namespace fort {
// Tick-boundary sampling, not per-instruction accounting. ISR work is bounded.
// One ISR writer per core; aligned 32-bit DRAM reads are atomic on this target.
static DRAM_ATTR volatile uint32_t coreTicks[2]={},coreBusyTicks[2]={};
static DRAM_ATTR TaskHandle_t idleHandles[2]={};
static bool coreSamplerOK[2]={};
static TaskHandle_t acquisitionTask=nullptr,uiTask=nullptr;
static uint32_t uiCycleMs=0;
static void IRAM_ATTR sampleCore0(){coreTicks[0]=coreTicks[0]+1;if(xTaskGetCurrentTaskHandleForCore(0)!=idleHandles[0])coreBusyTicks[0]=coreBusyTicks[0]+1;}
static void IRAM_ATTR sampleCore1(){coreTicks[1]=coreTicks[1]+1;if(xTaskGetCurrentTaskHandleForCore(1)!=idleHandles[1])coreBusyTicks[1]=coreBusyTicks[1]+1;}
inline void initProcessMonitor(){
  uiTask=xTaskGetCurrentTaskHandle();idleHandles[0]=xTaskGetIdleTaskHandleForCore(0);idleHandles[1]=xTaskGetIdleTaskHandleForCore(1);
  coreSamplerOK[0]=esp_register_freertos_tick_hook_for_cpu(sampleCore0,0)==ESP_OK;
  coreSamplerOK[1]=esp_register_freertos_tick_hook_for_cpu(sampleCore1,1)==ESP_OK;
}
struct CoreLoadSample {
  uint32_t previousTicks[2]{},previousBusy[2]{},sampledAt=0;
  double percent[2]={};bool ready[2]={};bool initialized=false;
  void update(uint32_t now){
    if(!initialized){for(unsigned core=0;core<2;++core){previousTicks[core]=coreTicks[core];previousBusy[core]=coreBusyTicks[core];}sampledAt=now;initialized=true;return;}
    if(uint32_t(now-sampledAt)<1000)return;
    for(unsigned core=0;core<2;++core){uint32_t ticks=coreTicks[core],busy=coreBusyTicks[core];
      uint32_t dt=ticks-previousTicks[core],db=busy-previousBusy[core];
      ready[core]=coreSamplerOK[core]&&dt>0;
      percent[core]=dt?100.0*(db>dt?dt:db)/dt:0;
      previousTicks[core]=ticks;previousBusy[core]=busy;
    }sampledAt=now;
  }
};
} // namespace fort