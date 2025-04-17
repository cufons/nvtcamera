/*===============================================================================

  Project: H264LiveStreamer
  Module: VencLiveSourceH264.cxx

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

#include "VencLiveSourceH264.h"
#include "debug_info.h"
#include "VideoEncoder.h"

VencLiveSourceH264* VencLiveSourceH264::createNew(UsageEnvironment& env, NVTSYS::VideoEncoder& venc)
{
  return new VencLiveSourceH264(env,venc);
}

EventTriggerId VencLiveSourceH264::eventTriggerId = 0;

unsigned VencLiveSourceH264::referenceCount = 0;

VencLiveSourceH264::VencLiveSourceH264(UsageEnvironment& env, NVTSYS::VideoEncoder& venc): FramedSource(env), venc(venc), isProfilingEnabled(
        false)
{
  if(referenceCount == 0)
    {

    }
  ++referenceCount;
  //videoCaptureDevice.open(0);
  //encoder = new x264Encoder();
  //encoder->initilize();
  venc.start();
  while(venc.popNALU()); // flush nal queue
  if(eventTriggerId == 0)
    {
      eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
    }
}


VencLiveSourceH264::~VencLiveSourceH264(void)
{
  --referenceCount;
  venc.stop();
  //videoCaptureDevice.release();
  //encoder->unInitilize();
  envir().taskScheduler().deleteEventTrigger(eventTriggerId);
  eventTriggerId = 0;
}
/*
void VencLiveSourceH264::encodeNewFrame()
{
  rawImage.data = NULL;
  while(rawImage.data == NULL)
    {
      videoCaptureDevice >> rawImage;
      cv::waitKey(100);
    }
  // Got new image to stream
  assert(rawImage.data != NULL);
  encoder->encodeFrame(rawImage);
  // Take all nals from encoder output queue to our input queue
  while(encoder->isNalsAvailableInOutputQueue() == true)
    {
      x264_nal_t nal = encoder->getNalUnit();
      nalQueue.push(nal);
    }
}
*/
void VencLiveSourceH264::deliverFrame0(void* clientData)
{
  ((VencLiveSourceH264*)clientData)->deliverFrame();
}

void VencLiveSourceH264::doGetNextFrame()
{
    if(isProfilingEnabled)
    if(!venc.getRunning()) {
        DBG_WARN("Venc not running! Attempting to start");
        venc.start();
    }
    if(isProfilingEnabled) vencReqStart = high_resolution_clock::now();
  if(!venc.isNALUAvail())
    {
      gettimeofday(&currentTime,NULL);
      int retryCount = 1;

      while(true) {
          try {
              venc.requestStreamData();
          } catch (NVTSYS::VideoEncoder::VencException &e) {
              if(retryCount > maxRetries) {
                  handleClosure();
                  return;
              }
              envir() << "Exception during frame request from nvt\n";
              envir() << "Attempting to restart venc...";
              envir() << "Try " << retryCount << "/" << maxRetries << "\n";
              retryCount++;
              venc.stop();
              usleep(200000);
              venc.start();
              continue;
          }
          break;
      }
        deliverFrame();
    } else {
      deliverFrame();
  }
    if(isProfilingEnabled) vencReqEnd = high_resolution_clock::now();

    if(isProfilingEnabled) deliverStart = high_resolution_clock::now();

    if(isProfilingEnabled) deliverEnd = high_resolution_clock::now();

    if(isProfilingEnabled) {
        const char* ftype_string = "";
        switch (venc.getLastFrameType()) {
            default:
                ftype_string = "unknown";
                break;
            case NVTSYS::VideoEncoder::FTYPE_IDR:
                ftype_string = "IDR";
                break;
            case NVTSYS::VideoEncoder::FTYPE_P:
                ftype_string = "P";
                break;
        }
        microseconds requestTime = duration_cast<microseconds>(vencReqEnd - vencReqStart);
        microseconds deliveryTime = duration_cast<microseconds>(deliverEnd - deliverStart);
        microseconds totalTime = requestTime + deliveryTime;
                DBG_INFO(ftype_string << " Frame delivered in " << totalTime.count() << " us. " <<\
        "(" <<requestTime.count() << " us - Encoding, " << deliveryTime.count()) << " us - Delivery)." << std::endl;
    }
}

void VencLiveSourceH264::deliverFrame()
{
  if(!isCurrentlyAwaitingData() ) return;
  if(!venc.isNALUAvail()) {
      DBG_WARN("called on an empty queue!");
      FramedSource::afterGetting(this);
      return;
  }
  NVTSYS::vencNALU nal = venc.getNALU();
  assert(nal.nalu_data != NULL);
  // You need to remove the start code which is there in front of every nal unit.
  // the start code might be 0x00000001 or 0x000001. so detect it and remove it. pass remaining data to live555
  int trancate = 0;
  if (nal.nalu_size >= 4 && nal.nalu_data[0] == 0 && nal.nalu_data[1] == 0 && nal.nalu_data[2] == 0 && nal.nalu_data[3] == 1 )
    {
      trancate = 4;
    }
  else
    {
      if(nal.nalu_size >= 3 && nal.nalu_data[0] == 0 && nal.nalu_data[1] == 0 && nal.nalu_data[2] == 1 )
        {
          trancate = 3;
        }
    }

  if(nal.nalu_size-trancate > fMaxSize)
    {
      fFrameSize = fMaxSize;
      fNumTruncatedBytes = nal.nalu_size-trancate - fMaxSize;
    }
  else
    {
      fFrameSize = nal.nalu_size-trancate;
    }
  //fPresentationTime = nal.timestamp;
  fPresentationTime = currentTime;
  memmove(fTo,nal.nalu_data+trancate,fFrameSize);
  FramedSource::afterGetting(this);
  venc.popNALU();
}

void VencLiveSourceH264::setIsProfilingEnabled(bool isProfilingEnabled) {
    VencLiveSourceH264::isProfilingEnabled = isProfilingEnabled;
}
