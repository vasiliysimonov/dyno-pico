#include "Servo.h"

#include "hardware/pwm.h"

void Servo::init(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    m_slice = pwm_gpio_to_slice_num(pin);
    m_channel = pwm_gpio_to_channel(pin);
    // servo cycle is 3000mks or 333Hz 
    uint32_t freq = 333;
    uint32_t clock = 125000000;
    uint32_t divider16 = clock / (freq * 4096) +
                        (clock % (freq * 4096) != 0);
    if (divider16 / 16 == 0) divider16 = 16;
    m_wrap = clock * 16 / divider16 / freq - 1;
    pwm_set_clkdiv_int_frac(m_slice, divider16 / 16, divider16 & 0xF);
    pwm_set_wrap(m_slice, m_wrap);
    pwm_set_enabled(m_slice, true);
}

void Servo::set_micros(uint32_t micros) {
    pwm_set_chan_level(m_slice, m_channel, m_wrap * micros / 3000); // given 333Hz or 3000us cycle
}
