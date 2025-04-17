#include <iostream>
#include <csignal>

extern "C" {
#include <nvtcodec.h>
}
#include "VideoEncoder.h"
#include "H264LiveServerMediaSession.h"
#include "ISP.h"
#include "Server.h"
#include "CameraCtlHandlerFactory.h"
#include "CameraCtlHandler.h"
#include "cameractl.pb.h"
#include <liveMedia/GroupsockHelper.hh>
#include <liveMedia/BasicUsageEnvironment.hh>
#include <thread>
#include "GPIO.h"
#include "StepperMotor.h"
#include "debug_info.h"
#include "INIParser.h"
#include "ADC.h"
#include "ModeSwitchController.h"
#include <sys/prctl.h>
#define DEFAULT_CFG_PATH "/mnt/mtd/nvtcamera/static.ini"
volatile bool isAppRunning = true;
static INIParser* staticConfig = nullptr;
static INIParser* persistentConfig = nullptr;
static INIParser* userBase = nullptr;

static ModeSwitchController* modeSwitchController = nullptr;

static UserAuthenticationDatabase* rtspServerAuthBase = nullptr;
static protocom::Authenticator* pcomServerAuthBase = nullptr;
static struct VencInstances {
    NVTSYS::VideoEncoder* mainVenc;
    NVTSYS::VideoEncoder* auxVenc;
} vencInstances{nullptr};

static struct SystemPeripherals {
    NVTSYS::ISP* isp;
    NVTSYS::GPIO* gpio;
    NVTSYS::ADC* adc;
    NVTSYS::StepperMotor* panMotor;
    NVTSYS::StepperMotor* tiltMotor;
} peripherals{nullptr};

void globalAuthnRemoveUser(const std::string& username) {
    if(rtspServerAuthBase) {
        rtspServerAuthBase->removeUserRecord(username.c_str());
    }
    if(pcomServerAuthBase) {
        pcomServerAuthBase->delUser(username);
    }
}
void globalAuthnAddUser(const std::string& username, const std::string& password) {
    if(rtspServerAuthBase) {
        rtspServerAuthBase->addUserRecord(username.c_str(),password.c_str());
    }

    if(pcomServerAuthBase) {
        pcomServerAuthBase->addUser(username,new protocom::PasswordAuthentication(password));
    }
}
void rtspThreadWorker(RTSPServer*& rtspServerPtr, NVTSYS::VideoEncoder& vencInstance, volatile char* taskSchedulerFlag) {
    prctl(PR_SET_NAME,"RTSP thread",NULL,NULL,NULL);
    std::cout << "RTSP thread worker started." << std::endl;
    ConfigSection* rtspConfig = staticConfig->findSection("rtsp");
    if(rtspConfig == nullptr) {
        std::cout << "RTSP config section not found or incorrect." << std::endl;
        raise(SIGTERM);
        return;
    }
    TaskScheduler* pTaskScheduler = BasicTaskScheduler::createNew();
    BasicUsageEnvironment* usageEnvironment = BasicUsageEnvironment::createNew(*pTaskScheduler);



    unsigned long rtspPort;
    if(!rtspConfig->getValueUint("port",rtspPort) && rtspPort < 65536) {
        rtspPort = 554;
    }
    RTSPServer* rtspServer = RTSPServer::createNew(*usageEnvironment, rtspPort, NULL);
    rtspServerPtr = rtspServer;
    if(rtspServer == NULL)
    {
        *usageEnvironment << "Failed to create rtsp server : " << usageEnvironment->getResultMsg() <<"\n";
        raise(SIGTERM);
        return;
    }
    std::string streamDesc = "Live IP camera";
    rtspConfig->getValueStr("stream_desc_main", streamDesc);
    if(userBase != nullptr) {
        auto* authBase = new UserAuthenticationDatabase;
        for(auto i = userBase->getSectionTableBegin(); i!= userBase->getSectionTableEnd(); ++i) {
            std::string userPass;
            if(!i->second.getValueStr("password",userPass))continue;
            authBase->addUserRecord(i->first.c_str(),userPass.c_str());
        }
        rtspServer->setAuthenticationDatabase(authBase);
        rtspServerAuthBase = authBase;
    }
    std::string streamName = "cam1";
    ServerMediaSession* sms = ServerMediaSession::createNew(*usageEnvironment, streamName.c_str(), streamName.c_str(), streamDesc.c_str());
    H264LiveServerMediaSession *liveSubSession = H264LiveServerMediaSession::createNew(*usageEnvironment, true,vencInstance);
    sms->addSubsession(liveSubSession);
    rtspServer->addServerMediaSession(sms);
    char* url = rtspServer->rtspURL(sms);
    *usageEnvironment << "Play the stream using url "<<url << "\n";
    delete[] url;
    pTaskScheduler->doEventLoop(taskSchedulerFlag);
}

