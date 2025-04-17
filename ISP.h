//
// Created by cufon on 18.02.24.
//

#ifndef NVTCAMERA_ISP_H
#define NVTCAMERA_ISP_H


#include <string>
#include <fstream>
#include <iostream>
namespace NVTSYS {
    class ISP {
        static ISP* instance;
        int cmdfd;
        int devfd;
        bool sendRawCmd(std::string& cmd);
        bool sendRawCmd(std::string&& cmd);
        bool sendRawCmd(const char* cmd);
        bool fetchCmdResp(std::string& cmd, std::string& resp);
        bool fetchCmdResp(const char* cmd, std::string &resp);
        bool readCmdResp(std::string& resp);
    public:
        ISP();
        ~ISP();
        static const std::string cfgPresetDayPath;
        static const std::string cfgPresetNightPath;
        static const std::string cfgPresetNightIRPath;
        static ISP *getInstance();
        static void init();
        static void destroy();

        bool loadPresetCfg(const std::string& path);

        int getBrightness();
        bool setBrightness(int val);
        int getSaturation();
        bool setSaturation(int val);
        int getHue();
        bool setHue(int val);
        int getContrast();
        bool setContrast(int val);
        int getNRLevel();
        bool setNRLevel(int val);
        int getEELevel();
        bool setEELevel(int val);
        int getTNR();
        bool setTNR(int val);
        bool getNightFilterEnable();
        bool setNightFilterEnable(bool enable);
    public:
    };
}


#endif //NVTCAMERA_ISP_H
