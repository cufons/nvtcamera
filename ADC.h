//
// Created by cufon on 12.06.24.
//

#ifndef NVTCAMERA_ADC_H
#define NVTCAMERA_ADC_H
#include <string>
#include <array>
#include <fstream>
namespace NVTSYS {

    class ADC {
        std::string basePath;
        static ADC* instance;
        std::array<std::ifstream ,4> channels;
    public:
        virtual ~ADC();

    private:
        bool tryEnable();
        bool openChannels();
    public:
        ADC();
        static ADC *getInstance();
        static void init();
        int readChannel(unsigned channel);
        int getChanCount();
    };

} // NVTSYS

#endif //NVTCAMERA_ADC_H
