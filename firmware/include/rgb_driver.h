// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "led.h"
#include <driver/rmt_tx.h>
namespace fort {
// Initialized first on core 0, so this station's only RMT TX allocation owns channel 0.
class RgbDriver {
 public:
  bool begin(int pin);
  bool write(const LedColor &color);
 private:
  rmt_channel_handle_t channel=nullptr;
  rmt_encoder_handle_t encoder=nullptr;
  rmt_symbol_word_t symbols[25]={};
};
}
