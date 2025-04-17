//
// Created by cufon on 09.01.24.
//

#include "VideoEncoder.h"
#include "debug_info.h"
#include <chrono>
using namespace std::chrono;
using namespace std;
std::mutex NVTSYS::VideoEncoder::nvtLibMutex;
bool NVTSYS::VideoEncoder::isNvtLibInit = false;
NVTSYS::VideoEncoder::VideoEncoder(unsigned int chnum) : chnum(chnum), isRunning(false), isFirstFrameRequested(true), lastFrameType(FTYPE_UNKNOWN) {
    if(!isNvtLibInit) throw runtime_error("NvtLib is not initialized yet!");
}

void NVTSYS::VideoEncoder::start() {
    ACCESS_GUARD_ACQUIRE;
    unlockedStart();
}
void NVTSYS::VideoEncoder::unlockedStart() {
    if(isRunning) {
        DBG_WARN("Venc already running");
    } else {
        int res = NvtCodec_StartVenc(chnum);
        if(res < 0) {
            DBG_ERROR("NvtCodec_StartVenc returned error code" << res);
            throw VencException(std::string("NvtCodec_StartVenc returned error code") + std::to_string(res));
        }
        DBG_INFO("Venc started");
        isRunning = true;
    }
}
void NVTSYS::VideoEncoder::stop() {
    ACCESS_GUARD_ACQUIRE;
    unlockedStop();
}

void NVTSYS::VideoEncoder::unlockedStop() {
    if (!naluQueue.empty()) {
        while (!naluQueue.empty()) naluQueue.pop();
        NvtCodec_ReleaseVencStream(chnum, &currentVencStream);
    }
    if(isRunning) {
        int res = NvtCodec_StopVenc(chnum);
        if (res < 0) {
            DBG_ERROR("NvtCodec_StopVenc returned error code" << res);
            throw VencException(string("NvtCodec_StopVenc returned error code") + to_string(res));
        }
        DBG_INFO("Venc stopped");
        isRunning = false;
    }
}

