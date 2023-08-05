#ifndef __RISE_TO_RISE_TIMER_H__
#define __RISE_TO_RISE_TIMER_H__

#include "hardware/pio.h"

class PioTimer {
public:
    enum Type{
        TYPE_FALLING_EDGE,
        TYPE_RAISING_EDGE
    };

    PioTimer(PIO pio, uint stateMachine, uint inputPin, Type type);
    bool readPeriod(uint32_t& outPeriodNs);
    ~PioTimer();
private:
    PIO pio;
    uint stateMachine;
    uint32_t nsInTick;
};

#endif