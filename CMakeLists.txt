cmake_minimum_required(VERSION 3.25)
project(nvtcamera)

set (MIPS_LIB_PATH /usr/mipsel-unknown-linux-gnu/sysroot/lib)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS -rdynamic)
#set(CMAKE_CXX_FLAGS -Wl,-dynamic-linker,./interpreter_alt.so)
include_directories(./protocom/protocom)

find_library(MGLUE_LIB NAMES mipcglue PATHS ${MIPS_LIB_PATH})
find_library(MIPC_LIB NAMES mipc PATHS ${MIPS_LIB_PATH})
find_library(PROTOCOM_LIB NAMES protocom PATHS /home/cufon/CLionProjects/protocom/cmake-build-debug-mipsel-glibc)
set(CMAKE_CXX_FLAGS -Wl,--copy-dt-needed-entries,-dynamic-linker,./interpreter_alt.so)
link_libraries(${MGLUE_LIB} ${MIPC_LIB} ${PROTOCOM_LIB} cryptopp protobuf liveMedia    BasicUsageEnvironment UsageEnvironment groupsock ssl crypto ctypewrapper)
add_library(cameractlproto STATIC cameractl.pb.cc)
add_executable(nvtcamera VideoEncoder.cpp VideoEncoder.h debug_info.h CameraCtlHandler.cpp CameraCtlHandler.h cameractl.pb.cc main.cpp VencLiveSourceH264.cxx H264LiveServerMediaSession.cxx ctypewrapper.c ISP.cpp ISP.h CameraCtlHandlerFactory.cpp CameraCtlHandlerFactory.h GPIO.h GPIO.cpp StepperMotor.h StepperMotor.cpp INIParser.cpp INIParser.h ADC.cpp ADC.h ModeSwitchController.cpp ModeSwitchController.h)