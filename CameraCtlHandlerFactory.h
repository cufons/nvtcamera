//
// Created by cufon on 28.02.24.
//

#ifndef NVTCAMERA_CAMERACTLHANDLERFACTORY_H
#define NVTCAMERA_CAMERACTLHANDLERFACTORY_H

#include "ProtocolUserHandlerFactory.h"
class CameraCtlHandlerFactory : public protocom::ProtocolUserHandlerFactory{
public:
    protocom::ProtocolStateHandler *
    createHandler(protocom::ProtocolContext &ctx, protocom::MessageCoder *coder) const override;
};


#endif //NVTCAMERA_CAMERACTLHANDLERFACTORY_H
