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

using namespace pico_ssd1306;

struct {
    const uint throttle = 15;
    const uint reverse = 10;

    void init() {
        gpio_pull_up(throttle);
        gpio_set_dir(throttle, GPIO_IN);
        gpio_pull_up(reverse);
        gpio_set_dir(reverse, GPIO_IN);
    }
} buttons;

uint32_t time_diff(uint32_t t2, uint32_t t1) {
    return (t2 >= t1) ? t2 - t1 : 0xFFFFFFFF - t1 + t2;
}

struct Sensor {
    uint pin;
    RingBuffer& buffer;
    uint32_t lastTime;
    bool lastState;

    Sensor(uint _pin, RingBuffer& _buffer) : 
        pin(_pin),
        buffer(_buffer)
    {
    }
    
    void reset() {
        lastState = gpio_get(pin);
        lastTime = 0;
    }

    void handle_interrupt(uint32_t time) {
        bool state = gpio_get(pin);
        if (state && !lastState) { // TODO will it work with edge rise only?
            if (lastTime != 0) {
                // TODO measure systick     
                // https://forums.raspberrypi.com/viewtopic.php?f=145&t=304201&p=1820770&hilit=Hermannsw+systick#p1822677
                buffer.push(time, time_diff(time, lastTime));
            }
            lastTime = time;
        }
        lastState = state;
    }
};

RingBuffer sensorBuffer;
Sensor sensorA(19, sensorBuffer);
Sensor sensorB(20, sensorBuffer);
Sensor sensorC(21, sensorBuffer);

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

void display(SSD1306 &lcd, const char* line) {
    lcd.clear();
    drawText(&lcd, font_12x16, line, 0, 8);        
    lcd.sendBuffer();
}

uint32_t gSmoothRpm = 0;
float gSmoothTemperature = 0.0f;

void update_lcd(SSD1306 &lcd) {
    lcd.clear();
    char line[17];
    itoa(gSmoothRpm, line, 10);
    drawText(&lcd, font_12x16, line, 0, 0);  

    sprintf(line, "%.1fC", gSmoothTemperature);
    drawText(&lcd, font_12x16, line, 0, 18);
    lcd.sendBuffer();
}

void measure_spool_up(SSD1306 &lcd) {
    sensorBuffer.clear();
    sensorA.reset();
    sensorB.reset();
    sensorC.reset();

    servo.setMicros(2000);
    auto startTime = time_us_32();
    uint32_t lastLcd = 0;
    uint32_t count = 0;
    char line[17];
    while (!gpio_get(buttons.throttle)) {
        uint32_t timestamp;
        uint32_t delta = 0;
        uint32_t elapsed = 0;
        while (sensorBuffer.pop(timestamp, delta)) {
            elapsed = time_diff(timestamp, startTime);
            if (count < 16) ++count;
            gSmoothRpm = ((count - 1) * gSmoothRpm + 60000000 / delta) / count;
            printf("%d,%d\n", elapsed, delta);
            if (elapsed > 4000000) { // sec
                printf("final rpm %d\n", gSmoothRpm);
                update_lcd(lcd);
                return;
            }
        }
        if (elapsed != 0 && time_diff(elapsed, lastLcd) > 33333) { // 33ms or 30fps
            update_lcd(lcd);
            lastLcd = elapsed;
        }
    }
}

void handle_interrupt(uint gpio, uint32_t event_mask) {
    auto time = time_us_32();
    if (gpio == sensorA.pin) {
        sensorA.handle_interrupt(time);
    } else if (gpio == sensorB.pin) {
        sensorB.handle_interrupt(time);
    } else if (gpio == sensorC.pin) {
        sensorC.handle_interrupt(time);
    } 
}

void setup_sensor_interrupts() {
    auto flags = GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE;
    gpio_set_irq_enabled(sensorA.pin, flags, true);
    gpio_set_irq_enabled(sensorA.pin, flags, true);
    gpio_set_irq_enabled(sensorA.pin, flags, true);
    gpio_set_irq_callback(&handle_interrupt);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

// TODO 
// explain gaps in measuring intervals
// faster IO?

const uint tempreturePin = 26;

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
    
    // init display
    i2c_setup(i2c1, 6, 7);
    sleep_ms(250);
    SSD1306 lcd = SSD1306(i2c1, 0x3C, Size::W128xH32);
    lcd.setOrientation(1);
    display(lcd, "0");
    
    buttons.init();
    servo.init();

    adc_init();
    adc_gpio_init(tempreturePin);
    adc_select_input(0);

    // interrups on second core
    multicore_launch_core1(&setup_sensor_interrupts);
    
    // ads1115 ADC address 0x48 (72)

    display(lcd, "0");
    bool lastThrottle = false;
    uint32_t count = 0;
    for (int i = 0; true; i++) { 
        // process buttons
        bool throttle = !gpio_get(buttons.throttle);
        bool reverse = !gpio_get(buttons.reverse);

        if (!lastThrottle && throttle) {
            measure_spool_up(lcd);
            servo.setMicros(1500);
        } else if (reverse) {
            servo.setMicros(1000);
        } else {
            servo.setMicros(1500);
        }
        lastThrottle = throttle;

        if (count < 32) ++count; 
        float t = voltage_to_degrees(adc_read());
        gSmoothTemperature = (gSmoothTemperature * (count - 1) + t) / count;

        update_lcd(lcd);

        sleep_ms(50);
    }
}
