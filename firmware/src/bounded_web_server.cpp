// SPDX-License-Identifier: GPL-3.0-only
#include "bounded_web_server.h"
#include "bounded_write.h"
#include <Arduino.h>
#include <lwip/sockets.h>
#include <errno.h>

namespace fort {
BoundedWebServer::BoundedWebServer(uint16_t port,void (*idlePump)())
  : WebServer(port),idlePump_(idlePump){}

void BoundedWebServer::handleClient(){
  // Preserve the previous 250ms speculative-socket policy. This does NOT
  // impose a timeout on an incomplete header inside WebServer::_parseRequest().
  if(_currentStatus==HC_WAIT_READ&&!_currentClient.available()&&
     uint32_t(millis()-_statusChange)>250&&_server.hasClient()){
    _currentClient.stop();
    _currentStatus=HC_NONE;
  }
  requestStarted_=millis();
  WebServer::handleClient();
}

size_t BoundedWebServer::_currentClientWrite(const char *data,size_t size){
  size_t sent=boundedWrite(data,size,requestStarted_,5000,
    [](){return millis();},
    [this](){return bool(_currentClient.connected());},
    [this](const char *bytes,size_t count){
      int n=::send(_currentClient.fd(),bytes,count,MSG_DONTWAIT);
      return n>=0?n:(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?0:-1);
    },
    [this](){if(idlePump_)idlePump_();});
  if(sent<size)_currentClient.stop();
  return sent;
}

size_t BoundedWebServer::_currentClientWrite_P(PGM_P data,size_t size){
  return _currentClientWrite(data,size);
}
} // namespace fort
