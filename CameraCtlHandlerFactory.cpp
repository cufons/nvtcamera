//
// Created by cufon on 28.02.24.
//

#include "CameraCtlHandlerFactory.h"
#include "CameraCtlHandler.h"

protocom::ProtocolStateHandler *
CameraCtlHandlerFactory::createHandler(protocom::ProtocolContext &ctx, protocom::MessageCoder *coder) const {
    return new CameraCtlHandler(ctx,coder);
}
