#ifndef SERVO_H
#define SERVO_h

#include "pico/stdlib.h"

class Servo {
public:
    void init(uint pin);

    // duty 1000mks - low
    // duty 1500mks - mid
    // duty 2000mks - high
    void set_micros(uint32_t micros);
private:
    uint m_slice;
    uint m_channel;
    uint16_t m_wrap;
};

#endif