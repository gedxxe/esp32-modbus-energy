// SPDX-License-Identifier: GPL-3.0-only
// Compile the production station adapter with SDK boundary doubles, not a second adapter.
#include "enterprise_wifi.h"
#include "network_attempt.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_eap_client.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
using namespace fort;
TestWiFi WiFi;
static wifi_config_t config{};
static std::vector<std::string> calls;
static std::string failAt,identity,username,password,domain;
static bool stationOK=true,clockOK=true,caOK=true,bundle=false,timeDisabled=true,autoConnect=false;
static int security=0,method=0;
static int api(const char *step){calls.emplace_back(step);return failAt==step?1234:ESP_OK;}
bool TestSTA::begin(){return stationOK;}
bool TestSTA::connect(const char *ssid,const char*,int,const uint8_t*,bool connect){
  calls.emplace_back("station_config");autoConnect=connect;
  if(!stationOK)return false;
  memset(&config,0,sizeof(config));memcpy(config.sta.ssid,ssid,strlen(ssid));return true;
}
void TestWiFi::setMinSecurity(int value){security=value;}
int esp_wifi_get_config(int,wifi_config_t *out){*out=config;return api("get_config");}
int esp_wifi_set_config(int,const wifi_config_t *value){config=*value;return api("set_config");}
int esp_wifi_sta_enterprise_enable(){return api("enterprise_enable");}
int esp_wifi_sta_enterprise_disable(){return api("enterprise_disable");}
int esp_wifi_connect(){return api("connect");}
int esp_eap_client_set_eap_methods(int value){method=value;return api("set_eap_methods");}
int esp_eap_client_set_disable_time_check(bool value){timeDisabled=value;return api("set_disable_time_check");}
int esp_eap_client_set_identity(const uint8_t *p,int size){identity.assign(reinterpret_cast<const char*>(p),size);return api("set_identity");}
int esp_eap_client_set_username(const uint8_t *p,int size){username.assign(reinterpret_cast<const char*>(p),size);return api("set_username");}
int esp_eap_client_set_password(const uint8_t *p,int size){password.assign(reinterpret_cast<const char*>(p),size);return api("set_password");}
int esp_eap_client_set_domain_name(const char *p){domain=p?p:"";return api("set_domain_name");}
int esp_eap_client_use_default_cert_bundle(bool value){bundle=value;return api("use_default_cert_bundle");}
int esp_eap_client_set_ca_cert(const uint8_t*,int){return api("set_ca_cert");}
void esp_eap_client_clear_identity(){identity.clear();}
void esp_eap_client_clear_username(){username.clear();}
void esp_eap_client_clear_password(){password.clear();}
void esp_eap_client_clear_new_password(){}
void esp_eap_client_clear_ca_cert(){}
void esp_eap_client_clear_certificate_and_key(){}
namespace fort {
bool setEnterpriseSystemClock(uint32_t){return clockOK;}
bool validateEnterpriseCA(const char*,uint8_t hash[32]){memset(hash,1,32);return caOK;}
}
int main(){
  NetworkSettings s{};s.mode=NETWORK_DUAL;s.auth=NETWORK_PEAP;
  strcpy(s.ssid,"Arbitrary company SSID");strcpy(s.username,"device-user");strcpy(s.eapPassword,"test-secret");
  EnterpriseWiFi driver;
  assert(driver.begin(s,nullptr,true,1793750400));
  assert(driver.attempted&&security==WIFI_AUTH_WPA2_ENTERPRISE&&!autoConnect);
  assert(identity==s.username&&username==s.username&&password==s.eapPassword&&domain.empty());
  assert(method==ESP_EAP_TYPE_PEAP&&!timeDisabled&&bundle);
  assert(calls.back()=="connect"&&calls[calls.size()-2]=="enterprise_enable");
  driver.disconnected(23);assert(driver.authenticationPaused&&!strcmp(driver.error,"enterprise_authentication_rejected"));
  driver.online();assert(!driver.authenticationPaused&&!driver.error[0]);
  assert(driver.reset()&&identity.empty()&&username.empty()&&password.empty());
  for(const char *step:{"get_config","set_config","set_eap_methods","set_disable_time_check","set_identity","set_username","set_password","set_domain_name","use_default_cert_bundle","enterprise_enable","connect"}){
    EnterpriseWiFi trial;calls.clear();failAt=step;
    assert(!trial.begin(s,nullptr,true,1793750400));
    assert(trial.apiCode==1234&&!strcmp(trial.apiStep,step)&&!trial.attempted);
    failAt.clear();assert(trial.reset());
  }
  stationOK=false;assert(!driver.begin(s,nullptr,true,1793750400));assert(!strcmp(driver.error,"station_config_failed"));stationOK=true;
  assert(!driver.begin(s,nullptr,false,1793750400)&&!strcmp(driver.error,"enterprise_clock_required"));
  clockOK=false;assert(!driver.begin(s,nullptr,true,1793750400));clockOK=true;
  strcpy(s.identity,"anonymous@example.org");strcpy(s.serverName,"radius.example.org");memset(s.caHash,1,32);
  assert(!driver.begin(s,nullptr,true,1793750400)&&!strcmp(driver.error,"enterprise_ca_missing"));
  caOK=false;assert(!driver.begin(s,"test-CA",true,1793750400));caOK=true;
  EnterpriseWiFi badCAAPI;failAt="set_ca_cert";
  assert(!badCAAPI.begin(s,"test-CA",true,1793750400)&&!strcmp(badCAAPI.apiStep,"set_ca_cert")&&!badCAAPI.attempted);
  failAt.clear();assert(badCAAPI.reset());
  assert(driver.begin(s,"test-CA",true,1793750400)&&!bundle&&domain==s.serverName&&identity==s.identity);
  failAt="enterprise_disable";assert(!driver.begin(s,"test-CA",true,1793750400)&&!driver.attempted);failAt.clear();assert(driver.reset());
  clearEnterprise(s);s.auth=NETWORK_PERSONAL;strcpy(s.password,"personal-test");
  assert(driver.begin(s,nullptr,false,0)&&autoConnect&&security==WIFI_AUTH_WPA_PSK);
  NetworkAttempt attempt;attempt.prepare(0xFFFFFF00u,true);assert(attempt.active());
  assert(!attempt.expired(0xFFFFFF00u+1999u)[0]);assert(!strcmp(attempt.expired(0xFFFFFF00u+2000u),"station_disconnect_timeout"));
  attempt.connecting(0xFFFFFFF0u);assert(!attempt.accepts(0xFFFFFFEFu));assert(attempt.accepts(16));
  assert(!attempt.expired(0xFFFFFFF0u+59999u)[0]);assert(!strcmp(attempt.expired(0xFFFFFFF0u+60000u),"connection_timeout"));
  attempt.associated(100);assert(attempt.stage==NetworkAttempt::ADDRESSING);
  assert(!attempt.expired(15099)[0]);assert(!strcmp(attempt.expired(15100),"dhcp_timeout"));
  attempt.fail(15100,"dhcp_timeout",0,0);assert(!attempt.retryAllowed(45099)&&attempt.retryAllowed(45100));
  auto previous=attempt.id;attempt.prepare(45100,true);assert(attempt.lastFailedId==previous&&!strcmp(attempt.lastError,"dhcp_timeout"));
  attempt.connecting(45350);attempt.online();assert(attempt.stage==NetworkAttempt::CONNECTING);
  attempt.associated(45360);attempt.online();assert(attempt.stage==NetworkAttempt::ONLINE&&attempt.lastFailedId==previous);
  attempt.idle();assert(!attempt.accepts(46000)&&!attempt.active());
  NetworkPolicy policy;policy.begin(NETWORK_CLIENT,0,60000);
  assert(!policy.update(60000,false,false)&&policy.apWanted(60000));assert(policy.update(90000,false,true));
  std::cout<<"PASS: production Enterprise adapter, optional DNS/default trust, PEAP/time checks, API fault injection, credential reset, Personal fallback and rollover-safe connection deadlines/history\n";
}
