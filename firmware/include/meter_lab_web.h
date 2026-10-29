// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <WebServer.h>
#include "meter_lab.h"
namespace fort {
struct MeterLabHooks {
  bool (*authorize)()=nullptr;
  const MeterLabSnapshot &(*read)()=nullptr;
  void (*submit)(const MeterLabRequest &)=nullptr;
  void (*stop)(uint32_t)=nullptr;
};
void registerMeterLabWeb(WebServer &server,const MeterLabHooks &hooks);
}
