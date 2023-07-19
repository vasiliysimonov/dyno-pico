#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "PwmIn.pio.h"
#include "hardware/pwm.h"

// class that sets up and reads PWM pulses: PwmIn. It has three functions:
// read_period (in seconds)
// read_pulsewidth (in seconds)
// read_dutycycle (between 0 and 1)
// read pulsewidth, period, and calculate the dutycycle

class PwmIn
{
public:
    // constructor
    // input = pin that receives the PWM pulses.
    PwmIn(uint input)
    {
        // pio 0 is used
        pio = pio0;
        // state machine 0
        sm = 0;
        // configure the used pins
        pio_gpio_init(pio, input);
        // load the pio program into the pio memory
        uint offset = pio_add_program(pio, &PwmIn_program);
        // make a sm config
        pio_sm_config c = PwmIn_program_get_default_config(offset);
        // set the 'jmp' pin
        sm_config_set_jmp_pin(&c, input);
        // set the 'wait' pin (uses 'in' pins)
        sm_config_set_in_pins(&c, input);
        // set shift direction
        sm_config_set_in_shift(&c, false, false, 0);
        // init the pio sm with the config
        pio_sm_init(pio, sm, offset, &c);
        // enable the sm
        pio_sm_set_enabled(pio, sm, true);
    }

    // read_period (in seconds)
    float read_period(void)
    {
        read();
        // one clock cycle is 1/125000000 seconds
        return (period * 0.000000008);
    }

    // read_pulsewidth (in seconds)
    float read_pulsewidth(void)
    {
        read();
        // one clock cycle is 1/125000000 seconds
        return (pulsewidth * 0.000000008);
    }

    // read_dutycycle (between 0 and 1)
    float read_dutycycle(void)
    {
        read();
        return ((float)pulsewidth / (float)period);
    }

    // read pulsewidth and period for one pulse
    void read_PWM(float *readings)
    {
        read();
        *(readings + 0) = (float)pulsewidth * 0.000000008;
        *(readings + 1) = (float)period * 0.000000008;
        *(readings + 2) = ((float)pulsewidth / (float)period);
    }

private:
    // read the period and pulsewidth
    void read(void)
    {
        // clear the FIFO: do a new measurement
        pio_sm_clear_fifos(pio, sm);
        // wait for the FIFO to contain two data items: pulsewidth and period
        while (pio_sm_get_rx_fifo_level(pio, sm) < 2)
            ;
        // read pulse width from the FIFO
        pulsewidth = pio_sm_get(pio, sm);
        // read period from the FIFO
        period = pio_sm_get(pio, sm) + pulsewidth;
        // the measurements are taken with 2 clock cycles per timer tick
        pulsewidth = 2 * pulsewidth;
        // calculate the period in clock cycles:
        period = 2 * period;
    }
    // the pio instance
    PIO pio;
    // the state machine
    uint sm;
    // data about the PWM input measured in pio clock cycles
    uint32_t pulsewidth, period;
};

struct {
    const uint pinEsc = 0;
    uint slice;
    uint channel;
    uint16_t wrap;

    void init() {
        gpio_set_function(pinEsc, GPIO_FUNC_PWM);
        slice = pwm_gpio_to_slice_num(pinEsc);
        channel = pwm_gpio_to_channel(pinEsc);
        // servo cycle is 3000mks or 333Hz 
        uint32_t freq = 333;
        uint32_t clock = 125000000;
        uint32_t divider16 = clock / (freq * 4096) +
                            (clock % (freq * 4096) != 0);
        if (divider16 / 16 == 0) divider16 = 16;
        wrap = clock * 16 / divider16 / freq - 1;
        pwm_set_clkdiv_int_frac(slice, divider16 / 16, divider16 & 0xF);
        pwm_set_wrap(slice, wrap);
        pwm_set_enabled(slice, true);
    }

    // duty 1000mks - low
    // duty 1500mks - mid
    // duty 2000mks - high
    void setMicros(uint32_t micros) {
        pwm_set_chan_level(slice, channel, wrap * micros / 3000); // given 333Hz or 3000us cycle
    }
} servo;

int main() {
    servo.init();
    servo.setMicros(1500);

    // needed for printf
    stdio_init_all();
    // the instance of the PwmIn
    PwmIn my_PwmIn(14);
    // the array with the results
    float pwm_reading[3];
    // infinite loop to print PWM measurements
    while (true) {
        my_PwmIn.read_PWM(pwm_reading);
        if (pwm_reading[0] >= 0.)
        {
            printf("pw=%.8f \tp=%.8f \tdc=%.8f\n", pwm_reading[0], pwm_reading[1], pwm_reading[2]);
        }
        sleep_ms(100);
    }
}