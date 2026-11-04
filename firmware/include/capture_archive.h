// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "checkpoint_log.h"
#include "recovery.h"
namespace fort {
inline bool captureExtends(const CaptureDay &next,const CaptureDay &previous){
  if(next.number!=previous.number)return false;
  for(unsigned i=0;i<CAPTURE_COUNT;++i){
    const auto &old=previous.points[i];const auto &point=next.points[i];
    if(old.status==CAPTURE_MISSED&&point.status!=CAPTURE_MISSED)return false;
    if(old.status!=CAPTURE_RECORDED)continue;
    if(point.status!=CAPTURE_RECORDED||point.raw!=old.raw||point.energy!=old.energy||
       point.startedUTC!=old.startedUTC||point.capturedUTC!=old.capturedUTC||
       point.generation!=old.generation||!sameCaptureSource(point.source,old.source))return false;
  }
  return true;
}
// Caller owns the filesystem lock. Existing newer or conflicting immutable
// points must never be replaced by an older orphaned staging file.
template<class FS,class Path,class Reader>
bool promoteCapture(FS &fs,const Path &temp,const Path &path,const Path &backup,
                    const CaptureDay &staged,Reader read){
  CaptureDay existing;bool covered=false;
  if(!read(temp,existing)||existing.number!=staged.number||existing.revision!=staged.revision||
     existing.generation!=staged.generation||!captureExtends(existing,staged)||!captureExtends(staged,existing))return false;
  for(const Path &candidate:{path,backup}){
    if(!read(candidate,existing)||existing.number!=staged.number)continue;
    if(newerCaptureDay(existing,staged)){
      if(!captureExtends(existing,staged))return false;
      covered=true;
    }else{
      if(!captureExtends(staged,existing))return false;
      if(!newerCaptureDay(staged,existing)){
        if(!captureExtends(existing,staged))return false;
        covered=true;
      }
    }
  }
  if(covered)return !fs.exists(temp)||fs.remove(temp);
  return installVerifiedArchive(fs,temp,path,backup,[&](const Path &candidate){
    CaptureDay value;return read(candidate,value)&&value.number==staged.number;
  });
}
} // namespace fort
