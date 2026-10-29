// SPDX-License-Identifier: GPL-3.0-only
#include "rgb_driver.h"
#include <soc/soc_caps.h>
namespace fort {
bool RgbDriver::begin(int pin){
  if(channel)return true;
  rmt_tx_channel_config_t config{};
  config.gpio_num=gpio_num_t(pin);config.clk_src=RMT_CLK_SRC_DEFAULT;
  config.resolution_hz=10000000;config.mem_block_symbols=SOC_RMT_MEM_WORDS_PER_CHANNEL;
  config.trans_queue_depth=1;
  if(rmt_new_tx_channel(&config,&channel)!=ESP_OK){channel=nullptr;return false;}
  rmt_copy_encoder_config_t copy{};
  if(rmt_new_copy_encoder(&copy,&encoder)!=ESP_OK){rmt_del_channel(channel);channel=nullptr;return false;}
  if(rmt_enable(channel)!=ESP_OK){rmt_del_encoder(encoder);rmt_del_channel(channel);encoder=nullptr;channel=nullptr;return false;}
  return true;
}
bool RgbDriver::write(const LedColor &color){
  // Poll completion without waiting. The persistent symbols cannot change during TX.
  if(!channel||rmt_tx_wait_all_done(channel,0)!=ESP_OK)return false;
  const uint8_t grb[]={color.green,color.red,color.blue};
  for(unsigned bit=0;bit<24;++bit){bool one=grb[bit/8]&(1<<(7-bit%8));
    symbols[bit].level0=1;symbols[bit].duration0=one?8:4;
    symbols[bit].level1=0;symbols[bit].duration1=one?4:8;
  }
  // Include an explicit 100 us low reset/latch after the 24 GRB bits.
  symbols[24].level0=0;symbols[24].duration0=500;
  symbols[24].level1=0;symbols[24].duration1=500;
  rmt_transmit_config_t transmit{};transmit.flags.queue_nonblocking=true;
  return rmt_transmit(channel,encoder,symbols,sizeof(symbols),&transmit)==ESP_OK;
}
}