bool NVTSYS::VideoEncoder::getRunning() const {
    return isRunning;
}
NVTSYS::VideoEncoder::eLastFrameType NVTSYS::VideoEncoder::getLastFrameType() {
    return lastFrameType;
}
void NVTSYS::VideoEncoder::getCodecSettingsInPB(NvtCodecVencSettings &settings) {
    settings.Clear();
    CodecVencSetting nvt_settings;
    getRawCodecSettings(nvt_settings);

    settings.set_einputsource(static_cast<NvtCodecVencSettings_InputSource>(nvt_settings.eVencInputSource + 1));
    settings.set_eformat(static_cast<NvtCodecVencSettings_Format>(nvt_settings.eVencFormat+1));
    if(nvt_settings.eVencFormat == CODEC_VENC_FORMAT_H264) {
        settings.set_eprofile(static_cast<NvtCodecVencSettings_Profile>(nvt_settings.eVencProfile + 1));
    }

    settings.set_targetheight(nvt_settings.uiTargetHeight);
    settings.set_targetwidth(nvt_settings.uiTargetWidth);

    switch (nvt_settings.eBRCType) {
        case CODEC_VENC_BRC_CBR:
            convertCBRSettingsToPB(nvt_settings,settings);
            break;
        case CODEC_VENC_BRC_VBR:
            convertVBRSettingsToPB(nvt_settings,settings);
            break;
        case CODEC_VENC_BRC_EVBR:
            convertEVBRSettingsToPB(nvt_settings,settings);
            break;
        case CODEC_VENC_BRC_FIXQP:
            convertFixQPSettingsToPB(nvt_settings,settings);
            break;
        case CODEC_VENC_BRC_NONE:
            convertNoneBRCSettingsToPB(nvt_settings,settings);
        default:
            throw VencException("BRC type invalid");
    }

    settings.set_svctsettings(static_cast<NvtCodecVencSettings_VencSVCT>(nvt_settings.sSVCSetting.eSVCTemporalLayer));
    settings.set_enablenr(nvt_settings.sNRSetting.bEnable);
    settings.set_evencrt(static_cast<NvtCodecVencSettings_VencRotation>(nvt_settings.eVencRotateType));

    if(nvt_settings.sAQSetting.aq_enable) {
        auto* pb_aq = settings.mutable_aqsettings();
        pb_aq->set_aqstr(nvt_settings.sAQSetting.aq_str);
        pb_aq->set_maxsraq(nvt_settings.sAQSetting.max_sraq);
        pb_aq->set_minsraq(nvt_settings.sAQSetting.min_sraq);
        pb_aq->set_sraqconstaslog2(nvt_settings.sAQSetting.sraq_const_aslog2);
        pb_aq->set_sraqinitaslog2(nvt_settings.sAQSetting.sraq_init_aslog2);
    }

    settings.mutable_ltrsettings()->set_preref(nvt_settings.sLTRInfo.uiLTRPreRef);
    settings.mutable_ltrsettings()->set_interval(nvt_settings.sLTRInfo.uiLTRInterval);
    settings.set_seienable(nvt_settings.sSEISetting.bEnable);
}
void NVTSYS::VideoEncoder::setCodecSettingsFromPB(const NvtCodecVencSettings &settings) {
    CodecVencSetting nvt_settings;
    memset(&nvt_settings,0, sizeof nvt_settings);
    nvt_settings.eVencInputSource = static_cast<CodecVencInputSource>(settings.einputsource() - 1);
    nvt_settings.eVencFormat = static_cast<CodecVencFormat>(settings.eformat() - 1);
    if(settings.has_eprofile()) nvt_settings.eVencProfile = static_cast<CodecVencProfile>(settings.eprofile() - 1);

    nvt_settings.uiTargetWidth = settings.targetwidth();
    nvt_settings.uiTargetHeight = settings.targetheight();

    switch (settings.brcType_case()) {
        case NvtCodecVencSettings::kNoneBrcSettings:
            nvt_settings.eBRCType = CODEC_VENC_BRC_NONE;
            convertNoneBRCSettingsFromPB(nvt_settings, settings);
            break;
        case NvtCodecVencSettings::kCBRSettings:
            nvt_settings.eBRCType = CODEC_VENC_BRC_CBR;
            convertCBRSettingsFromPB(nvt_settings, settings);
            break;
        case NvtCodecVencSettings::kEVBRSettings:
            nvt_settings.eBRCType = CODEC_VENC_BRC_EVBR;
            convertEVBRSettingsFromPB(nvt_settings, settings);
            break;
        case NvtCodecVencSettings::kFIXQPSettings:
            nvt_settings.eBRCType = CODEC_VENC_BRC_FIXQP;
            convertFixQPSettingsFromPB(nvt_settings, settings);
            break;
        case NvtCodecVencSettings::kVBRSettings:
            nvt_settings.eBRCType = CODEC_VENC_BRC_VBR;
            convertVBRSettingsFromPB(nvt_settings, settings);
            break;
        case NvtCodecVencSettings::BRCTYPE_NOT_SET:
            throw VencException("No valid BRC type set");
    }

    nvt_settings.sSVCSetting.eSVCTemporalLayer = static_cast<CodecVencSVCT>(settings.svctsettings());
    nvt_settings.sNRSetting.bEnable = settings.enablenr();
    nvt_settings.eVencRotateType = static_cast<CodecVencRT>(settings.evencrt());

    if(settings.has_aqsettings()) {
        nvt_settings.sAQSetting.aq_enable = 1;
        nvt_settings.sAQSetting.aq_str = settings.aqsettings().aqstr();
        nvt_settings.sAQSetting.max_sraq = settings.aqsettings().maxsraq();
        nvt_settings.sAQSetting.min_sraq = settings.aqsettings().minsraq();
        nvt_settings.sAQSetting.sraq_const_aslog2 = settings.aqsettings().sraqconstaslog2();
        nvt_settings.sAQSetting.sraq_init_aslog2 = settings.aqsettings().sraqinitaslog2();
    }
    nvt_settings.sLTRInfo.uiLTRInterval = settings.ltrsettings().interval();
    nvt_settings.sLTRInfo.uiLTRPreRef = settings.ltrsettings().preref();

    nvt_settings.sSEISetting.bEnable = settings.seienable();

    ACCESS_GUARD_ACQUIRE;
    nvtStreamMutex.lock();
    unlockedStop();
    unlockedSetRawCodecSettings(nvt_settings);
    unlockedStart();
    nvtStreamMutex.unlock();
}

