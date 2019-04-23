//
// Created by player999 on 30.03.19.
//

#include "RTLSDR.h"
#include <rtl-sdr.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdexcept>
#include <iostream>
#include <string>
#include <mutex>
#include <cmath>
#include <fstream>
#include <tuple>

using namespace std;
const unsigned int default_sample_rate = 1008000;
const unsigned int audio_sample_rate = 44100;
const unsigned int default_buf_length = 16384;
const unsigned int maximum_oversample = 16;
const unsigned int maximum_buf_length = maximum_oversample * default_buf_length;


RTLSDR::RTLSDR() {
    devIdx = 0;
    constructor();
}

RTLSDR::RTLSDR(int index): devIdx(index) {constructor();}

void RTLSDR::constructor() {
    char vendor[256], product[256], cserial[256];
    int device_count = rtlsdr_get_device_count();
    int retval;
    if(device_count == 0)
    {
        throw runtime_error("Could not find any RTLSDR device");
    }
    if(devIdx >= device_count)
    {
        throw runtime_error("We have only" + to_string(device_count) + "RTLSDR devices");
    }
    rtlsdr_get_device_usb_strings(devIdx, vendor, product, cserial);
    int device = rtlsdr_get_index_by_serial(cserial);
    if(retval = rtlsdr_open(&dev, device))
    {
        throw  runtime_error("Failed to open RTLSDR. Error #" + retval);
    }

    int error;
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = audio_sample_rate,
        .channels = 1
    };
    if (!(s = pa_simple_new(NULL, "FM Radio", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        throw  runtime_error("pa_simple_new() failed: " + to_string(error));
    }

    rate_in = default_sample_rate;
    rate_out = default_sample_rate;
    downsample_passes = 0;
    compbuffer.reserve(maximum_buf_length / 2 /*nums per complex*/);
    set_frequency = 100000000;
}

RTLSDR::~RTLSDR() {
    int error;
    pa_simple_drain(s, &error);
    if (s) pa_simple_free(s);
    rtlsdr_close(dev);
}

void RTLSDR::optimal_settings() {
    downsample = (1000000 / rate_in) + 1;
    if (downsample_passes) {
        downsample_passes = (int)log2(downsample) + 1;
        downsample = 1 << downsample_passes;
    }
    freq = set_frequency;
    rate = downsample * rate_in;
}

const double pi = 3.1415926;
void RTLSDR::demodulate() {
    lock_guard<mutex> lock(compbuffer_mutex);
    vector<double>atg(compbuffer.size() - 1);
    double atan_old = atan2(get<1>(compbuffer[0]), get<0>(compbuffer[0]));
    double atan_new;
    for(int i = 1; i < compbuffer.size(); i++)
    {
        atan_new = atan2(get<1>(compbuffer[i]), get<0>(compbuffer[i]));
        atg[i - 1] = atan_new - atan_old;
        if(atg[i - 1] > pi)
        {
            atg[i - 1] -= 2 * pi;
        }
        else if(atg[i - 1] < -pi)
        {
            atg[i - 1] += 2 * pi;
        }
        atan_old = atan_new;
    }

    const unsigned int filter_size = 10;
    vector<double>mn(atg.size() - filter_size);
    double mean = 0;
    for(int i = 0; i < filter_size; i++)
    {
        mean += atg[i];
    }
    mean /= filter_size;
    for(int i = 0; i < mn.size(); i++)
    {
        mn[i] = mean + pi;
        mean -= atg[i] / filter_size;
        mean += atg[filter_size + i] / filter_size;
    }
    int downsample_rate = default_sample_rate / (audio_sample_rate);
    short output_buffer[mn.size()/downsample_rate+1];
    unsigned int offset = 0;
    for(int i = 0; i < mn.size(); i++)
    {
        if((i % downsample_rate) == 0)
        {
            output_buffer[offset++] = static_cast<short>(mn[i] * 32768.0 * 2);
        }
    }
    int error;
    if(pa_simple_write(s, reinterpret_cast<void*>(output_buffer), sizeof(short) * offset, &error) < 0)
    {
        cout << "Error write: " << error << endl;
    }
    return;
}

extern "C" {
    static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx)
    {
        RTLSDR *cl = static_cast<RTLSDR *>(ctx);
        {
            lock_guard<mutex> lock(cl->compbuffer_mutex);
            cl->compbuffer.clear();
            for (uint32_t i = 0; i < len; i += 2) {
                cl->compbuffer.push_back(tuple<double,double>(buf[i + 0] - 127, buf[i + 1] - 127));
            }
        }

        future<void> fut = async([cl]() {cl->demodulate();});
    }
}

int RTLSDR::start()
{
    int retval;

    optimal_settings();
    retval = rtlsdr_set_bias_tee(dev, 1);
    if (retval) {
        return retval;
    }

    if(ppm_error) {
        retval = rtlsdr_set_freq_correction(dev, ppm_error);
        if (retval) {
            return retval;
        }
    }

    retval = rtlsdr_reset_buffer(dev);
    if (retval) {
        return retval;
    }

    retval = rtlsdr_set_center_freq(dev, freq);
    if (retval) {
        return retval;
    }

    retval = rtlsdr_set_sample_rate(dev, default_sample_rate);
    if (retval) {
        return retval;
    }

    retval = rtlsdr_set_tuner_gain_mode(dev, 0);
    if (retval) {
        return retval;
    }

    dongle_th = async([=]() {
        rtlsdr_read_async(dev, rtlsdr_callback, static_cast<void *>(this), 0, maximum_buf_length);
    });
}

int RTLSDR::stop()
{
    rtlsdr_cancel_async(dev);
    dongle_th.wait();
}
