//
// Created by cufon on 14.10.2024.
//

#include "ModeSwitchController.h"
#include "debug_info.h"

ModeSwitchController::ModeSwitchController() {
    irEnabled = false;
    isManualControl = false;
    currentMode = MODE_NIGHT;
    adc = NVTSYS::ADC::getInstance();
    isp = NVTSYS::ISP::getInstance();
    gpio = NVTSYS::GPIO::getInstance();
    adcDayNightThresh = 1200;
    adcWindow = 500;
    adcDayThresh = adcDayNightThresh + adcWindow;
    adcNightThresh = adcDayNightThresh - adcWindow;
    isAutoControlRunning = false;
    controlThreadHandle = nullptr;
}

void ModeSwitchController::applyMode() {

    switch (currentMode) {
        case MODE_DAY:
            isp->loadPresetCfg(NVTSYS::ISP::cfgPresetDayPath);
            isp->setNightFilterEnable(false);
            setIRLight(false);
            break;
        case MODE_NIGHT:
            if(irEnabled) {
                isp->loadPresetCfg(NVTSYS::ISP::cfgPresetNightIRPath);
                isp->setNightFilterEnable(true);
                setIRLight(true);
            } else {
                isp->loadPresetCfg(NVTSYS::ISP::cfgPresetNightPath);
                isp->setBrightness(130);
                isp->setNightFilterEnable(false);
                setIRLight(false);
            }
    }
}

void ModeSwitchController::setIRLight(bool enabled) {
    gpio->pinWrite(GPIO_PIN_IR_LIGHT,enabled ? NVTSYS::GPIO::GPIO_STATE_HIGH : NVTSYS::GPIO::GPIO_STATE_LOW);
}

void ModeSwitchController::EnableAutoControl() {
    isManualControl = false;
}

void ModeSwitchController::SetDayMode() {
    std::lock_guard<std::mutex> lock(autoControlMutex);
    isManualControl = true;
    currentMode = MODE_DAY;
    applyMode();
}

void ModeSwitchController::SetNightMode() {
    std::lock_guard<std::mutex> lock(autoControlMutex);
    isManualControl = true;
    currentMode = MODE_NIGHT;
    applyMode();
}

void ModeSwitchController::setIrEnabled(bool irEnabled) {
    std::lock_guard<std::mutex> lock(autoControlMutex);
    ModeSwitchController::irEnabled = irEnabled;
    applyMode();
}

bool ModeSwitchController::isIrEnabled() const {
    return irEnabled;
}

ModeSwitchController::CamMode ModeSwitchController::getCurrentMode() const {
    return currentMode;
}

void ModeSwitchController::runAutoControl() {
    while (isAutoControlRunning) {
        sleep(adcCheckIntervalSec);
        std::lock_guard<std::mutex> lock(autoControlMutex);
        if(isManualControl) continue;
        int adcValue = adc->readChannel(adcChannelN);
        if(adcValue == -1) {
            DBG_ERROR("Unable to read ADC value");
            continue;
        }
        switch (currentMode) {
            case MODE_DAY:
                if(adcValue < adcNightThresh) {
                    DBG_INFO("Setting night mode (reported ADC value: " << adcValue << ")");
                    currentMode = MODE_NIGHT;
                    applyMode();
                }
                break;
            case MODE_NIGHT:
                if(adcValue > adcDayThresh) {
                    DBG_INFO("Setting day mode (reported ADC value: " << adcValue << ")");
                    currentMode = MODE_DAY;
                    applyMode();
                }
                break;
        }
    }
}

void ModeSwitchController::StartAutoControl() {
    DBG_INFO("ModeSwitchController thread starting...");
    isAutoControlRunning = true;
    applyMode();
    controlThreadHandle = new std::thread(&ModeSwitchController::runAutoControl, this);
}

ModeSwitchController::~ModeSwitchController() {
    isAutoControlRunning = false;
    delete controlThreadHandle;
}

bool ModeSwitchController::isAutoControlEnabled() {
    return !isManualControl;
}
