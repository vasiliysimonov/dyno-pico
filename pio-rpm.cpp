#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"

#include "RiseToRiseTimer.h"

struct {
    uint pinEsc;
    uint slice;
    uint channel;
    uint16_t wrap;
    uint32_t divider16;

    void init(uint pin) {
        pinEsc = pin;
        gpio_set_function(pinEsc, GPIO_FUNC_PWM);
        slice = pwm_gpio_to_slice_num(pinEsc);
        channel = pwm_gpio_to_channel(pinEsc);
        // servo cycle is 3000mks or 333Hz 
        uint32_t freq = 50;//333;
        uint32_t clock = 125000000;
        divider16 = clock / (freq * 4096) +
                            (clock % (freq * 4096) != 0);
        if (divider16 / 16 == 0) divider16 = 16;
        wrap = clock * 16 / divider16 / freq - 1;
        pwm_set_clkdiv_int_frac(slice, 10, 0);
        pwm_set_wrap(slice, 40000);
        pwm_set_enabled(slice, true);
    }

    // duty 1000mks - low
    // duty 1500mks - mid
    // duty 2000mks - high
    void setMicros(uint32_t micros) {
        pwm_set_chan_level(slice, channel, 20000); // given 333Hz or 3000us cycle
    }
} servo;

int main() {
    stdio_init_all();
    
    servo.init(0);
    servo.setMicros(500);

    RiseToRiseTimer timer(14);
    uint i = 0;
    printf("start\n");
    while (true) {
        uint32_t perNs;
        while (timer.readPeriod(perNs)) {
            //auto pwUs = (pwNano + 999) / 1000;
            //auto perUs = (perNano + 999) / 1000;
            printf("period=%dns", perNs);
            i++;
        }
        sleep_ms(10);
    }
    printf("end\n");
}
