#pragma once
#include <stdint.h>
constexpr int ESP_OK=0,WIFI_IF_STA=0,WIFI_AUTH_OPEN=0,WIFI_AUTH_WPA_PSK=1,WIFI_AUTH_WPA2_ENTERPRISE=5;
struct wifi_config_t {struct {uint8_t ssid[32],password[64];struct {int authmode;} threshold;struct {bool capable;} pmf_cfg;} sta;};
int esp_wifi_get_config(int,wifi_config_t*);
int esp_wifi_set_config(int,const wifi_config_t*);
int esp_wifi_sta_enterprise_enable();
int esp_wifi_sta_enterprise_disable();
int esp_wifi_connect();
