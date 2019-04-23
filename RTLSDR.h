//
// Created by player999 on 30.03.19.
//

#ifndef RTLSDR_AM_RADIO_RTLSDR_H
#define RTLSDR_AM_RADIO_RTLSDR_H


#include <rtl-sdr.h>
#include <future>
#include <vector>
#include <complex>
#include <mutex>
#include <pulse/simple.h>

class RTLSDR {
public:
    RTLSDR();
    RTLSDR(int index);
    RTLSDR(const RTLSDR &other) = delete;
    RTLSDR operator =(const RTLSDR &other) = delete;
    ~RTLSDR();
    int start();
    void demodulate();
    int stop();
    std::vector<std::tuple<double,double>> compbuffer;
    std::mutex compbuffer_mutex;
private:
    rtlsdr_dev_t *dev;
    pa_simple *s;
    int devIdx;
    std::future<void> dongle_th;
    void constructor();
    void optimal_settings();
    int ppm_error = 0;
    unsigned int rate_in;
    unsigned int rate_out;
    int downsample;
    int downsample_passes;
    unsigned int set_frequency;
    unsigned int freq;
    unsigned int rate;
    double output_scale;
};


#endif //RTLSDR_AM_RADIO_RTLSDR_H
