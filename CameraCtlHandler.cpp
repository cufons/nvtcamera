//
// Created by cufon on 19.01.24.
//

#include "CameraCtlHandler.h"
#include "debug_info.h"
std::unordered_map<CamCtlRequest::CmdBodyCase,CameraCtlHandler::RequestHandlerDef> CameraCtlHandler::globalHandlerTable;
void CameraCtlHandler::handleMsg(CamCtlRequest &msg) {
    response->Clear();
    response->set_responsetype(CamCtlResponse_ResponseType_INVALID_REQUEST);
    auto handlerIter =  globalHandlerTable.find(msg.cmdBody_case());
    if(handlerIter != globalHandlerTable.end()) {
        auto[reqType,reqHandler] = *handlerIter;
        try {
            reqHandler(msg, *response);
        } catch (std::exception& e) {
            DBG_ERROR("Exception " << e.what());
            response->set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            response->set_errorreason(e.what());
        }
    }
    sendResp();
}

void CameraCtlHandler::handleDecodeErr() {
    response->Clear();
    response->set_responsetype(CamCtlResponse_ResponseType_DECODE_ERROR);
    sendResp();
}

CameraCtlHandler::CameraCtlHandler(protocom::ProtocolContext &ctx, protocom::MessageCoder *coder)
        : ProtocolUserHandler(ctx, coder) {
    DBG_INFO("New connection handler created");
}

bool CameraCtlHandler::registerHandler(CamCtlRequest::CmdBodyCase reqType, CameraCtlHandler::RequestHandlerDef handler) {
    DBG_INFO("Registered handler for reqType " << reqType);
    auto[iter,success] = globalHandlerTable.insert(std::make_pair(reqType,handler));
    return success;
}
