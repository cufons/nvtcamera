//
// Created by cufon on 18.02.24.
//

#include <fcntl.h>
#include <csignal>
#include <cstring>
#include "ISP.h"
#include "debug_info.h"

const std::string NVTSYS::ISP::cfgPresetDayPath = "/tmp/isp510_day.cfg";
const std::string NVTSYS::ISP::cfgPresetNightIRPath = "/tmp/isp510_night.cfg";
const std::string NVTSYS::ISP::cfgPresetNightPath = "/tmp/isp510_white_light_night.cfg";
NVTSYS::ISP* NVTSYS::ISP::instance = nullptr;
NVTSYS::ISP *NVTSYS::ISP::getInstance() {
    if(!instance) {
        throw std::runtime_error("ISP is not initialized or not supported!");
    }
    return instance;
}

void NVTSYS::ISP::init() {
    instance = new NVTSYS::ISP;
    DBG_INFO("subsystem init!");
}

NVTSYS::ISP::ISP()  {
    cmdfd = open("/proc/isp510/command", O_RDWR);
    if(cmdfd == -1) {
        throw std::system_error(errno,std::system_category(),"cmd file open failed");
    }

    devfd = open("/dev/isp510", O_RDWR);
    if(devfd == -1) {
        throw std::system_error(errno,std::system_category(),"dev file open failed");
    }
}

bool NVTSYS::ISP::loadPresetCfg(const std::string& path) {
    std::string cmd = "r rld_cfg " + path + "\n";
    return sendRawCmd(cmd);
}


NVTSYS::ISP::~ISP() {
    close(cmdfd);
    close(devfd);
}

bool NVTSYS::ISP::sendRawCmd(std::string &cmd) {
    int res = write(cmdfd,cmd.c_str(),cmd.size());
    return res > 0;
}

bool NVTSYS::ISP::sendRawCmd(const char *cmd) {
    int res = write(cmdfd,cmd,strlen(cmd));
    return res > 0;
}
bool NVTSYS::ISP::sendRawCmd(std::string &&cmd) {
    int res = write(cmdfd,cmd.c_str(),cmd.size());
    return res > 0;
}

bool NVTSYS::ISP::readCmdResp(std::string &resp) {
    char c;
    resp.clear();
    while(true) {
        int res = read(cmdfd,&c,1);
        if(res == -1) return false;
        if(!res) break;
        resp.push_back(c);
    }
    return true;
}



bool NVTSYS::ISP::fetchCmdResp(std::string &cmd, std::string &resp) {
    return sendRawCmd(cmd) && readCmdResp(resp);
}

bool NVTSYS::ISP::fetchCmdResp(const char *cmd, std::string &resp) {
    return sendRawCmd(cmd) && readCmdResp(resp);
}
void NVTSYS::ISP::destroy() {
    delete instance;
}
int NVTSYS::ISP::getBrightness() {
    std::string resp;
    if(!fetchCmdResp("r brightness\n", resp)) return -1;
    return std::stoi(resp);
}
bool NVTSYS::ISP::setBrightness(int val) {
    //if(val < 0 || val > 200) throw std::out_of_range("set value out of range");
    return sendRawCmd("w brightness " + std::to_string(val) + "\n");
}

int NVTSYS::ISP::getSaturation() {
    std::string resp;
    if(!fetchCmdResp("r saturation\n", resp)) return -1;
    return std::stoi(resp);
}

bool NVTSYS::ISP::setSaturation(int val) {
    //if(val < 0 || val > 200) throw std::out_of_range("set value out of range");
    return sendRawCmd("w saturation " + std::to_string(val) + "\n");
}



int NVTSYS::ISP::getHue() {
    std::string resp;
    if(!fetchCmdResp("r hue\n", resp)) return -1;
    return std::stoi(resp);
}

bool NVTSYS::ISP::setHue(int val) {
    //if(val < 0 || val > 360) throw std::out_of_range("set value out of range");
    return sendRawCmd("w hue " + std::to_string(val) + "\n");
}

int NVTSYS::ISP::getContrast() {
    std::string resp;
    if(!fetchCmdResp("r contrast\n", resp)) return -1;
    return std::stoi(resp);
}

bool NVTSYS::ISP::setContrast(int val) {
    //if(val < 0 || val > 200) throw std::out_of_range("set value out of range");
    return sendRawCmd("w contrast " + std::to_string(val) + "\n");
}

int NVTSYS::ISP::getNRLevel() {
    std::string resp;
    if(!fetchCmdResp("r denoise\n", resp)) return -1;
    return std::stoi(resp);
}

bool NVTSYS::ISP::setNRLevel(int val) {
    //if(val < 0 || val > 200) throw std::out_of_range("set value out of range");
    return sendRawCmd("w denoise " + std::to_string(val) + "\n");
}

int NVTSYS::ISP::getEELevel() {
    std::string resp;
    if(!fetchCmdResp("r eenh\n", resp)) return -1;
    return std::stoi(resp);
}

bool NVTSYS::ISP::setEELevel(int val) {
    //if(val < 0 || val > 200) throw std::out_of_range("set value out of range");
    return sendRawCmd("w eenh " + std::to_string(val) + "\n");
}

int NVTSYS::ISP::getTNR() {
    std::string resp;
    if(!fetchCmdResp("r tnr\n", resp)) return -1;
    return std::stoi(resp);
}

bool NVTSYS::ISP::setTNR(int val) {
    //if(val < 0 || val > 10) throw std::out_of_range("set value out of range");
    return sendRawCmd("w tnr " + std::to_string(val) + "\n");
}

bool NVTSYS::ISP::getNightFilterEnable() {
    std::string resp;
    if(!fetchCmdResp("r daynight\n", resp)) return -1;
    return resp.find("NIGHT") != -1;
}

bool NVTSYS::ISP::setNightFilterEnable(bool enable) {
    return sendRawCmd("w daynight " + std::to_string(enable ? 1 : 0) + "\n");
}
