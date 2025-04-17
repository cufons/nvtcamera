//
// Created by cufon on 14.03.24.
//

#ifndef NVTCAMERA_GPIO_H
namespace NVTSYS {
#define NVTCAMERA_GPIO_H
#define IOC_GPIO_SET 0x66
#define IOC_GPIO_GET 0x65
#define IOC_GPIO_DIR_IN 0x67
#define IOC_GPIO_DIR_OUT 0x68
#define GPIO_PIN_IR_LIGHT 44

    class GPIO {
        bool isOpen;
        int fd;
        static GPIO* instance;
    public:
        static GPIO *getInstance();

    private:
        unsigned drvIoctl(unsigned request,unsigned arg1,unsigned arg2);
    public:
        enum GPIOMode {
            GPIO_MODE_IN = 0x67,
            GPIO_MODE_OUT = 0x68
        };
        enum GPIOState {
            GPIO_STATE_LOW = 0,
            GPIO_STATE_HIGH = 1
        };
        GPIO();
        static void init();
        static bool isInit();
        void pinMode(unsigned pin,GPIOMode mode);
        void pinWrite(unsigned pin, GPIOState state);
        GPIOState pinRead(unsigned pin);
        virtual ~GPIO();
    };
}

#include "unistd.h"
#include "fcntl.h"
#include "sys/ioctl.h"
#include <cerrno>
#include <exception>
#include <iostream>


#endif //NVTCAMERA_GPIO_H
