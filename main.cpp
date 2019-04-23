#include <iostream>
#include <chrono>
#include <thread>
#include "RTLSDR.h"

int main() {
    RTLSDR rtlsdr;
    rtlsdr.start();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    rtlsdr.stop();
    return 0;
}