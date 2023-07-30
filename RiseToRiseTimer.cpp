#include "RiseToRiseTimer.h"
#include "RiseToRise.pio.h"
#include "hardware/clocks.h"

RiseToRiseTimer::RiseToRiseTimer(PIO pio, uint stateMachine, uint inputPin) {
    this->pio = pio;
    this->stateMachine = stateMachine;
    nsInTick = 1000000000 / clock_get_hz(clk_sys);
    pio_gpio_init(pio, inputPin);
    uint offset = pio_add_program(pio, &RiseToRise_program);
    pio_sm_config config = RiseToRise_program_get_default_config(offset);
    sm_config_set_jmp_pin(&config, inputPin);
    sm_config_set_in_shift(&config, false, false, 0);
    pio_sm_init(pio, stateMachine, offset, &config);
    pio_sm_set_enabled(pio, stateMachine, true);
    // pio_sm_clear_fifos(pio, sm); should I need it
}

RiseToRiseTimer::RiseToRiseTimer(uint inputPin) : RiseToRiseTimer(pio0, 0, inputPin) {
}

bool RiseToRiseTimer::readPeriod(uint32_t& outPeriodNs) {
    if (pio_sm_get_rx_fifo_level(pio, stateMachine) < 1) return false;
    auto count = 2 * pio_sm_get(pio, stateMachine) - 6; // 2x clock cycle per measurement - adjustment
    outPeriodNs = count * nsInTick;
    return true;
}