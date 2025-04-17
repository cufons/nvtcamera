//
// Created by cufon on 10.01.24.
//

#ifndef NVTCAMERA_DEBUG_INFO_H
#define NVTCAMERA_DEBUG_INFO_H
#define TC_GREEN "\u001b[32m"
#define TC_RED "\u001b[31m"
#define TC_YELLOW "\u001b[33m"
#define TC_CLEAR "\u001b[0m"
#define NVTVNC_DEBUG
#ifdef NVTVNC_DEBUG
#define DBG_INFO(x) std::cout <<  "(" << __FILE__ << ":" <<  __LINE__ << ") " << TC_GREEN  << "[" << __PRETTY_FUNCTION__ << "] INFO: "    << x << TC_CLEAR << std::endl
#define DBG_ERROR(x)std::cout <<  "(" << __FILE__ << ":" <<  __LINE__ << ") " << TC_RED    << "[" << __PRETTY_FUNCTION__ << "] ERROR: "   << x << TC_CLEAR << std::endl
#define DBG_WARN(x) std::cout <<  "(" << __FILE__ << ":" <<  __LINE__ << ") " << TC_YELLOW << "[" << __PRETTY_FUNCTION__ << "] WARN: "    << x << TC_CLEAR << std::endl
#else
#define DBG_INFO(x) do {} while(0)
#define DBG_ERROR(x) do {} while(0)
#define DBG_WARN(x) do {} while(0)
#endif
#endif //NVTCAMERA_DEBUG_INFO_H
