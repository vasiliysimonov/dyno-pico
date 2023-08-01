#ifndef __RISE_TO_RISE_TIMER_H__
#define __RISE_TO_RISE_TIMER_H__

#include "hardware/pio.h"

class RiseToRiseTimer {
public:
    RiseToRiseTimer(PIO pio, uint stateMachine, uint inputPin);
    RiseToRiseTimer(uint inputPin);
    bool readPeriod(uint32_t& outPeriodNs);
    ~RiseToRiseTimer();
private:
    PIO pio;
    uint stateMachine;
    uint32_t nsInTick;
};

#endif