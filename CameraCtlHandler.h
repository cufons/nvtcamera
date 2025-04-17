//
// Created by cufon on 19.01.24.
//

#ifndef NVTCAMERA_CAMERACTLHANDLER_H
#define NVTCAMERA_CAMERACTLHANDLER_H

#include "ProtocolUserHandler.h"
#include "cameractl.pb.h"
#include <functional>
#include <unordered_map>
class CameraCtlHandler : public protocom::ProtocolUserHandler<CamCtlRequest,CamCtlResponse>{
public:
    using RequestHandlerDef = std::function<void(const CamCtlRequest&,CamCtlResponse&)>;
private:
    static std::unordered_map<CamCtlRequest::CmdBodyCase,RequestHandlerDef> globalHandlerTable;
    void handleMsg(CamCtlRequest& msg) override;
    void handleDecodeErr() override;
public:
    CameraCtlHandler(protocom::ProtocolContext &ctx, protocom::MessageCoder *coder);
    static bool registerHandler(CamCtlRequest::CmdBodyCase reqType, RequestHandlerDef handler);
};


#endif //NVTCAMERA_CAMERACTLHANDLER_H
