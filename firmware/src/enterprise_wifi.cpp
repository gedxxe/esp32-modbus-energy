// SPDX-License-Identifier: GPL-3.0-only
#include "enterprise_wifi.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_eap_client.h>
#include <esp_idf_version.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/sha256.h>
#include <sys/time.h>
#ifndef CONFIG_MBEDTLS_HAVE_TIME_DATE
#error "Enterprise requires mbedTLS certificate date checks in the compiled SDK"
#endif
#if !defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE) || !defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL)
#error "Enterprise requires the pinned SDK full public CA bundle"
#endif
#ifdef CONFIG_ESP_WIFI_DISABLE_KEY_USAGE_CHECK
#error "Enterprise requires certificate key-usage checks"
#endif
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5,5,0)
#error "Enterprise validation requires the pinned ESP-IDF 5.5+ EAP APIs"
#endif
namespace fort {
void formatCAHash(const uint8_t hash[32],char text[65]){const char hex[]="0123456789abcdef";for(unsigned i=0;i<32;++i){text[i*2]=hex[hash[i]>>4];text[i*2+1]=hex[hash[i]&15];}text[64]=0;}
bool validateEnterpriseCA(const char *pem,uint8_t hash[32]){
  size_t size=strnlen(pem,NETWORK_CA_MAX+1);if(!size||size>NETWORK_CA_MAX||!strstr(pem,"-----BEGIN CERTIFICATE-----")||strstr(pem,"PRIVATE KEY"))return false;
  mbedtls_x509_crt cert;mbedtls_x509_crt_init(&cert);
  int result=mbedtls_x509_crt_parse(&cert,reinterpret_cast<const unsigned char*>(pem),size+1);
  bool valid=result==0;unsigned count=0;
  for(const mbedtls_x509_crt *p=&cert;p&&p->version;p=p->next){++count;if(!mbedtls_x509_crt_get_ca_istrue(p))valid=false;}
  mbedtls_x509_crt_free(&cert);
  return valid&&count>0&&count<=4&&mbedtls_sha256(reinterpret_cast<const unsigned char*>(pem),size,hash,0)==0;
}
static String caPath(const uint8_t hash[32]){char text[65];formatCAHash(hash,text);return String("/wifi-ca-")+text+".pem";}
bool loadNetworkCA(fs::FS &fs,const uint8_t hash[32],char *pem){
  File file=fs.open(caPath(hash),"r");if(!file)return false;size_t size=file.size();
  if(!size||size>NETWORK_CA_MAX||file.readBytes(pem,size)!=size){file.close();return false;}file.close();pem[size]=0;
  uint8_t actual[32];return strlen(pem)==size&&validateEnterpriseCA(pem,actual)&&!memcmp(hash,actual,32);
}
bool storeNetworkCA(fs::FS &fs,const char *pem,const uint8_t hash[32]){
  uint8_t actual[32];if(!validateEnterpriseCA(pem,actual)||memcmp(actual,hash,32))return false;
  String path=caPath(hash);
  if(fs.exists(path)){
    static char verify[NETWORK_CA_MAX+1];if(loadNetworkCA(fs,hash,verify))return true;
    String quarantine=path+".bad";if(fs.exists(quarantine)||!fs.rename(path,quarantine))return false;
  }
  File directory=fs.open("/");unsigned count=0;for(File item=directory.openNextFile();item;item=directory.openNextFile())if(String(item.name()).startsWith("wifi-ca-")&&String(item.name()).endsWith(".pem"))++count;
  if(count>=8)return false;
  String temporary=path+".tmp";File file=fs.open(temporary,"w");if(!file)return false;
  size_t size=strlen(pem);bool ok=file.write(reinterpret_cast<const uint8_t*>(pem),size)==size;file.flush();file.close();
  if(!ok)return false; // Preserve partial and valid records for investigation.
  return fs.rename(temporary,path);
}
bool setEnterpriseSystemClock(uint32_t utc){
  timeval value{};value.tv_sec=utc;return settimeofday(&value,nullptr)==0;
}
} // namespace fort
