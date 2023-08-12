#include "PioTimer.h"
#include "RiseToRise.pio.h"
#include "FallToFall.pio.h"
#include "HighAndLow.pio.h"
#include "hardware/clocks.h"

PioTimer::PioTimer(PIO pio, uint state_machine, uint input_pin):
    m_pio(pio),
    m_state_machine(state_machine),
    m_previous(),
    m_high(true)
{
    m_ns_in_tick = 1000000000 / clock_get_hz(clk_sys);

    pio_gpio_init(pio, input_pin);
    
    m_offset = pio_add_program(pio, &HighAndLow_program);
    pio_sm_config config = HighAndLow_program_get_default_config(m_offset);
    
    sm_config_set_jmp_pin(&config, input_pin);
    sm_config_set_in_shift(&config, false, false, 0);
    pio_sm_init(pio, state_machine, m_offset, &config);
}

PioTimer::~PioTimer() {
    pio_remove_program(m_pio, &HighAndLow_program, m_offset);
}

bool PioTimer::read_period(uint32_t& out_period_ns, char& out_type) {
    if (pio_sm_get_rx_fifo_level(m_pio, m_state_machine) < 1) return false;
    m_high = !m_high;
    auto count = 2 * pio_sm_get(m_pio, m_state_machine) + 4; // 2x clock cycle per measurement + reset + 2x push + 1?
    if (count <= 6) return false; // pin was low initially
    out_period_ns = (count + m_previous) * m_ns_in_tick;
    m_previous = count;
    out_type = m_high ? 'r' : 'f';
    return true;
}
