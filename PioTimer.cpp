#include "PioTimer.h"
#include "RiseToRise.pio.h"
#include "FallToFall.pio.h"
#include "HighAndLow.pio.h"
#include "hardware/clocks.h"

PioTimer::PioTimer(PIO pio, uint stateMachine, uint inputPin) {
    this->pio = pio;
    this->stateMachine = stateMachine;
    nsInTick = 1000000000 / clock_get_hz(clk_sys);
    previous = 0;
    high = true;

    pio_gpio_init(pio, inputPin);
    
    offset = pio_add_program(pio, &HighAndLow_program);
    pio_sm_config config = HighAndLow_program_get_default_config(offset);
    
    sm_config_set_jmp_pin(&config, inputPin);
    sm_config_set_in_shift(&config, false, false, 0);
    pio_sm_init(pio, stateMachine, offset, &config);
}

PioTimer::~PioTimer() {
    pio_remove_program(pio, &HighAndLow_program, offset);
}

bool PioTimer::readPeriod(uint32_t& outPeriodNs, char& outType) {
    if (pio_sm_get_rx_fifo_level(pio, stateMachine) < 1) return false;
    high = !high;
    auto count = 2 * pio_sm_get(pio, stateMachine) + 4; // 2x clock cycle per measurement + reset + 2x push + 1?
    if (count <= 6) return false; // pin was low initially
    outPeriodNs = (count + previous) * nsInTick;
    previous = count;
    outType = high ? 'r' : 'f';
    return true;
}
