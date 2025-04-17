//
// Created by cufon on 09.01.24.
//
extern "C" {
#include <mipc_glue.h>
#include <nvtcodec.h>
}
#include <iostream>
#include <mutex>
#include <queue>
#include "cameractl.pb.h"

#ifndef NVTCAMERA_NVTVENC_H
namespace NVTSYS {
#define NVTCAMERA_NVTVENC_H
    struct vencNALU {
        timeval timestamp;
        size_t nalu_size;
        uint8_t* nalu_data;
    };

    class VideoEncoder {
    public:
        enum eLastFrameType {
            FTYPE_UNKNOWN,
            FTYPE_IDR,
            FTYPE_P
        };
    private:
        unsigned chnum;
        bool isRunning;
        bool isVencHeld;
        CodecVencStream currentVencStream;
        std::queue<vencNALU> naluQueue;
        static std::mutex nvtLibMutex;
        std::mutex nvtStreamMutex;
        static bool isNvtLibInit;
        bool isFirstFrameRequested;
        eLastFrameType lastFrameType;
    #define ACCESS_GUARD_ACQUIRE std::lock_guard<std::mutex> lock(nvtLibMutex)
        static void convertCBRSettingsFromPB(CodecVencSetting& nvtsettings, const NvtCodecVencSettings &settings);
        static void convertVBRSettingsFromPB(CodecVencSetting& nvtsettings, const NvtCodecVencSettings &settings);
        static void convertEVBRSettingsFromPB(CodecVencSetting& nvtsettings, const NvtCodecVencSettings &settings);
        static void convertFixQPSettingsFromPB(CodecVencSetting& nvtsettings, const NvtCodecVencSettings &settings);
        static void convertNoneBRCSettingsFromPB(CodecVencSetting& nvtsettings, const NvtCodecVencSettings &settings);

        static void convertCBRSettingsToPB(const CodecVencSetting& nvtsettings, NvtCodecVencSettings &settings);
        static void convertVBRSettingsToPB(const CodecVencSetting& nvtsettings, NvtCodecVencSettings &settings);
        static void convertEVBRSettingsToPB(const CodecVencSetting& nvtsettings, NvtCodecVencSettings &settings);
        static void convertFixQPSettingsToPB(const CodecVencSetting& nvtsettings, NvtCodecVencSettings &settings);
        static void convertNoneBRCSettingsToPB(const CodecVencSetting& nvtsettings, NvtCodecVencSettings &settings);
        void unlockedStart();
        void unlockedStop();
        void unlockedSetRawCodecSettings(CodecVencSetting &settings);
        void unlockedGetRawCodecSettings(CodecVencSetting &settings);
    public:
        explicit VideoEncoder(unsigned int chnum);
        bool getRunning() const;
        void start();
        void stop();
        static bool validatePBCodecSettings(const NvtCodecVencSettings &settings);
        void setCodecSettingsFromPB(const NvtCodecVencSettings &settings);
        void getCodecSettingsInPB(NvtCodecVencSettings &settings);
        bool setRawCodecSettings(CodecVencSetting& settings);
        void setDefaultSettings();
        bool requestStreamData();
        eLastFrameType getLastFrameType();
        bool isNALUAvail();
        vencNALU getNALU();
        bool popNALU();
        NvtCodecVencSettings getCodecSettings();

        class VencException : public std::exception {
            std::string reason;
        public:
            explicit VencException(const char *reason);
            VencException(std::string&& reason);

            const char *what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW override;
        };

        static void globalInit();



        void getRawCodecSettings(CodecVencSetting &settings);

        virtual ~VideoEncoder();
    };
}


#endif //NVTCAMERA_NVTVENC_H