void controlThreadWorker(protocom::Server*& srv, NVTSYS::VideoEncoder& vencInstance) {
    prctl(PR_SET_NAME,"control thread",NULL,NULL,NULL);
    std::cout << "Control thread worker started." << std::endl;
    ConfigSection* controlCfg = staticConfig->findSection("control");
    if(controlCfg == nullptr) {
        std::cout << "Control thread config section not found or incorrect." << std::endl;
        isAppRunning = false;
        raise(SIGTERM);
        return;
    }
    unsigned long controlPort;
    unsigned long rtspPort;
    if(!controlCfg->getValueUint("port",controlPort) && controlPort < 65536) {
        controlPort = 4444;
    }
    srv = new protocom::Server("0.0.0.0",controlPort);
    srv->setUserHandlerFactory(new CameraCtlHandlerFactory());

    if(userBase != nullptr) {
        auto* authn = new protocom::Authenticator;
        for(auto i = userBase->getSectionTableBegin(); i!= userBase->getSectionTableEnd(); ++i) {
            std::string userPass;
            if(!i->second.getValueStr("password",userPass))continue;
            authn->addUser(i->first,new protocom::PasswordAuthentication(userPass));
        }
        srv->setAuthenticator(authn);
        pcomServerAuthBase = authn;
    }


    if(srv->bindSock()) {
        std::cout << "Control socket bound on port 4444" << std::endl;
        srv->run();
    } else {
        std::cout << "Control socket failed to bound" << std::endl;
        isAppRunning = false;
        raise(SIGTERM);
        return;
    }
    delete srv;
}

void termHandler(int signum) {
    std::cout << "Signal received. Terminating the application..." << std::endl;
    isAppRunning = false;
}
void testCtlHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kPingRequest);
    resp.set_pingresponse(true);
    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}

void vencSetParamsHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kSetVencSettings);
    NVTSYS::VideoEncoder* targetVenc;
    switch(msg.setvencsettings().tgtvenc()) {
        case VENC_MAIN:
            targetVenc = vencInstances.mainVenc;
            break;
        case VENC_AUX:
            //TODO implement
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Not implemented");
            return;
        default:
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Not implemented");
            return;
    }
    targetVenc->setCodecSettingsFromPB(msg.setvencsettings().newsettings());
    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}

void vencGetParamsHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kGetVencSettings);
    NVTSYS::VideoEncoder* targetVenc;
    switch(msg.getvencsettings()) {
        case VENC_MAIN:
            targetVenc = vencInstances.mainVenc;
            break;
        case VENC_AUX:
            //TODO implement
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Not implemented");
            return;
        default:
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Not implemented");
            return;
    }
    targetVenc->getCodecSettingsInPB(*resp.mutable_currentsettings());
    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}

void motorMoveHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kMoveMotor);
    NVTSYS::StepperMotor* targetMotor;
    switch(msg.movemotor().motor()) {
        case STEP_MOTOR_PAN:
            targetMotor = peripherals.panMotor;
            break;
        case STEP_MOTOR_TILT:
            targetMotor = peripherals.tiltMotor;
            break;
        default:
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Invalid motor type");
            return;
    }
    if(!targetMotor) {
        resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
        resp.set_errorreason("Requested motor is not supported");
        return;
    }
    switch(msg.movemotor().dir()) {
        case STEP_DIRECTION_FORWARD:
            targetMotor->advance(msg.movemotor().steps());
            break;
        case STEP_DIRECTION_REVERSE:
            targetMotor->reverse(msg.movemotor().steps());
            break;
        default:
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Invalid direction");
            return;
    }
    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}
void ispSettingsGetHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kIspSettingGetReq);
    ISPSettings* sentSettings = resp.mutable_currentispsettings();
    if(peripherals.isp == nullptr) {
        resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
        resp.set_errorreason("ISP is not initialized yet!");
        return;
    }
    sentSettings->set_brightness(peripherals.isp->getBrightness());
    sentSettings->set_contrast(peripherals.isp->getContrast());
    sentSettings->set_eelevel(peripherals.isp->getEELevel());
    sentSettings->set_hue(peripherals.isp->getHue());
    sentSettings->set_nightmode(peripherals.isp->getNightFilterEnable());
    sentSettings->set_nrlevel(peripherals.isp->getNRLevel());
    sentSettings->set_saturation(peripherals.isp->getSaturation());
    sentSettings->set_tnrlevel(peripherals.isp->getTNR());

    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}
void ispSettingsSetHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kNewIspSettings);
    const ISPSettings& newSettings = msg.newispsettings();
    if(peripherals.isp == nullptr) {
        resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
        resp.set_errorreason("ISP is not initialized yet!");
        return;
    }

    if(newSettings.has_selectedpreset()) {
        bool result;
        switch(newSettings.selectedpreset()) {
            case ISPSettings_ISPPreset_PRESET_DAY:
                result = peripherals.isp->loadPresetCfg(NVTSYS::ISP::cfgPresetDayPath);
                break;
            case ISPSettings_ISPPreset_PRESET_NIGHT:
                result = peripherals.isp->loadPresetCfg(NVTSYS::ISP::cfgPresetNightPath);
                break;
            case ISPSettings_ISPPreset_PRESET_NIGHT_IR:
                result = peripherals.isp->loadPresetCfg(NVTSYS::ISP::cfgPresetNightIRPath);
                break;
            default:
                break;
        }
        if(!result) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Failed to set requested preset!");
            return;
        }
    }

    if(newSettings.has_nightmode()) {
        if(!peripherals.isp->setNightFilterEnable(newSettings.nightmode())) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Failed to set night mode!");
            return;
        }
    }

    if(newSettings.has_brightness()) {
        if(!peripherals.isp->setBrightness(newSettings.brightness())) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Failed to set requested brightness!");
            return;
        }
    }
    if(newSettings.has_saturation()) {
        if(!peripherals.isp->setSaturation(newSettings.saturation())) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Failed to set requested saturation!");
            return;
        }
    }
    if(newSettings.has_hue()) {
        if(!peripherals.isp->setHue(newSettings.hue())) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Failed to set requested hue!");
            return;
        }
    }
    if(newSettings.has_contrast()) {
        if(!peripherals.isp->setContrast(newSettings.contrast())) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Failed to set requested contrast!");
            return;
        }
    }
    if(newSettings.has_nrlevel()) {
        if(!peripherals.isp->setNRLevel(newSettings.nrlevel())) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Failed to set requested NR level!");
            return;
        }
    }
    if(newSettings.has_eelevel()) {
        if(!peripherals.isp->setEELevel(newSettings.eelevel())) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Failed to set requested EE level!");
            return;
        }
    }
    if(newSettings.has_tnrlevel()) {
        if(!peripherals.isp->setTNR(newSettings.tnrlevel())) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Failed to set requested TNR level!");
            return;
        }
    }
    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}
void irLightEnableHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kEnableIRLight);
    peripherals.gpio->pinWrite(GPIO_PIN_IR_LIGHT,msg.enableirlight() ? NVTSYS::GPIO::GPIO_STATE_HIGH : NVTSYS::GPIO::GPIO_STATE_LOW);
    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}
void userFetchHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kFetchUserList);
    if(userBase == nullptr) {
        resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
        resp.set_errorreason("User database does not exist!");
        return;
    }
    for(auto i = userBase->getSectionTableBegin(); i!= userBase->getSectionTableEnd(); ++i) {
        resp.mutable_userlist()->add_username(i->first);
    }

    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}

void userModHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kUserModRequest);
    if(userBase == nullptr) {
        resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
        resp.set_errorreason("User database does not exist!");
        return;
    }
    if(!msg.usermodrequest().exists()) {
        auto* userRecord = userBase->findSection(msg.usermodrequest().username());
        if(userRecord == nullptr) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Requested user does not exist!");
            return;
        }
        userBase->deleteSection(msg.usermodrequest().username());
        globalAuthnRemoveUser(msg.usermodrequest().username());
    } else {
        if(!msg.usermodrequest().has_password()) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("Will not create a user without password!");
            return;
        }
        auto* userRecord = userBase->createSection(msg.usermodrequest().username());
        if(userRecord == nullptr) {
            resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
            resp.set_errorreason("User creation failed!");
            return;
        }
        userRecord->addKVPair("password",msg.usermodrequest().password());
        globalAuthnAddUser(msg.usermodrequest().username(),msg.usermodrequest().password());
    }
    userBase->save();
    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}

void msParamsGetHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kMsParamsGetReq);
    if(modeSwitchController == nullptr) {
        resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
        resp.set_errorreason("Mode switch controller has not started yes or is not supported!");
        return;
    }
    resp.mutable_currentmsparams()->set_irenable(modeSwitchController->isIrEnabled());
    if(modeSwitchController->isAutoControlEnabled()) {
        resp.mutable_currentmsparams()->set_mode(ModeSwitchParams_MSMode_MS_AUTO);
    } else {
        switch(modeSwitchController->getCurrentMode()) {
            case ModeSwitchController::MODE_DAY:
                resp.mutable_currentmsparams()->set_mode(ModeSwitchParams_MSMode_MS_DAY);
                break;
            case ModeSwitchController::MODE_NIGHT:
                resp.mutable_currentmsparams()->set_mode(ModeSwitchParams_MSMode_MS_NIGHT);
                break;
            default:
                resp.mutable_currentmsparams()->set_mode(ModeSwitchParams_MSMode_MS_UNKNOWN);
                break;
        }
    }
    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}