bool NVTSYS::VideoEncoder::validatePBCodecSettings(const NvtCodecVencSettings &settings) {
    if(settings.einputsource() > NvtCodecVencSettings::InputSource_MAX ||
       settings.eformat() > NvtCodecVencSettings::Format_MAX ||
       (settings.has_eprofile() && settings.eprofile() > NvtCodecVencSettings::Profile_MAX) ||
       settings.evencrt() > NvtCodecVencSettings::VencRotation_MAX ||
       settings.svctsettings() > NvtCodecVencSettings::VencSVCT_MAX) return false;

    if(settings.einputsource() == NvtCodecVencSettings_InputSource_INPUT_SOURCE_UNKNOWN ||
       settings.eformat() == NvtCodecVencSettings_Format_FORMAT_UNKNOWN ||
       (settings.has_eprofile() && settings.eprofile() == NvtCodecVencSettings_Profile_PROFILE_UNKNOWN)) return false;

    if(settings.eformat() == NvtCodecVencSettings_Format_FORMAT_H264 && !settings.has_eprofile()) {
        return false;
    }

    if(settings.targetheight() < 144 || settings.targetwidth() < 128) return false;
    return true;
}


void NVTSYS::VideoEncoder::convertCBRSettingsFromPB(CodecVencSetting& nvtsettings, const NvtCodecVencSettings &settings) {
    CodecVencCBR& brcSettings = nvtsettings.sVencCBRConfig;
    const NvtCodecVencSettings::VencCBR& cbrSettings = settings.cbrsettings();
    brcSettings.uiEnable = 1;
    brcSettings.uiStaticTime = cbrSettings.statictime();
    brcSettings.uiByteRate = cbrSettings.byterate();
    brcSettings.uiFrameRate = cbrSettings.framerate();
    brcSettings.uiGOP = cbrSettings.gop();

    brcSettings.uiInitIQp = cbrSettings.iqp().init();
    brcSettings.uiMinIQp = cbrSettings.iqp().min();
    brcSettings.uiMaxIQp = cbrSettings.iqp().max();

    brcSettings.uiInitPQp = cbrSettings.pqp().init();
    brcSettings.uiMinPQp = cbrSettings.pqp().min();
    brcSettings.uiMaxPQp = cbrSettings.pqp().max();

    brcSettings.iIPWeight = cbrSettings.ipweight();
    if(cbrSettings.has_rowrcsettings()) {
        brcSettings.uiRowRcEnalbe = 1;
        brcSettings.uiRowRcQpRange = cbrSettings.rowrcsettings().qprange();
        brcSettings.uiRowRcQpStep = cbrSettings.rowrcsettings().qpstep();
    }
}

void NVTSYS::VideoEncoder::convertVBRSettingsFromPB(CodecVencSetting &nvtsettings, const NvtCodecVencSettings &settings) {
    CodecVencVBR& brcSettings = nvtsettings.sVencVBRConfig;
    const NvtCodecVencSettings::VencCBR& cbrSettings = settings.vbrsettings().cbrsettings();
    brcSettings.uiEnable = 1;
    brcSettings.uiStaticTime = cbrSettings.statictime();
    brcSettings.uiByteRate = cbrSettings.byterate();
    brcSettings.uiFrameRate = cbrSettings.framerate();
    brcSettings.uiGOP = cbrSettings.gop();

    brcSettings.uiInitIQp = cbrSettings.iqp().init();
    brcSettings.uiMinIQp = cbrSettings.iqp().min();
    brcSettings.uiMaxIQp = cbrSettings.iqp().max();

    brcSettings.uiInitPQp = cbrSettings.pqp().init();
    brcSettings.uiMinPQp = cbrSettings.pqp().min();
    brcSettings.uiMaxPQp = cbrSettings.pqp().max();

    brcSettings.iIPWeight = cbrSettings.ipweight();
    if(cbrSettings.has_rowrcsettings()) {
        brcSettings.uiRowRcEnalbe = 1;
        brcSettings.uiRowRcQpRange = cbrSettings.rowrcsettings().qprange();
        brcSettings.uiRowRcQpStep = cbrSettings.rowrcsettings().qpstep();
    }

    brcSettings.uiChangePos = settings.vbrsettings().changepos();
}

