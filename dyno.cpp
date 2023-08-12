#define PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 1000

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "pico-ssd1306/ssd1306.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"

#include "RingBuffer.h"
#include "PioTimer.h"

using namespace pico_ssd1306;

const uint PIN_BUTTON_THROTTLE = 15;
const uint PIN_BUTTON_REVERSE = 10;
const uint PIN_SENSOR_A = 19;
const uint PIN_SENSOR_B = 20;
const uint PIN_SENSOR_C = 21;
const uint PIN_ESC = 0;
const uint PIN_ADC_TEMPERATURE = 26;

class EMA { // Exponential Moving Average
public:
    EMA(uint32_t period) : period(period) {
        // mutex_init(&mutex);
    }

    void update(float measurement) {
        // mutex_enter_blocking(&mutex);
        if (count < period) ++count;
        value = ((count - 1) * value + measurement) / count;
        // mutex_exit(&mutex);
    }

    void reset(float v) {
        // mutex_enter_blocking(&mutex);
        value = v;
        count = 0;
        // mutex_exit(&mutex);
    }

    float get() {
        // mutex_enter_blocking(&mutex);
        auto copy = value;
        // mutex_exit(&mutex);
        return copy;
    }

private:
    volatile float value;
    uint32_t count;
    uint32_t period;
    // mutex_t mutex;
};

EMA smoothRpm(16);
EMA smoothTemperature(32);

void i2c_setup(i2c_inst_t *i2c, uint sda, uint scl) {
    i2c_init(i2c, 400 * 1000);
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_pull_up(scl);
}

bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void i2c_scan_address() {
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr)) {
            ret = PICO_ERROR_GENERIC;
        } else {
            ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);
        }

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");
}

void button_init(uint pin) {
    gpio_pull_up(pin);
    gpio_set_dir(pin, GPIO_IN);
}

bool button_is_pressed(uint pin) {
    return !gpio_get(pin);
}

struct {
    uint slice;
    uint channel;
    uint16_t wrap;

    void init(uint pin) {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        slice = pwm_gpio_to_slice_num(pin);
        channel = pwm_gpio_to_channel(pin);
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

void update_lcd(SSD1306 &lcd) {
    lcd.clear(); // TODO don't clear?
    char line[17];
    sprintf(line, "%.0f", smoothRpm.get());
    drawText(&lcd, font_12x16, line, 0, 0);  
    
    sprintf(line, "%.1fC", smoothTemperature.get());
    drawText(&lcd, font_12x16, line, 0, 18);
    lcd.sendBuffer();
}

void print_lcd_updates() {
    i2c_setup(i2c1, 6, 7);
    sleep_ms(250);
    SSD1306 lcd = SSD1306(i2c1, 0x3C, Size::W128xH32);
    lcd.setOrientation(1);

    while (true) {
        update_lcd(lcd);
        sleep_ms(66);
    }
}

void measure_spool_up() {
    PioTimer timers[] {
        PioTimer(pio0, 0, PIN_SENSOR_A),
        PioTimer(pio0, 1, PIN_SENSOR_B),
        PioTimer(pio0, 2, PIN_SENSOR_C)
    };
    const char* timerNames = "abc";
    const uint32_t numTimers = 3;
    const uint32_t mask = 0x7;
    smoothRpm.reset(0);

    servo.setMicros(2000);
    pio_set_sm_mask_enabled(pio0, mask, true);
    uint32_t startTime = time_us_32();
    while (button_is_pressed(PIN_BUTTON_THROTTLE)) { // TODO several button reads
        uint32_t periodNs;
        char type;
        for (int i = 0; i < numTimers; i++) {
            if (!timers[i].readPeriod(periodNs, type)) continue;
            fprintf(stdout, "%c%c %d\n", timerNames[i], type, periodNs);
            smoothRpm.update(60E+9f / periodNs);
            if (time_us_32() - startTime > 1 * 1000000) {
                pio_set_sm_mask_enabled(pio0, mask, false);
                if (pio0->fdebug != 0) fprintf(stdout, "pio debug %x\n", pio0->fdebug);
                fprintf(stdout, "final rpm %.0f\n", smoothRpm.get());
                fflush(stdout);
                return;
            }
        }
    }
}

// TODO 
// explain gaps in measuring intervals

float voltage_to_degrees(uint16_t adc) {
    const float r1 = 9811.00872f;
    const float a = 7.49510291e-04f;
    const float b = 3.39853521e-04f;
    const float c = -6.07716855e-07f;
    const float conversionFactor = 3.3f / (1 << 12);
    float volt = adc * conversionFactor;
    float lnR = log(r1 * volt / (3.3f - volt));
    return 1 / (a + b * lnR + c * lnR * lnR * lnR) - 273.15;
}

int main() {
    stdio_init_all();
    
    // ads1115 ADC address 0x48 (72)
        
    button_init(PIN_BUTTON_THROTTLE);
    button_init(PIN_BUTTON_REVERSE);
    servo.init(PIN_ESC);

    adc_init();
    adc_gpio_init(PIN_ADC_TEMPERATURE);
    adc_select_input(0);

    // lcd on second core
    multicore_launch_core1(&print_lcd_updates);
    
    bool lastThrottle = false;
    while (true) { 
        // process buttons
        bool throttle = button_is_pressed(PIN_BUTTON_THROTTLE);
        bool reverse = button_is_pressed(PIN_BUTTON_REVERSE);

        if (!lastThrottle && throttle) {
            measure_spool_up();
            servo.setMicros(1500);
        } else if (reverse) {
            servo.setMicros(1000);
        } else {
            servo.setMicros(1500);
        }
        lastThrottle = throttle;

        float t = voltage_to_degrees(adc_read());
        smoothTemperature.update(t);

        sleep_ms(50);
    }
}
