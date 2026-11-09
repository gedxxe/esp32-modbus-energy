// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <WebServer.h>
#include <cstdint>

namespace fort {
// Keeps the existing synchronous WebServer transport behavior isolated from
// the application. An idle-pump callback is injected by the UI task: this
// module does not own RTOS, LoRa, Modbus or credentials.
class BoundedWebServer final : public WebServer {
 public:
  BoundedWebServer(uint16_t port,void (*idlePump)());
  void handleClient() override;
 protected:
  size_t _currentClientWrite(const char *data,size_t size) override;
  size_t _currentClientWrite_P(PGM_P data,size_t size) override;
 private:
  uint32_t requestStarted_=0;
  void (*idlePump_)()=nullptr;
};
} // namespace fort
