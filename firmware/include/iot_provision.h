// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Pure provisioning checks: never expose the bearer or endpoint in diagnostics.
#include "iot_contract.h"
#include <cstring>

namespace fort { namespace iot {
inline const char *validateProvision(bool enabled,const Binding &binding,
                                     const char *bearer,const char *endpoint,
                                     bool localHeaderPresent){
  if(!enabled)return "iot_disabled";
  if(!localHeaderPresent)return "iot_header_not_compiled";
  if(!valid(binding))return "iot_binding_invalid";
  if(!bearer||strlen(bearer)!=43)return "iot_bearer_length_invalid";
  for(const char *p=bearer;*p;++p){
    const char c=*p;
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
         (c>='0'&&c<='9')||c=='_'||c=='-'))
      return "iot_bearer_format_invalid";
  }
  if(!endpoint||strncmp(endpoint,"https://",8))return "iot_endpoint_https_required";
  if(strlen(endpoint)>=256)return "iot_endpoint_too_long";
  const char *host=endpoint+8;
  const char *path=strchr(host,'/');
  if(!path||path==host||strcmp(path,"/api/iot-ingest"))
    return "iot_endpoint_path_invalid";
  // Reject credentials, fragments, query strings and whitespace without
  // ever changing TLS hostname verification or reporting the URL.
  for(const char *p=host;*p;++p){
    const char c=*p;
    if(c=='@'||c=='#'||c=='?'||c<=0x20||c==0x7f)
      return "iot_endpoint_url_invalid";
  }
  return nullptr;
}
} } // namespace fort::iot