void NVTSYS::VideoEncoder::convertEVBRSettingsFromPB(CodecVencSetting &nvtsettings, const NvtCodecVencSettings &settings) {
    CodecVencEVBR & brcSettings = nvtsettings.sVencEVBRConfig;
    const NvtCodecVencSettings::VencCBR& cbrSettings = settings.evbrsettings().cbrsettings();
    brcSettings.uiEnable = 1;
    brcSettings.uiStaticTime = cbrSettings.statictime();
    brcSettings.uiByteRate = cbrSettings.byterate();
    brcSettings.uiFrameRate = cbrSettings.framerate();
    brcSettings.uiGOP = cbrSettings.gop();

    brcSettings.uiInitIQp = cbrSettings.iqp().init();
    brcSettings.uiMinIQp = cbrSettings.iqp().min();
    brcSettings.uiMaxIQp = cbrSettings.iqp().max();

    brcSettings.uiInitPQp = cbrSettings.pqp().init();
    brcSettings.uiMinPQp = cbrSettings.pqp().min();
    brcSettings.uiMaxPQp = cbrSettings.pqp().max();

    brcSettings.iIPWeight = cbrSettings.ipweight();
    if(cbrSettings.has_rowrcsettings()) {
        brcSettings.uiRowRcEnalbe = 1;
        brcSettings.uiRowRcQpRange = cbrSettings.rowrcsettings().qprange();
        brcSettings.uiRowRcQpStep = cbrSettings.rowrcsettings().qpstep();
    }

    brcSettings.uiKeyPPeriod = settings.evbrsettings().keypperiod();
    brcSettings.iKeyPWeight = settings.evbrsettings().keypweight();
    brcSettings.iMotionAQStrength = settings.evbrsettings().motionaqstrength();
    brcSettings.uiStillFrameCnd = settings.evbrsettings().stillframecnt();
    brcSettings.uiMotionRatioThd = settings.evbrsettings().motionratiothd();
    brcSettings.uiIPsnrCnd = settings.evbrsettings().ipsnrcnd();
    brcSettings.uiPPsnrCnd = settings.evbrsettings().ppsnrcnd();
    brcSettings.uiKeyPPsnrCnd = settings.evbrsettings().keyppsnrcnd();
}

void NVTSYS::VideoEncoder::convertFixQPSettingsFromPB(CodecVencSetting &nvtsettings, const NvtCodecVencSettings &settings) {
    CodecVencFIXQP& brcSettings = nvtsettings.sVencFixQPConfig;

    brcSettings.uiEnable = 1;
    brcSettings.uiByteRate = settings.fixqpsettings().byterate();
    brcSettings.uiFrameRate = settings.fixqpsettings().framerate();
    brcSettings.uiGOP = settings.fixqpsettings().gop();

    brcSettings.uiIFixQP = settings.fixqpsettings().ifixqp();
    brcSettings.uiPFixQP = settings.fixqpsettings().pfixqp();
}

void NVTSYS::VideoEncoder::convertNoneBRCSettingsFromPB(CodecVencSetting &nvtsettings, const NvtCodecVencSettings &settings) {
    CodecVencBRCNoneInfo& brcSettings = nvtsettings.sVencBRCNoneInfo;

    brcSettings.uiGOP = settings.nonebrcsettings().gop();
    brcSettings.uiFrameRate = settings.nonebrcsettings().framerate();
    brcSettings.uiByteRate = settings.nonebrcsettings().byterate();
}

void NVTSYS::VideoEncoder::globalInit() {
    int res = mglue_init();
    if(res == -1) throw std::runtime_error("Nvt lib init failed!");
    NvtCodec_Video_Init();
    isNvtLibInit = true;
    DBG_INFO("subsystem init!");
}

void NVTSYS::VideoEncoder::getRawCodecSettings(CodecVencSetting &settings) {
    ACCESS_GUARD_ACQUIRE;
    unlockedGetRawCodecSettings(settings);

}
bool NVTSYS::VideoEncoder::setRawCodecSettings(CodecVencSetting &settings) {
    ACCESS_GUARD_ACQUIRE;
    unlockedSetRawCodecSettings(settings);
    CodecVencSetting actualSettings;
    unlockedGetRawCodecSettings(actualSettings);

    return memcmp(&actualSettings,&settings,sizeof(CodecVencSetting)) == 0;
}

