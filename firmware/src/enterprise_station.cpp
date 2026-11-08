// SPDX-License-Identifier: GPL-3.0-only
#include "enterprise_wifi.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_eap_client.h>
namespace fort {
bool EnterpriseWiFi::checked(int32_t result,const char *step){
  if(result==ESP_OK)return true;
  apiCode=result;apiStep=step;error="enterprise_api_failed";return false;
}
bool EnterpriseWiFi::reset(){
  attempted=false;authenticationPaused=false;
  if(configured){
    if(enabled&&!checked(esp_wifi_sta_enterprise_disable(),"enterprise_disable"))return false;
    enabled=false;
    esp_eap_client_clear_identity();esp_eap_client_clear_username();esp_eap_client_clear_password();
    esp_eap_client_clear_new_password();esp_eap_client_clear_ca_cert();esp_eap_client_clear_certificate_and_key();
    if(!checked(esp_eap_client_set_domain_name(nullptr),"clear_domain_name"))return false;
  }
  error="";apiCode=0;apiStep="";disconnectReason=0;configured=false;return true;
}
bool EnterpriseWiFi::begin(const NetworkSettings &s,const char *ca,bool clockValid,uint32_t utc){
  ++attempts;
  if(!reset())return false;
  auth=s.auth;
  if(s.auth!=NETWORK_PEAP){
    WiFi.setMinSecurity(s.auth==NETWORK_OPEN?WIFI_AUTH_OPEN:WIFI_AUTH_WPA_PSK);
    // Disable implicit association with cached SSID in every mode. An
    // explicit Personal connect follows with the requested current SSID.
    if(!WiFi.STA.begin(false)||!WiFi.STA.connect(s.ssid,s.password[0]?s.password:nullptr)){
      error="station_config_failed";apiStep="station_connect";return false;
    }
    attempted=true;return true;
  }
  if(!clockValid||utc<1704067200UL||utc>4102444799UL){error="enterprise_clock_required";return false;}
  const bool customCA=hasCustomNetworkCA(s);
  uint8_t hash[32];
  if(customCA&&(!ca||!validateEnterpriseCA(ca,hash)||memcmp(hash,s.caHash,32))){error="enterprise_ca_missing";return false;}
  if(!setEnterpriseSystemClock(utc)){error="enterprise_clock_required";return false;}
  WiFi.setMinSecurity(WIFI_AUTH_WPA2_ENTERPRISE);
  // Pinned Arduino-ESP32 3.3.11: STA.begin() already defaults to false.
  // The prepare-only STA.connect(..., false) is essential: in addition to
  // configuring SSID it starts DHCP on a dynamic-IP station, while NEVER
  // calling esp_wifi_connect(). Skipping it can leave Enterprise without IP.
  if(!WiFi.STA.begin(false)||!WiFi.STA.connect(s.ssid,nullptr,0,nullptr,false)){
    error="station_config_failed";apiStep="station_prepare";return false;
  }
  wifi_config_t config{};
  if(!checked(esp_wifi_get_config(WIFI_IF_STA,&config),"get_config"))return false;
  const size_t ssidLength=strnlen(s.ssid,sizeof(s.ssid));
  if(!ssidLength||ssidLength>sizeof(config.sta.ssid)||
     memcmp(config.sta.ssid,s.ssid,ssidLength)||
     config.sta.bssid_set||config.sta.channel){
    error="station_config_failed";apiStep="station_verify_ssid";return false;
  }
  // STA.connect(..., false) created a fresh, unpinned configuration. Keep its
  // scan policy and initialized DHCP, but explicitly enforce Enterprise auth.
  memset(config.sta.password,0,sizeof(config.sta.password));
  config.sta.threshold.authmode=WIFI_AUTH_WPA2_ENTERPRISE;
  config.sta.threshold.rssi=-127;
  config.sta.pmf_cfg.capable=true;
  if(!checked(esp_wifi_set_config(WIFI_IF_STA,&config),"set_config"))return false;
  wifi_config_t verified{};
  if(!checked(esp_wifi_get_config(WIFI_IF_STA,&verified),"verify_config"))return false;
  if(memcmp(verified.sta.ssid,config.sta.ssid,sizeof(config.sta.ssid))||
     memcmp(verified.sta.password,config.sta.password,sizeof(config.sta.password))||
     verified.sta.threshold.authmode!=WIFI_AUTH_WPA2_ENTERPRISE||
     verified.sta.bssid_set||verified.sta.channel){
    error="station_config_failed";apiStep="station_verify";return false;
  }
  configured=true;
  if(!checked(esp_eap_client_set_eap_methods(ESP_EAP_TYPE_PEAP),"set_eap_methods")||
     !checked(esp_eap_client_set_disable_time_check(false),"set_disable_time_check")||
     !checked(esp_eap_client_set_identity(reinterpret_cast<const uint8_t*>(s.identity[0]?s.identity:s.username),strlen(s.identity[0]?s.identity:s.username)),"set_identity")||
     !checked(esp_eap_client_set_username(reinterpret_cast<const uint8_t*>(s.username),strlen(s.username)),"set_username")||
     !checked(esp_eap_client_set_password(reinterpret_cast<const uint8_t*>(s.eapPassword),strlen(s.eapPassword)),"set_password")||
     !checked(esp_eap_client_set_domain_name(s.serverName[0]?s.serverName:nullptr),"set_domain_name"))return false;
  if(!checked(esp_eap_client_use_default_cert_bundle(!customCA),"use_default_cert_bundle"))return false;
  if(customCA&&!checked(esp_eap_client_set_ca_cert(reinterpret_cast<const uint8_t*>(ca),strlen(ca)+1),"set_ca_cert"))return false;
  if(!checked(esp_wifi_sta_enterprise_enable(),"enterprise_enable"))return false;
  enabled=true;
  if(!checked(esp_wifi_connect(),"connect")){
    if(esp_wifi_sta_enterprise_disable()==ESP_OK)enabled=false;
    return false;
  }
  attempted=true;return true;
}
} // namespace fort
