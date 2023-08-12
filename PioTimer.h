#ifndef __RISE_TO_RISE_TIMER_H__
#define __RISE_TO_RISE_TIMER_H__

#include "hardware/pio.h"

class PioTimer {
public:
    PioTimer(PIO pio, uint state_machine, uint input_pin);
    ~PioTimer();
    bool read_period(uint32_t& out_period_ns, char& out_type);
private:
    PIO m_pio;
    uint m_state_machine;
    uint m_offset;
    uint32_t m_ns_in_tick;
    uint32_t m_previous;
    bool m_high;
};

#endif