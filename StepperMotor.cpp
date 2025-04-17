//
// Created by cufon on 23.03.24.
//

#include "StepperMotor.h"
#include "debug_info.h"

namespace NVTSYS {
    const StepperMotor::PhaseArrDef StepperMotor::phaseMasks{9, 3, 6, 12};
    const unsigned StepperMotor::stepDelayUsecs = 2000;
    StepperMotor* StepperMotor::panMotorInstance = nullptr;
    StepperMotor* StepperMotor::tiltMotorInstance = nullptr;
    StepperMotor::StepperMotor(GPIO &gpio, PinArrDef&& pins) : gpio(gpio), pins(pins), currentPhase(phaseMasks.begin()) {
        for(auto pin : pins) {
            gpio.pinMode(pin,GPIO::GPIO_MODE_OUT);
        }
    }

    void StepperMotor::doStep(bool isReverseStep) {
        for(int i = 0; i < pins.size(); i++) {
            char pinState = (*currentPhase >> i) & 1;
            gpio.pinWrite(pins[i],pinState ? GPIO::GPIO_STATE_HIGH : GPIO::GPIO_STATE_LOW);
        }

        if(isReverseStep) {
            if(currentPhase == phaseMasks.begin()) {
                currentPhase = phaseMasks.end() - 1;
            } else {
                currentPhase--;
            }
        } else {
            currentPhase++;
            if(currentPhase == phaseMasks.end()) currentPhase = phaseMasks.begin();
        }
    }

    void StepperMotor::advance(unsigned int n) {
        for(int i = 0; i < n; i++) {
            doStep();
            usleep(stepDelayUsecs);
        }
        doStop();
    }

    void StepperMotor::reverse(unsigned int n) {
        for(int i = 0; i < n; i++) {
            doStep(true);
            usleep(stepDelayUsecs);
        }
        doStop();
    }

    StepperMotor *StepperMotor::getPanMotorInstance() {
        if(!panMotorInstance) {
            throw std::runtime_error("Stepper motor subsystem is not initialized or not supported!");
        }
        return panMotorInstance;
    }

    StepperMotor *StepperMotor::getTiltMotorInstance() {
        if(!tiltMotorInstance) {
            throw std::runtime_error("Stepper motor subsystem is not initialized or not supported!");
        }
        return tiltMotorInstance;
    }

    void StepperMotor::globalInit() {
        //TODO add pin loading from config files
        panMotorInstance = new StepperMotor(*GPIO::getInstance(),{32,33,34,35});
        tiltMotorInstance = new StepperMotor(*GPIO::getInstance(),{36,37,38,39});
        DBG_INFO("subsystem init!");
    }

    void StepperMotor::doStop() {
        for(auto p : pins) {
            gpio.pinWrite(p,GPIO::GPIO_STATE_LOW);
        }
    }
} // NVTSYS