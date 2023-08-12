#ifndef EMA_H
#define EMA_H

#include <stdint.h>

// Exponential Moving Average
class EMA { 
public:
    EMA(uint32_t period);
    void update(float measurement);
    void reset(float v);
    float get();
private:
    volatile float m_value;
    uint32_t m_count;
    uint32_t m_period;
};

#endif