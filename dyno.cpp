#define PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 1000

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "pico-ssd1306/ssd1306.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"

#include "RingBuffer.h"
#include "PioTimer.h"
#include "EMA.h"
#include "Servo.h"

using namespace pico_ssd1306;

const uint PIN_SERIAL_DATA = 6;
const uint PIN_SERIAL_CLOCK = 7;
const uint PIN_BUTTON_THROTTLE = 15;
const uint PIN_BUTTON_REVERSE = 10;
const uint PIN_SENSOR_A = 19;
const uint PIN_SENSOR_B = 20;
const uint PIN_SENSOR_C = 21;
const uint PIN_ESC = 0;
const uint PIN_ADC_TEMPERATURE = 26;

EMA g_smooth_rpm(16);
EMA g_smooth_temperature(32);
Servo g_servo;

void i2c_setup(i2c_inst_t *i2c, uint sda, uint scl) {
    i2c_init(i2c, 400 * 1000);
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_pull_up(scl);
}

bool i2c_is_reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void i2c_scan_address() {
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            fprintf(stdout, "%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (i2c_is_reserved_addr(addr)) {
            ret = PICO_ERROR_GENERIC;
        } else {
            ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);
        }

        fprintf(stdout, ret < 0 ? "." : "@");
        fprintf(stdout, addr % 16 == 15 ? "\n" : "  ");
    }
    fprintf(stdout, "done.\n");
}

void button_init(uint pin) {
    gpio_pull_up(pin);
    gpio_set_dir(pin, GPIO_IN);
}

bool button_is_pressed(uint pin) {
    return !gpio_get(pin);
}

bool button_released_for_ms(uint pin, uint ms) {
    for (int i = 0; i < ms; ++i) {
        if (button_is_pressed(pin)) return false;
        sleep_ms(1);
    }
    return true;
}

void lcd_show_updates() {
    i2c_setup(i2c1, PIN_SERIAL_DATA, PIN_SERIAL_CLOCK);
    sleep_ms(250); // let it settle
    SSD1306 lcd = SSD1306(i2c1, 0x3C, Size::W128xH32);
    lcd.setOrientation(1);

    char line[17];
    while (true) {
        lcd.clear();
        
        sprintf(line, "%.0f", g_smooth_rpm.get());
        drawText(&lcd, font_12x16, line, 0, 0);  
        
        sprintf(line, "%.1fC", g_smooth_temperature.get());
        drawText(&lcd, font_12x16, line, 0, 18);

        lcd.sendBuffer();

        sleep_ms(66);
    }
}

void measure_spool_up() {
    PioTimer timers[] {
        PioTimer(pio0, 0, PIN_SENSOR_A),
        PioTimer(pio0, 1, PIN_SENSOR_B),
        PioTimer(pio0, 2, PIN_SENSOR_C)
    };
    const char* timer_names = "abc";
    const uint32_t num_timers = 3;
    const uint32_t mask = 0x7;

    g_smooth_rpm.reset(0);
    g_servo.set_micros(2000);
    pio_set_sm_mask_enabled(pio0, mask, true);
    uint32_t start_time = time_us_32();

    for (int i = 0; true; ++i) {
        if (button_released_for_ms(PIN_BUTTON_THROTTLE, 100)) break;
        if (i >= num_timers) i = 0;
        uint32_t period_ns;
        char type;
        if (!timers[i].read_period(period_ns, type)) continue;
        fprintf(stdout, "%c%c %d\n", timer_names[i], type, period_ns);
        g_smooth_rpm.update(60E+9f / period_ns);
        if (time_us_32() - start_time > 4 * 1000000) {
            pio_set_sm_mask_enabled(pio0, mask, false);
            fprintf(stdout, "-- final rpm %.0f\n", g_smooth_rpm.get());
            if (pio0->fdebug != 0) {
                fprintf(stdout, "-- pio debug %x\n", pio0->fdebug);
            }
            fflush(stdout);
            return;
        }
    }
}

float voltage_to_degrees(uint16_t adc) {
    const float r1 = 9811.00872f;
    const float a = 7.49510291e-04f;
    const float b = 3.39853521e-04f;
    const float c = -6.07716855e-07f;
    const float conversion_factor = 3.3f / (1 << 12);
    float volt = adc * conversion_factor;
    float ln_r = log(r1 * volt / (3.3f - volt));
    return 1 / (a + b * ln_r + c * ln_r * ln_r * ln_r) - 273.15;
}

int main() {
    stdio_init_all();
    
    // ads1115 ADC address 0x48 (72)
        
    button_init(PIN_BUTTON_THROTTLE);
    button_init(PIN_BUTTON_REVERSE);
    g_servo.init(PIN_ESC);

    adc_init();
    adc_gpio_init(PIN_ADC_TEMPERATURE);
    adc_select_input(0);

    // lcd on second core
    multicore_launch_core1(&lcd_show_updates);
    
    bool last_throttle = false;
    while (true) { 
        // process buttons
        bool throttle = button_is_pressed(PIN_BUTTON_THROTTLE);
        bool reverse = button_is_pressed(PIN_BUTTON_REVERSE);

        if (!last_throttle && throttle) {
            measure_spool_up();
            g_servo.set_micros(1500);
        } else if (reverse) {
            g_servo.set_micros(1000);
        } else {
            g_servo.set_micros(1500);
        }
        last_throttle = throttle;

        float t = voltage_to_degrees(adc_read());
        g_smooth_temperature.update(t);

        sleep_ms(50);
    }
}
