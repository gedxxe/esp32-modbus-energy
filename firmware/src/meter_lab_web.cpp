// SPDX-License-Identifier: GPL-3.0-only
#include "meter_lab_web.h"
#include <ArduinoJson.h>
#include "version.h"
namespace fort {
namespace {
bool number(const String &s,uint32_t &value){if(!s.length()||s.length()>10)return false;uint64_t n=0;for(unsigned i=0;i<s.length();++i){if(s[i]<'0'||s[i]>'9')return false;n=n*10+s[i]-'0';if(n>UINT32_MAX)return false;}value=uint32_t(n);return true;}
String hex(const uint8_t *p,size_t n){const char digits[]="0123456789ABCDEF";String s;s.reserve(n*3);for(size_t i=0;i<n;++i){if(i)s+=' ';s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;}
void deviceJSON(JsonObject out,const DiscoveryDevice &d){
  out["slave"]=d.link.slave;out["baud"]=d.link.baud;out["parity"]=String(d.link.parity);out["stopBits"]=d.link.stops;out["direction"]=d.direction?"auto":"manual";
  out["boot"]=d.boot;out["run"]=d.run;out["utc"]=d.utc;out["elapsedMs"]=d.elapsedMs;out["requests"]=d.requests;out["timeouts"]=d.timeouts;out["crcErrors"]=d.crcErrors;out["rejected"]=d.rejected;out["exceptions"]=d.exceptions;out["mapEvidence"]=d.mapEvidence();JsonArray hints=out["mappingHints"].to<JsonArray>();for(uint8_t id=0;id<profiles::COUNT&&id<8;++id)if(d.mapEvidence()&(1<<id))hints.add(profiles::get(id).key);out["confirmed"]=d.confirmed();
  JsonArray probes=out["probes"].to<JsonArray>();
  for(unsigned i=0;i<d.count;++i){const auto &p=d.probes[i];JsonObject row=probes.add<JsonObject>();row["address"]=p.address;row["words"]=p.words;row["attempts"]=p.attempts;row["confirmed"]=p.confirmed();row["data"]=p.data();row["tx"]=hex(p.tx[0],p.txSize[0]);
    JsonArray replies=row["replies"].to<JsonArray>();for(unsigned k=0;k<p.attempts;++k){JsonObject reply=replies.add<JsonObject>();reply["tx"]=hex(p.tx[k],p.txSize[k]);reply["txBytes"]=p.txSize[k];reply["status"]=p.status[k];reply["accepted"]=bool(p.validMask&(1<<k));reply["truncated"]=bool(p.flags&(1<<k));reply["attemptMs"]=p.durationMs[k];reply["rx"]=hex(p.rx[k],p.rxSize[k]);}
  }
}
void report(WebServer &server,const MeterLabSnapshot &view,bool download){
  JsonDocument doc;doc["version"]=FIRMWARE_VERSION;doc["schema"]=1;doc["ready"]=view.ready;doc["storageOK"]=view.storageOK;doc["activeId"]=view.activeId;doc["error"]=view.error;
  doc["identityPolicy"]="Operator supplied model and profile binding. FC03 responses do not identify a manufacturer.";
  const auto &scan=view.discovery;JsonObject progress=doc["discovery"].to<JsonObject>();progress["phase"]=discoveryName(scan.phase);progress["run"]=scan.run;progress["tested"]=scan.tested;progress["total"]=scan.total;progress["elapsedMs"]=scan.elapsedMs;
  progress["requests"]=scan.requests;progress["timeouts"]=scan.timeouts;progress["crcErrors"]=scan.crcErrors;progress["rejected"]=scan.rejected;progress["exceptions"]=scan.exceptions;progress["first"]=scan.config.first;progress["last"]=scan.config.last;progress["matrix"]=scan.config.matrix;progress["timeout"]=scan.config.responseMs;JsonArray plan=progress["plan"].to<JsonArray>();for(unsigned i=0;i<scan.config.count;++i){JsonObject row=plan.add<JsonObject>();row["address"]=scan.config.plan[i].address;row["words"]=scan.config.plan[i].words;}
  JsonArray found=doc["findings"].to<JsonArray>();for(unsigned i=0;i<scan.count;++i){JsonObject row=found.add<JsonObject>();row["index"]=i;deviceJSON(row,scan.found[i]);}
  deviceJSON(doc["current"].to<JsonObject>(),scan.current);
  JsonArray saved=doc["saved"].to<JsonArray>();for(unsigned i=0;i<view.registry.count;++i){const auto &m=view.registry.entries[i];JsonObject row=saved.add<JsonObject>();row["id"]=m.id;row["name"]=m.name;row["model"]=m.model;row["notes"]=m.notes;row["profile"]=profiles::valid(m.profile)?profiles::get(m.profile).key:"unknown";row["verified"]=m.verified;Settings settings;row["activatable"]=canActivateSaved(m,settings);deviceJSON(row["evidence"].to<JsonObject>(),m.evidence);}
  if(download)server.sendHeader("Content-Disposition","attachment; filename=meter-lab-report.json");
  server.sendHeader("Cache-Control","no-store");String body;serializeJson(doc,body);server.send(200,"application/json",body);
}
void error(WebServer &server,const char *message){JsonDocument d;d["code"]="invalid_meter_lab";d["message"]=message;String body;serializeJson(d,body);server.send(400,"application/json",body);}
}
void registerMeterLabWeb(WebServer &server,const MeterLabHooks &hooks){
  server.on("/api/meter-lab",HTTP_GET,[&server,hooks](){report(server,hooks.read(),server.arg("download")=="1");});
  server.on("/api/meter-lab/stop",HTTP_POST,[&server,hooks](){if(!hooks.authorize())return;uint32_t run=0;if(!number(server.arg("run"),run)||!run){error(server,"Choose the current discovery run");return;}hooks.stop(run);});
  server.on("/api/meter-lab",HTTP_POST,[&server,hooks](){
    if(!hooks.authorize())return;MeterLabRequest request;String action=server.arg("action");uint32_t value=0;
    if(action=="start"){
      const auto &view=hooks.read();if(!view.ready){error(server,"Meter service is starting");return;}
      if(!number(server.arg("first"),value)||value<1||value>247){error(server,"Use first ID 1-247");return;}request.config.first=uint8_t(value);
      if(!number(server.arg("last"),value)||value<1||value>247){error(server,"Use last ID 1-247");return;}request.config.last=uint8_t(value);
      if(!number(server.arg("timeout"),value)||value<150||value>3000){error(server,"Use response timeout 150-3000 ms");return;}request.config.responseMs=uint16_t(value);
      String mode=server.arg("mode");if(mode!="current"&&mode!="matrix"){error(server,"Choose current or matrix format");return;}request.config.matrix=mode=="matrix";
      if(!parseDiscoveryPlan(server.arg("plan").c_str(),request.config)){error(server,"Use up to eight unique HEX:WORDS probes separated by commas");return;}
      if(!validDiscoveryConfig(request.config)){error(server,"Invalid or repeated probe plan");return;}
    }else if(action=="save"){
      request.operation=LAB_SAVE;
      if(!number(server.arg("run"),request.run)||!request.run||!number(server.arg("index"),value)||value>=DISCOVERY_RESULTS){error(server,"Select a completed finding from the current run");return;}request.index=uint8_t(value);
      String name=server.arg("name"),model=server.arg("model"),notes=server.arg("notes");
      if(name.length()>=sizeof(request.name)||model.length()>=sizeof(request.model)||notes.length()>=sizeof(request.notes)){error(server,"Name, model or notes exceed their byte limit");return;}
      strcpy(request.name,name.c_str());strcpy(request.model,model.c_str());strcpy(request.notes,notes.c_str());
      if(!meterText(request.name,sizeof(request.name),true)||!meterText(request.model,sizeof(request.model),true)||!meterText(request.notes,sizeof(request.notes),false)){error(server,"Use printable ASCII for name, model and notes");return;}
      String profile=server.arg("profile");if(profile!="unknown"&&!profiles::parse(profile.c_str(),request.profile)){error(server,"Unknown profile binding");return;}
      request.verified=server.arg("verified")=="1";
      if(request.verified&&!profiles::valid(request.profile)){error(server,"A verified binding needs a supported profile");return;}
    }else if(action=="remove"||action=="select"){
      request.operation=action=="remove"?LAB_REMOVE:LAB_SELECT;
      if(!number(server.arg("id"),value)||!value||value>65535){error(server,"Invalid saved meter ID");return;}request.meterId=uint16_t(value);
    }else{error(server,"Unknown meter operation");return;}
    hooks.submit(request);
  });
}
} // namespace fort
