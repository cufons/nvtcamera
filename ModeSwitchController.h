//
// Created by cufon on 14.10.2024.
//

#ifndef NVTCAMERA_MODESWITCHCONTROLLER_H
#define NVTCAMERA_MODESWITCHCONTROLLER_H

#include "ADC.h"
#include "ISP.h"
#include "GPIO.h"
#include <thread>
#include <atomic>
#include <mutex>
class ModeSwitchController {
public:
    enum CamMode {
        MODE_DAY,
        MODE_NIGHT
    } currentMode;
private:
    std::atomic_bool irEnabled;
    std::atomic_bool isManualControl;
    std::atomic_bool isAutoControlRunning;
    std::mutex autoControlMutex;
    std::thread* controlThreadHandle;
    NVTSYS::ADC* adc;
    NVTSYS::ISP* isp;
    NVTSYS::GPIO* gpio;
    unsigned adcDayNightThresh;
    unsigned adcWindow;
    unsigned adcDayThresh;
    unsigned adcNightThresh;

    const unsigned adcCheckIntervalSec = 2;
    const int adcChannelN = 1;

    void applyMode();
    void setIRLight(bool enabled);
    void runAutoControl();
public:
    ModeSwitchController();

    bool isIrEnabled() const;
    void setIrEnabled(bool irEnabled);
    CamMode getCurrentMode() const;
    bool isAutoControlEnabled();

    void StartAutoControl();
    void EnableAutoControl();

    virtual ~ModeSwitchController();

    void SetDayMode();
    void SetNightMode();


};


#endif //NVTCAMERA_MODESWITCHCONTROLLER_H