//Sometimes will corrupt registers
void NVTSYS::VideoEncoder::unlockedGetRawCodecSettings(CodecVencSetting &settings) {
    uint8_t vencSettingDataBuf[0x150];
    int res = NvtCodec_GetVencSetting(chnum, reinterpret_cast<CodecVencSetting *>(vencSettingDataBuf));
    if (res < 0) {
        DBG_ERROR("NvtCodec_GetVencSetting returned error code" << res);
        throw VencException(string("NvtCodec_SetVencSetting returned error code ") + to_string(res));
    }
    memcpy(&settings,vencSettingDataBuf,sizeof(CodecVencSetting));
}
void NVTSYS::VideoEncoder::unlockedSetRawCodecSettings(CodecVencSetting &settings) {
    if(isRunning) {
        throw VencException("Can't set settings While venc is running!");
    }
    int res = NvtCodec_SetVencSetting(chnum, &settings);
    if (res < 0) {
        DBG_ERROR("NvtCodec_SetVencSetting returned error code" << res);
        throw VencException(string("NvtCodec_SetVencSetting returned error code ") + to_string(res));
    }
    DBG_INFO("Venc settings changed");
}

bool NVTSYS::VideoEncoder::requestStreamData() {
    if(!isRunning || !naluQueue.empty()) return false;
    ACCESS_GUARD_ACQUIRE;
    if(isFirstFrameRequested) {
        DBG_INFO("Requesting First frame to be I-frame");
        int res = NvtCodec_RequestVencIFrame(chnum);
        if(res == -1) {
            DBG_ERROR("NvtCodec_RequestVencIFrame returned error code" << res);
            throw VencException(std::string("NvtCodec_RequestVencIFrame returned error code ") + std::to_string(res));
        }
        isFirstFrameRequested = false;
    }
    int res = NvtCodec_GetVencStream(chnum, &currentVencStream, 500, TRUE);
    if(res < 0) {
        DBG_ERROR("NvtCodec_GetVencStream returned error code" << res);
        throw VencException(std::string("NvtCodec_GetVencStream returned error code ") + std::to_string(res));
    }
    nvtStreamMutex.lock();
    switch(currentVencStream.uiFrameType) {
        case 0:
            lastFrameType = FTYPE_P;
            break;
        case 3:
            lastFrameType = FTYPE_IDR;
            break;
        default:
            lastFrameType = FTYPE_UNKNOWN;
            break;
    }
    for(int i = 0; i < currentVencStream.uiPackNum; i++) {
        vencNALU nalu{
            .timestamp = currentVencStream.sTimeStamp,
            .nalu_size = currentVencStream.sVencPack[i].uiDataLength,
            .nalu_data = currentVencStream.sVencPack[i].puiStreamPayloadData
        };
        naluQueue.push(nalu);

    }
    return true;
}

bool NVTSYS::VideoEncoder::isNALUAvail() {
    return !naluQueue.empty();
}

NVTSYS::vencNALU NVTSYS::VideoEncoder::getNALU() {
    return naluQueue.front();
}

bool NVTSYS::VideoEncoder::popNALU() {
    if(naluQueue.empty()) return false;
    naluQueue.pop();
    if(naluQueue.empty()) {
        NvtCodec_ReleaseVencStream(chnum, &currentVencStream);
        nvtStreamMutex.unlock();
    }
    return true;
}

