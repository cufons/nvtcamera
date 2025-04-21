/*===============================================================================

  Project: H264LiveStreamer
  Module: VencLiveSourceH264.h

  Copyright (c) 2014-2015, Rafael Palomar <rafaelpalomaravalos@gmail.com>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

  =============================================================================*/

#ifndef __LIVE_SOURCE_WITH_X264_H
#define __LIVE_XOURCE_WITH_X264_H


#include <queue>
#include <chrono>
#include <liveMedia/FramedSource.hh>
#include "VideoEncoder.h"
using namespace std::chrono;
class VencLiveSourceH264: public FramedSource
{
    using ProfTimePoint = time_point<high_resolution_clock>;
 public:
  static VencLiveSourceH264* createNew(UsageEnvironment& env, NVTSYS::VideoEncoder& venc);
  static EventTriggerId eventTriggerId;
 protected:
  VencLiveSourceH264(UsageEnvironment& env, NVTSYS::VideoEncoder& venc);
  virtual ~VencLiveSourceH264(void);
 private:
    bool isProfilingEnabled;
public:
    void setIsProfilingEnabled(bool isProfilingEnabled);

private:
    static const int maxRetries = 3;
    ProfTimePoint deliverStart, deliverEnd;
    ProfTimePoint vencReqStart, vencReqEnd;
  virtual void doGetNextFrame();
  static void deliverFrame0(void* clientData);
  void deliverFrame();
  void encodeNewFrame();
  static unsigned referenceCount;
  NVTSYS::VideoEncoder& venc;
  //std::queue<x264_nal_t> nalQueue;
  timeval currentTime;
  // videoCaptureDevice is my BGR data source. You can have according to your need
  //cv::VideoCapture videoCaptureDevice;
  //cv::Mat rawImage;
  // Remember the x264 encoder wrapper we wrote in the start
  //x264Encoder *encoder;
};

#endif
