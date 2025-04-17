//
// Created by cufon on 12.06.24.
//

#include "ADC.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include "debug_info.h"

namespace NVTSYS {
    ADC* ADC::instance = nullptr;
    ADC::ADC() {
        basePath = "/sys/bus/iio/devices/iio:device0/";
    }

    ADC *ADC::getInstance() {
        if(!instance) {
            throw std::runtime_error("ADC subsystem is not initialized or not supported!");
        }
        return instance;
    }

    void ADC::init() {
        instance = new ADC();
        if(!(instance->tryEnable() && instance->openChannels())) {
            delete instance;
            instance = nullptr;
            return;
        } else {
            DBG_INFO("subsystem init!");
        }

    }

    bool ADC::tryEnable() {
        std::string enableControlPath = basePath + "enable";
        int fd = open(enableControlPath.c_str(),O_WRONLY);
        if(fd == -1) {
            DBG_ERROR("enable control file open errno: " << errno);
            return false;
        }

        if(write(fd,"1",2) == -1) {
            DBG_ERROR("enable control file write errno: " << errno);
            return false;
        }

        close(fd);
        return true;
    }

    bool ADC::openChannels() {
        char fname_buf[20];
        for(int i = 0 ; i< channels.size();i++) {
            sprintf(fname_buf,"in_voltage%d_raw",i);
            std::string chanPath = basePath + fname_buf;
            channels[i].open(chanPath);
            if(!channels[i].is_open()) {
                DBG_ERROR("Channel " << i << " open errno:" << errno);
                return false;
            }
        }
        return true;
    }

    ADC::~ADC() {

    }

    int ADC::readChannel(unsigned int channel) {
        try {
            std::ifstream& fd = channels.at(channel);
            int ret;
            fd >> ret;
            fd.clear();
            fd.seekg(0,std::ifstream::beg);
            return ret;

        } catch (const std::out_of_range& e) {
            return -1;
        }
    }

    int ADC::getChanCount() {
        return channels.size();
    }
} // NVTSYS