void msParamsSetHandler(const CamCtlRequest& msg, CamCtlResponse& resp) {
    assert(msg.cmdBody_case() == CamCtlRequest::kNewMsParams);
    if(modeSwitchController == nullptr) {
        resp.set_responsetype(CamCtlResponse_ResponseType_REQUEST_ERROR);
        resp.set_errorreason("Mode switch controller has not started yes or is not supported!");
        return;
    }
    if(msg.newmsparams().has_irenable()) {
        modeSwitchController->setIrEnabled(msg.newmsparams().irenable());
    }

    if(msg.newmsparams().has_mode()) {
        switch (msg.newmsparams().mode()) {
            case ModeSwitchParams_MSMode_MS_AUTO:
                modeSwitchController->EnableAutoControl();
                break;
            case ModeSwitchParams_MSMode_MS_DAY:
                modeSwitchController->SetDayMode();
                break;
            case ModeSwitchParams_MSMode_MS_NIGHT:
                modeSwitchController->SetNightMode();
                break;
        }
    }
    resp.set_responsetype(CamCtlResponse_ResponseType_OK);
}
int main(int argc, char** argv) {
    CameraCtlHandler::registerHandler(CamCtlRequest::kPingRequest,testCtlHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kGetVencSettings,vencGetParamsHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kSetVencSettings,vencSetParamsHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kMoveMotor,motorMoveHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kIspSettingGetReq,ispSettingsGetHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kNewIspSettings,ispSettingsSetHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kEnableIRLight,irLightEnableHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kFetchUserList,userFetchHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kUserModRequest,userModHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kMsParamsGetReq,msParamsGetHandler);
    CameraCtlHandler::registerHandler(CamCtlRequest::kNewMsParams,msParamsSetHandler);
    bool hasForked = false;
    std::string cfgPath = DEFAULT_CFG_PATH;
    for(int i = 1; i < argc; i++) {
        if(argv[i] == std::string("-f") && !hasForked) {
            hasForked = true;
            std::cout << "Forking to background" << std::endl;
            int pid = fork();
            if(pid == -1) {
                perror("Fork failed!");
                return -1;
            }
            if(pid != 0) {
                return 0;
            }
        }
        if(argv[i] == std::string("-c") && argc > i+1) {
            cfgPath = argv[i+1];
        }
    }
    std::cout << "Loading default config from " << DEFAULT_CFG_PATH << std::endl;
    staticConfig = new INIParser();
    staticConfig->setFilePath(DEFAULT_CFG_PATH);
    if(!staticConfig->load()) {
        DBG_ERROR("Failed to load config file!");
        return -1;
    }
    std::string userBasePath, persistentConfigPath;
    ConfigSection* mainSection = staticConfig->findSection("main");
    if(mainSection == nullptr || !mainSection->getValueStr("userdb_path",userBasePath) || !mainSection->getValueStr("persist_path",persistentConfigPath)) {
        DBG_ERROR("Main section nonexistent or is not in the correct format!");
        return -1;
    }


    persistentConfig = new INIParser;
    persistentConfig->setFilePath(persistentConfigPath);
    if(!persistentConfig->load()) {
        DBG_ERROR("Failed to load persistent config!");
        return -1;
    }
    ConfigSection* securityOptions = persistentConfig->findSection("security");
    if(securityOptions != nullptr) {
        bool anonEnabled;
        if(!securityOptions->getValueBool("anon_access",anonEnabled)) {
            anonEnabled = false;
        };

        if(!anonEnabled) {
            userBase = new INIParser();
            userBase->setFilePath(userBasePath);
            if(!userBase->load()) {
                DBG_ERROR("Failed to load user base!");
                return -1;
            }
        }
    }

    NVTSYS::VideoEncoder::globalInit();


    NVTSYS::ISP::init();

    NVTSYS::ISP* isp = NVTSYS::ISP::getInstance();

    peripherals.isp = isp;
    isp->loadPresetCfg(NVTSYS::ISP::cfgPresetDayPath);

    NVTSYS::GPIO::init();

    peripherals.gpio =  NVTSYS::GPIO::getInstance();

    NVTSYS::ADC::init();

    peripherals.adc = NVTSYS::ADC::getInstance();

    NVTSYS::StepperMotor::globalInit();

    peripherals.panMotor = NVTSYS::StepperMotor::getPanMotorInstance();
    peripherals.tiltMotor = NVTSYS::StepperMotor::getTiltMotorInstance();

    peripherals.gpio->pinMode(GPIO_PIN_IR_LIGHT,NVTSYS::GPIO::GPIO_MODE_OUT);
    peripherals.gpio->pinWrite(GPIO_PIN_IR_LIGHT,NVTSYS::GPIO::GPIO_STATE_LOW);

    for(int i = 0; i < peripherals.adc->getChanCount(); i++) {
        std::cout << "ADC Channel " << i << " reads " << peripherals.adc->readChannel(i) << " counts" << std::endl;
    }
    std::cout << "Set IR light off!" << std::endl;

    modeSwitchController = new ModeSwitchController();
    modeSwitchController->StartAutoControl();
    const int chnum = 0;

    NVTSYS::VideoEncoder camVenc(0);
    vencInstances.mainVenc = &camVenc;
    camVenc.setDefaultSettings();


    RTSPServer* rtspSrvHandle = nullptr;
    volatile char liveTaskSchedulerShouldStop = 0;
    std::thread rtspThread(rtspThreadWorker, std::ref(rtspSrvHandle), std::ref(camVenc), &liveTaskSchedulerShouldStop);

    protocom::Server* ctlSrvHandle = nullptr;
    std::thread controlThread(controlThreadWorker,std::ref(ctlSrvHandle),std::ref(camVenc));
    signal(SIGINT,termHandler);
    signal(SIGTERM,termHandler);
    std::cout << "Init complete..." << std::endl;
    while(isAppRunning) {
        sleep(2);
    }
    std::cout << "Stopping RTSP thread..." << std::endl;
    liveTaskSchedulerShouldStop = 1;
    rtspThread.join();
    Medium::close(rtspSrvHandle);
    std::cout << "Stopping control thread..." << std::endl;
    ctlSrvHandle->stop();
    controlThread.join();
    delete modeSwitchController;
    return 0;
}
