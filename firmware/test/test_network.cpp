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
static unsigned stationConnectCalls=0,espConnectCalls=0,earlyAssocCalls=0;
static bool corruptReadback=false,corruptFinalReadback=false,stationConfigOK=true,dhcpPrepared=false;
static bool finalConfigWritten=false;
static int security=0,method=0;
static int api(const char *step){calls.emplace_back(step);return failAt==step?1234:ESP_OK;}
bool TestSTA::begin(bool tryConnect){
  calls.emplace_back("station_begin");
  if(tryConnect)++earlyAssocCalls;
  dhcpPrepared=false;
  return stationOK;
}
bool TestSTA::connect(const char *ssid,const char*,int,const uint8_t*,bool connect){
  ++stationConnectCalls;calls.emplace_back("station_config");autoConnect=connect;
  if(!stationOK||!stationConfigOK)return false;
  config=wifi_config_t{};
  memcpy(config.sta.ssid,ssid,strlen(ssid));
  config.sta.threshold.rssi=-127;
  config.sta.pmf_cfg.capable=true;
  finalConfigWritten=false;
  dhcpPrepared=true; // Pinned STA.connect(...,false) configures DHCPC.
  return true;
}
void TestWiFi::setMinSecurity(int value){security=value;}
int esp_wifi_get_config(int,wifi_config_t *out){
  *out=config;
  if(corruptReadback&&!finalConfigWritten)out->sta.bssid_set=true;
  if(corruptFinalReadback&&finalConfigWritten)out->sta.threshold.authmode=WIFI_AUTH_WPA_PSK;
  return api("get_config");
}
int esp_wifi_set_config(int,const wifi_config_t *value){config=*value;finalConfigWritten=true;return api("set_config");}
int esp_wifi_sta_enterprise_enable(){return api("enterprise_enable");}
int esp_wifi_sta_enterprise_disable(){return api("enterprise_disable");}
int esp_wifi_connect(){++espConnectCalls;return dhcpPrepared?api("connect"):4321;}
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
  assert(validNetwork(s));
  assert(!peapServerNameConfigured(s)); // Legacy PEAP is readable but lacks server-name binding.
  NetworkSettings pinned=s;strcpy(pinned.serverName,"radius.example.org");
  assert(validNetwork(pinned)&&peapServerNameConfigured(pinned));
  NetworkSettings personal=s;personal.auth=NETWORK_PERSONAL;strcpy(personal.password,"test-personal");
  assert(peapServerNameConfigured(personal));
  assert(canReuseEnterprise(s,s));
  NetworkSettings newSsid=s;strcpy(newSsid.ssid,"New plant WiFi");
  assert(!canReuseEnterprise(s,newSsid));
  NetworkSettings changedTrust=s;strcpy(changedTrust.serverName,"radius.example.org");
  assert(!canReuseEnterprise(s,changedTrust));
  EnterpriseWiFi driver;
  assert(driver.begin(s,nullptr,true,1793750400));
  assert(driver.attempted&&security==WIFI_AUTH_WPA2_ENTERPRISE);
  // Arduino 3.3.11 STA.begin() defaults to false; STA.connect(...,false)
  // configures SSID and DHCP without associating. The single post-EAP
  // esp_wifi_connect() must see a prepared DHCP client.
  assert(earlyAssocCalls==0&&stationConnectCalls==1&&!autoConnect&&dhcpPrepared&&espConnectCalls==1);
  assert(calls.front()=="station_begin");
  assert(calls[1]=="station_config"&&calls[2]=="get_config");
  assert(calls[3]=="set_config"&&calls[4]=="get_config");
  assert(config.sta.threshold.authmode==WIFI_AUTH_WPA2_ENTERPRISE&&config.sta.threshold.rssi==-127);
  assert(!config.sta.bssid_set&&!config.sta.channel);
  assert(!memcmp(config.sta.ssid,s.ssid,strlen(s.ssid)));
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
  stationOK=false;assert(!driver.begin(s,nullptr,true,1793750400));assert(!strcmp(driver.error,"station_config_failed")&&!strcmp(driver.apiStep,"station_prepare"));stationOK=true;
  stationConfigOK=false;
  assert(!driver.begin(s,nullptr,true,1793750400)&&!strcmp(driver.apiStep,"station_prepare"));
  stationConfigOK=true;
  corruptReadback=true;
  assert(!driver.begin(s,nullptr,true,1793750400)&&!strcmp(driver.apiStep,"station_verify_ssid"));
  corruptReadback=false;
  corruptFinalReadback=true;
  assert(!driver.begin(s,nullptr,true,1793750400)&&!strcmp(driver.apiStep,"station_verify"));
  corruptFinalReadback=false;
  // Each new Enterprise attempt must overwrite any stale personal SSID and
  // credentials, even if the previous station driver config was different.
  config=wifi_config_t{};
  config.sta.bssid_set=true;config.sta.channel=11;config.sta.threshold.rssi=-40;
  memset(config.sta.password,'X',sizeof(config.sta.password));calls.clear();
  assert(driver.begin(s,nullptr,true,1793750400));
  assert(earlyAssocCalls==0&&dhcpPrepared&&!autoConnect&&!memcmp(config.sta.ssid,s.ssid,strlen(s.ssid)));
  assert(config.sta.channel==0&&!config.sta.bssid_set&&config.sta.threshold.rssi==-127);
  for(unsigned i=0;i<sizeof(config.sta.password);++i)assert(config.sta.password[i]==0);
  assert(calls.back()=="connect"&&espConnectCalls>=2);
  assert(!driver.begin(s,nullptr,false,1793750400)&&!strcmp(driver.error,"enterprise_clock_required"));
  clockOK=false;assert(!driver.begin(s,nullptr,true,1793750400));clockOK=true;
  strcpy(s.identity,"anonymous@example.org");strcpy(s.serverName,"radius.example.org");memset(s.caHash,1,32);
  assert(!driver.begin(s,nullptr,true,1793750400)&&!strcmp(driver.error,"enterprise_ca_missing"));
  caOK=false;assert(!driver.begin(s,"test-CA",true,1793750400));caOK=true;
  EnterpriseWiFi badCAAPI;failAt="set_ca_cert";
  assert(!badCAAPI.begin(s,"test-CA",true,1793750400)&&!strcmp(badCAAPI.apiStep,"set_ca_cert")&&!badCAAPI.attempted);
  failAt.clear();assert(badCAAPI.reset());
  assert(driver.begin(s,"test-CA",true,1793750400)&&!bundle&&domain==s.serverName&&identity==s.identity);
  assert(stationConnectCalls>0&&dhcpPrepared&&!autoConnect);
  failAt="enterprise_disable";assert(!driver.begin(s,"test-CA",true,1793750400)&&!driver.attempted);failAt.clear();assert(driver.reset());
  clearEnterprise(s);s.auth=NETWORK_PERSONAL;strcpy(s.password,"personal-test");
  const unsigned personalConnectBase=stationConnectCalls;
  assert(driver.begin(s,nullptr,false,0)&&autoConnect&&security==WIFI_AUTH_WPA_PSK&&
         stationConnectCalls==personalConnectBase+1&&earlyAssocCalls==0);
  NetworkAttempt attempt;attempt.prepare(0xFFFFFF00u,true);assert(attempt.active());
  assert(!attempt.expired(0xFFFFFEFFu)[0]);
  assert(!attempt.expired(0xFFFFFF00u+4999u)[0]);assert(!strcmp(attempt.expired(0xFFFFFF00u+5000u),"station_disconnect_timeout"));
  attempt.connecting(0xFFFFFFF0u);assert(!attempt.accepts(0xFFFFFFEFu));assert(attempt.accepts(16));
  assert(!attempt.expired(0xFFFFFFF0u+59999u)[0]);assert(!strcmp(attempt.expired(0xFFFFFFF0u+60000u),"connection_timeout"));
  assert(!attempt.accepts(0xFFFFFFF0u+60000u));
  attempt.associated(100);assert(attempt.stage==NetworkAttempt::ADDRESSING);
  assert(!attempt.expired(15099)[0]&&attempt.accepts(15099));assert(!attempt.accepts(15100));assert(!strcmp(attempt.expired(15100),"dhcp_timeout"));
  attempt.fail(15100,"dhcp_timeout",0,0);assert(!attempt.retryAllowed(45099)&&attempt.retryAllowed(45100));
  auto previous=attempt.id;attempt.prepare(45100,true);assert(attempt.lastFailedId==previous&&!strcmp(attempt.lastError,"dhcp_timeout"));
  attempt.connecting(45350);attempt.online();assert(attempt.stage==NetworkAttempt::CONNECTING);
  attempt.associated(45360);attempt.online();assert(attempt.stage==NetworkAttempt::ONLINE&&attempt.lastFailedId==previous);
  attempt.idle();assert(!attempt.accepts(46000)&&!attempt.active());
  NetworkPolicy policy;policy.begin(NETWORK_CLIENT,0,60000);
  assert(!policy.update(60000,false,false)&&policy.apWanted(60000));assert(policy.update(90000,false,true));
  assert(wifiEventMatchesSSID("NewCompanySSID","NewCompanySSID"));
  assert(!wifiEventMatchesSSID("NewCompanySSID","OldCompanySSID"));
  assert(wifiEventMatchesSSID("NewCompanySSID","")); // unknown association still gets normal recovery
  assert(wifiEventMatchesSSID("NewCompanySSID",nullptr));
  // Deterministic network recovery tests: per-device jitter, bounded
  // exponential retry, suspended retry, healthy reset, and millis rollover.
  {
    NetworkPolicy first,second;
    first.begin(NETWORK_CLIENT,0,NETWORK_RETRY_MS,1);
    second.begin(NETWORK_CLIENT,0,NETWORK_RETRY_MS,2);
    assert(first.retryWaitMs()>=30000&&first.retryWaitMs()<=40000);
    assert(second.retryWaitMs()>=30000&&second.retryWaitMs()<=40000);
    assert(first.retryWaitMs()!=second.retryWaitMs()); // fleet desynchronization
    const uint32_t initial=first.retryWaitMs();
    assert(!first.update(initial-1,false,true));
    assert(first.retryRemainingMs(initial-1)==1);
    assert(!first.update(initial,false,false)); // a radio scan owns the station
    assert(first.retryRemainingMs(initial)==0);
    assert(first.update(initial,false,true));
    assert(first.retryStreak()==1&&first.retryWaitMs()>=60000&&first.retryWaitMs()<=70000);
    uint32_t clock=initial;
    for(unsigned i=0;i<8;++i){
      const uint32_t wait=first.retryWaitMs();
      assert(wait>=60000&&wait<=300000);
      assert(!first.update(clock+wait-1,false,true));
      clock+=wait;assert(first.update(clock,false,true));
      assert(first.retryWaitMs()<=300000);
    }
    assert(first.retryStreak()==4&&first.retryWaitMs()>=240000&&first.retryWaitMs()<=250000);
    // PEAP's 60-second base reaches the five-minute capped window, including
    // bounded jitter. Personal's 30-second base saturates below that cap.
    NetworkPolicy peap;peap.begin(NETWORK_CLIENT,0,60000,7);
    uint32_t peapNow=0;
    for(unsigned i=0;i<3;++i){
      peapNow+=peap.retryWaitMs();
      assert(peap.update(peapNow,false,true));
    }
    assert(peap.retryWaitMs()>=290000&&peap.retryWaitMs()<=300000);
    assert(!first.update(clock+100,true,true));
    assert(first.retryStreak()==0&&first.retryRemainingMs(clock+100)==0);
    assert(!first.update(clock+200,false,true));
    assert(first.retryStreak()==0); // a new outage starts at the base interval
    assert(first.retryRemainingMs(clock+200)>0);
    const uint32_t nearWrap=UINT32_MAX-200u;
    NetworkPolicy rollover;rollover.begin(NETWORK_DUAL,nearWrap,60000,3);
    const uint32_t due=rollover.retryWaitMs();
    assert(!rollover.update(nearWrap+due-1u,false,true));
    assert(rollover.update(nearWrap+due,false,true));
    assert(rollover.retryWaitMs()>=120000&&rollover.retryWaitMs()<=130000);
    assert(rollover.apWanted(nearWrap+due)); // AP fallback persists
  }
  // One-year software timeline: the station retry budget must keep recovering
  // across seven 49.7-day millis() wraps without relying on a reboot.
  // Reconnects remain bounded by the policy, not a signed absolute deadline.
  {
    constexpr uint64_t HOUR=3600000ULL, YEAR=365ULL*24ULL*HOUR;
    const uint32_t begin=0xFFFFFF00u;
    NetworkPolicy year;year.begin(NETWORK_CLIENT,begin,NETWORK_RETRY_MS);
    unsigned retries=0;
    for(uint64_t elapsed=HOUR;elapsed<=YEAR;elapsed+=HOUR){
      const uint32_t now=begin+uint32_t(elapsed);
      if(year.update(now,false,true))++retries;
      assert(year.apWanted(now)); // recovery hotspot remains available
    }
    assert(retries==365u*24u);
    NetworkAttempt attempt;
    for(uint64_t elapsed=HOUR;elapsed<=YEAR;elapsed+=31ULL*24ULL*HOUR){
      const uint32_t now=begin+uint32_t(elapsed);
      attempt.prepare(now,true);
      assert(attempt.active()&&!attempt.expired(now+4999u)[0]);
      attempt.connecting(now+500u);
      attempt.associated(now+800u);
      assert(attempt.accepts(now+2000u));
      attempt.online();assert(attempt.stage==NetworkAttempt::ONLINE);
    }
  }
  std::cout<<"PASS: production Enterprise adapter: correct pinned Arduino 3.3.11 non-associating STA setup with DHCP, one post-EAP association, public/custom CA, SDK faults, Personal fallback and rollover-safe deadlines\n";
}