void NVTSYS::VideoEncoder::setDefaultSettings() {
    /*
    ACCESS_GUARD_ACQUIRE;
    if(isRunning) {
        throw VencException("Can't set settings While venc is running!");
    }


    int res = NvtCodec_GetVencSetting(chnum,&codec_settings);
    if(res == -1) {
        DBG_ERROR("NvtCodec_GetVencSetting returned error code" << res);
        throw VencException(std::string("NvtCodec_GetVencSetting returned error code ") + std::to_string(res));
    }
     */
    CodecVencSetting codec_settings;
    //NvtCodec_GetVencSetting(chnum,&codec_settings);
    memset(&codec_settings, 0, sizeof(codec_settings));
    codec_settings.eVencInputSource = CODEC_VENC_INPUT_SOURCE_SENSOR1;
    codec_settings.eVencFormat = CODEC_VENC_FORMAT_H264;
    codec_settings.uiTargetWidth = 2560;
    codec_settings.uiTargetHeight = 1440;
    codec_settings.eBRCType = CODEC_VENC_BRC_VBR;

    codec_settings.sVencVBRConfig.uiEnable = 1;
    codec_settings.sVencVBRConfig.uiStaticTime = 0;
    codec_settings.sVencVBRConfig.uiFrameRate = 12;
    codec_settings.sVencVBRConfig.uiByteRate = 524000;
    codec_settings.sVencVBRConfig.uiGOP = 36;
    codec_settings.sVencVBRConfig.uiInitIQp = 7;
    codec_settings.sVencVBRConfig.uiInitPQp = 7;
    codec_settings.sVencVBRConfig.uiMinIQp = 15;
    codec_settings.sVencVBRConfig.uiMaxIQp = 1;
    codec_settings.sVencVBRConfig.uiMinPQp = 15;
    codec_settings.sVencVBRConfig.uiMaxPQp = 1;
    codec_settings.sVencVBRConfig.iIPWeight = 0x20;
    codec_settings.sVencVBRConfig.uiRowRcEnalbe = 0;
    codec_settings.sVencVBRConfig.uiRowRcQpRange = 1;
    codec_settings.sVencVBRConfig.uiRowRcQpStep = 4;
    codec_settings.sVencVBRConfig.uiChangePos = 15;
    codec_settings.eVencProfile = CODEC_VENC_PROFILE_MAIN;

    if(setRawCodecSettings(codec_settings)) {
        DBG_INFO("Loaded default venc settings");
    } else {
        DBG_WARN("Default settings did not apply!");
    }

    //NvtCodecVencSettings pbSettings;
    //getCodecSettingsInPB(pbSettings);
    //std::cout << pbSettings.DebugString() << std::endl;

}

NVTSYS::VideoEncoder::~VideoEncoder() {
    if(isRunning) stop();
}

void NVTSYS::VideoEncoder::convertVBRSettingsToPB(const CodecVencSetting &nvtsettings, NvtCodecVencSettings &settings) {
    auto* pb_cbr = settings.mutable_vbrsettings()->mutable_cbrsettings();
    const CodecVencVBR& brc_settings = nvtsettings.sVencVBRConfig;
    pb_cbr->set_framerate(brc_settings.uiFrameRate);
    pb_cbr->set_statictime(brc_settings.uiStaticTime);
    pb_cbr->set_byterate(brc_settings.uiByteRate);
    pb_cbr->set_gop(brc_settings.uiGOP);

    pb_cbr->mutable_iqp()->set_init(brc_settings.uiInitIQp);
    pb_cbr->mutable_iqp()->set_min(brc_settings.uiMinIQp);
    pb_cbr->mutable_iqp()->set_max(brc_settings.uiMaxIQp);

    pb_cbr->mutable_pqp()->set_init(brc_settings.uiInitPQp);
    pb_cbr->mutable_pqp()->set_min(brc_settings.uiMinPQp);
    pb_cbr->mutable_pqp()->set_max(brc_settings.uiMaxPQp);

    pb_cbr->set_ipweight(brc_settings.iIPWeight);

    if(brc_settings.uiRowRcEnalbe) {
        pb_cbr->mutable_rowrcsettings()->set_qpstep(brc_settings.uiRowRcQpStep);
        pb_cbr->mutable_rowrcsettings()->set_qpstep(brc_settings.uiRowRcQpRange);
    }

    settings.mutable_vbrsettings()->set_changepos(brc_settings.uiChangePos);
}

void NVTSYS::VideoEncoder::convertCBRSettingsToPB(const CodecVencSetting &nvtsettings, NvtCodecVencSettings &settings) {
    auto* pb_cbr = settings.mutable_cbrsettings();
    const CodecVencCBR & brc_settings = nvtsettings.sVencCBRConfig;
    pb_cbr->set_framerate(brc_settings.uiFrameRate);
    pb_cbr->set_statictime(brc_settings.uiStaticTime);
    pb_cbr->set_byterate(brc_settings.uiByteRate);
    pb_cbr->set_gop(brc_settings.uiGOP);

    pb_cbr->mutable_iqp()->set_init(brc_settings.uiInitIQp);
    pb_cbr->mutable_iqp()->set_min(brc_settings.uiMinIQp);
    pb_cbr->mutable_iqp()->set_max(brc_settings.uiMaxIQp);

    pb_cbr->mutable_pqp()->set_init(brc_settings.uiInitPQp);
    pb_cbr->mutable_pqp()->set_min(brc_settings.uiMinPQp);
    pb_cbr->mutable_pqp()->set_max(brc_settings.uiMaxPQp);

    pb_cbr->set_ipweight(brc_settings.iIPWeight);

    if(brc_settings.uiRowRcEnalbe) {
        pb_cbr->mutable_rowrcsettings()->set_qpstep(brc_settings.uiRowRcQpStep);
        pb_cbr->mutable_rowrcsettings()->set_qpstep(brc_settings.uiRowRcQpRange);
    }
}

