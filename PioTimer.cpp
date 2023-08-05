#include "PioTimer.h"
#include "RiseToRise.pio.h"
#include "FallToFall.pio.h"
#include "hardware/clocks.h"

PioTimer::PioTimer(PIO pio, uint stateMachine, uint inputPin, PioTimer::Type type) {
    this->pio = pio;
    this->stateMachine = stateMachine;
    nsInTick = 1000000000 / clock_get_hz(clk_sys);
    pio_gpio_init(pio, inputPin);
    uint offset;
    pio_sm_config config;
    if (type == TYPE_RAISING_EDGE) {
        offset = pio_add_program(pio, &RiseToRise_program);
        config = RiseToRise_program_get_default_config(offset);
    } else {
        offset = pio_add_program(pio, &FallToFall_program);
        config = FallToFall_program_get_default_config(offset);
    }
    sm_config_set_jmp_pin(&config, inputPin);
    sm_config_set_in_shift(&config, false, false, 0);
    pio_sm_init(pio, stateMachine, offset, &config);
}

bool PioTimer::readPeriod(uint32_t& outPeriodNs) {
    if (pio_sm_get_rx_fifo_level(pio, stateMachine) < 1) return false;
    auto count = 2 * pio_sm_get(pio, stateMachine) + 4; // 2x clock cycle per measurement + reset + 2x push + 1?
    outPeriodNs = count * nsInTick;
    return true;
}

PioTimer::~PioTimer() {
    pio_sm_set_enabled(pio, stateMachine, false);
}