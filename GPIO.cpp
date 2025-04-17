//
// Created by cufon on 14.03.24.
//

#include "GPIO.h"
#include "debug_info.h"

NVTSYS::GPIO* NVTSYS::GPIO::instance = nullptr;
#define IOC_STRTAB_BASE 0x66
static const std::string gpioIocStrTab[4] = {
        "IOC_GPIO_SET",
        "IOC_GPIO_GET",
        "IOC_GPIO_DIR_IN",
        "IOC_GPIO_DIR_OUT"
};
NVTSYS::GPIO::GPIO() {
    fd = open("/dev/mgpio",0,0);
    if(fd == -1) {
        throw std::system_error(errno,std::generic_category(),"gpio handle open failed");
    }
}

void NVTSYS::GPIO::init() {
    instance = new GPIO();
    DBG_INFO("subsystem init!");
}

NVTSYS::GPIO::~GPIO() {
    close(fd);
}

void NVTSYS::GPIO::pinMode(unsigned int pin, GPIO::GPIOMode mode) {
    drvIoctl(mode,pin,0);
}

void NVTSYS::GPIO::pinWrite(unsigned int pin, GPIO::GPIOState state) {
    drvIoctl(IOC_GPIO_SET,pin,state);
}

NVTSYS::GPIO::GPIOState NVTSYS::GPIO::pinRead(unsigned int pin) {
    return drvIoctl(IOC_GPIO_SET,pin,0) == 1 ? GPIO_STATE_HIGH : GPIO_STATE_LOW;
}

unsigned NVTSYS::GPIO::drvIoctl(unsigned int request, unsigned int arg1, unsigned int arg2) {
    unsigned argArr[] = {arg1,arg2};
    int res = ioctl(fd,request,argArr);

    if(res == -1) {
        throw std::system_error(errno,std::system_category(), gpioIocStrTab[request - IOC_STRTAB_BASE] + " ioctl failed");
    }
    return argArr[1];
}

NVTSYS::GPIO *NVTSYS::GPIO::getInstance() {
    if(!instance) {
        throw std::runtime_error("GPIO subsystem is not initialized or not supported!");
    }
    return instance;
}

bool NVTSYS::GPIO::isInit() {
    return instance;
}