void NVTSYS::VideoEncoder::convertEVBRSettingsToPB(const CodecVencSetting &nvtsettings, NvtCodecVencSettings &settings) {
    auto* pb_cbr = settings.mutable_evbrsettings()->mutable_cbrsettings();
    auto* pb_evbr = settings.mutable_evbrsettings();
    const CodecVencEVBR& brcSettings = nvtsettings.sVencEVBRConfig;
    pb_cbr->set_framerate(brcSettings.uiFrameRate);
    pb_cbr->set_statictime(brcSettings.uiStaticTime);
    pb_cbr->set_byterate(brcSettings.uiByteRate);
    pb_cbr->set_gop(brcSettings.uiGOP);

    pb_cbr->mutable_iqp()->set_init(brcSettings.uiInitIQp);
    pb_cbr->mutable_iqp()->set_min(brcSettings.uiMinIQp);
    pb_cbr->mutable_iqp()->set_max(brcSettings.uiMaxIQp);

    pb_cbr->mutable_pqp()->set_init(brcSettings.uiInitPQp);
    pb_cbr->mutable_pqp()->set_min(brcSettings.uiMinPQp);
    pb_cbr->mutable_pqp()->set_max(brcSettings.uiMaxPQp);

    pb_cbr->set_ipweight(brcSettings.iIPWeight);

    if(brcSettings.uiRowRcEnalbe) {
        pb_cbr->mutable_rowrcsettings()->set_qpstep(brcSettings.uiRowRcQpStep);
        pb_cbr->mutable_rowrcsettings()->set_qpstep(brcSettings.uiRowRcQpRange);
    }

    pb_evbr->set_keypperiod(brcSettings.uiKeyPPeriod);
    pb_evbr->set_keypweight(brcSettings.iKeyPWeight);
    pb_evbr->set_motionaqstrength(brcSettings.iMotionAQStrength);
    pb_evbr->set_stillframecnt(brcSettings.uiStillFrameCnd);
    pb_evbr->set_motionratiothd(brcSettings.uiMotionRatioThd);
    pb_evbr->set_ipsnrcnd(brcSettings.uiIPsnrCnd);
    pb_evbr->set_ppsnrcnd(brcSettings.uiPPsnrCnd);
    pb_evbr->set_keyppsnrcnd(brcSettings.uiKeyPPsnrCnd);


}

void NVTSYS::VideoEncoder::convertFixQPSettingsToPB(const CodecVencSetting &nvtsettings, NvtCodecVencSettings &settings) {
    const CodecVencFIXQP& brcSettings = nvtsettings.sVencFixQPConfig;
    auto* pb_fixqp = settings.mutable_fixqpsettings();

    pb_fixqp->set_framerate(brcSettings.uiFrameRate);
    pb_fixqp->set_byterate(brcSettings.uiByteRate);
    pb_fixqp->set_gop(brcSettings.uiGOP);
    pb_fixqp->set_ifixqp(brcSettings.uiIFixQP);
    pb_fixqp->set_pfixqp(brcSettings.uiPFixQP);
}

void NVTSYS::VideoEncoder::convertNoneBRCSettingsToPB(const CodecVencSetting &nvtsettings, NvtCodecVencSettings &settings) {
    settings.mutable_nonebrcsettings()->set_framerate(nvtsettings.sVencBRCNoneInfo.uiFrameRate);
    settings.mutable_nonebrcsettings()->set_byterate(nvtsettings.sVencBRCNoneInfo.uiByteRate);
    settings.mutable_nonebrcsettings()->set_gop(nvtsettings.sVencBRCNoneInfo.uiGOP);

}


const char *NVTSYS::VideoEncoder::VencException::what() const noexcept {
    return reason.c_str();
}

NVTSYS::VideoEncoder::VencException::VencException(const char *reason) : reason(reason) {}

NVTSYS::VideoEncoder::VencException::VencException(string &&reason) : reason(reason){}
