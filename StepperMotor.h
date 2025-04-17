//
// Created by cufon on 23.03.24.
//

#ifndef NVTCAMERA_STEPPERMOTOR_H
#define NVTCAMERA_STEPPERMOTOR_H

#include "GPIO.h"
#include <array>
namespace NVTSYS {

    class StepperMotor {
    public:
        using PinArrDef = std::array<unsigned,4>;
    private:
        static StepperMotor* panMotorInstance;
    public:
        static void globalInit();
        static StepperMotor *getPanMotorInstance();

        static StepperMotor *getTiltMotorInstance();

    private:
        static StepperMotor* tiltMotorInstance;
        using PhaseArrDef = std::array<char,4>;
        GPIO& gpio;
        static const PhaseArrDef phaseMasks;
        const PinArrDef pins;
        static const unsigned stepDelayUsecs;

        PhaseArrDef::const_iterator currentPhase;
        void doStep(bool isReverseStep = false);
        void doStop();
    public:
        StepperMotor(GPIO& gpio, PinArrDef&& pins);
        void advance(unsigned n);
        void reverse(unsigned n);
    };

} // NVTSYS

#endif //NVTCAMERA_STEPPERMOTOR_H
