#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/mutex.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico-ssd1306/ssd1306.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"

const uint led_pin = 25;

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
    const uint throttle = 16;
    const uint reverse = 15;

    void init() {
        gpio_pull_up(throttle);
        gpio_set_dir(throttle, GPIO_IN);
        gpio_pull_up(reverse);
        gpio_set_dir(reverse, GPIO_IN);
    }
} buttons;

#define SIZE 64

struct RingBuffer {
    int32_t head;
    int32_t tail;
    bool full;
    uint32_t measuresNs[SIZE];
    mutex_t mutex;

    RingBuffer() :
        head(),
        tail(),
        full(false) 
    {
        mutex_init(&mutex);
    }

    void clear() {
        mutex_enter_blocking(&mutex);
        head = 0;
        tail = 0;
        full = false;
        mutex_exit(&mutex);
    }

    void push(uint32_t measure) {
        mutex_enter_blocking(&mutex);
        measuresNs[tail] = measure;
        tail = next(tail);
        if (full) { // overwrite
            head = tail;
        } else {
            full = (tail == head);
        }
        mutex_exit(&mutex);
    }

    bool pop(uint32_t& prev, uint32_t& curr) { 
        mutex_enter_blocking(&mutex);
        bool result = false;
        if (size() > 1) { // TODO simplify count
            auto nextHead = next(head);
            prev = measuresNs[head];
            curr = measuresNs[nextHead];
            head = nextHead;
            full = false;
            result = true;
        }
        mutex_exit(&mutex);
        return result;
    }

    int32_t size() {
        if (full) return SIZE;
        auto diff = tail - head;
        return (diff >= 0) ? diff : diff + 16;
    }

    inline int32_t next(int32_t i) {
        auto next = i + 1;
        return (next == SIZE) ? 0 : next;
    }
};

struct Sensor {
    uint pin;
    RingBuffer measures;
    
    void init(uint _pin) {
        pin = _pin;
        gpio_set_irq_enabled(_pin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    }    
};

Sensor sensorA;
Sensor sensorB;
Sensor sensorC;

struct {
    const uint pinEsc = 3;
    uint slice;
    uint channel;
    uint16_t wrap;

    void init() {
        gpio_set_function(pinEsc, GPIO_FUNC_PWM);
        slice = pwm_gpio_to_slice_num(pinEsc);
        channel = pwm_gpio_to_channel(pinEsc);
        // servo cycle is 20000mks or 50Hz 
        uint32_t freq = 50;
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
        pwm_set_chan_level(slice, channel, wrap * micros / 20000); // given 50Hz or 20_000mks cycle
    }
} servo;

uint32_t time_diff(uint32_t t2, uint32_t t1) {
    return (t2 >= t1) ? t2 - t1 : 0xFFFFFFFF - t1 + t2;
}

void display(SSD1306 &lcd, const char* line) {
    lcd.clear();
    drawText(&lcd, font_12x16, line, 0, 12);        
    lcd.sendBuffer();
}

void measure_spool_up(SSD1306 &lcd) {
    sensorB.measures.clear();
    servo.setMicros(1580);
    auto startUs = time_us_32();
    int32_t maxLength = 0;
    uint32_t rpm = 0;
    char line[17];
    while (!gpio_get(buttons.throttle)) {
        uint32_t prev;
        uint32_t curr;
        maxLength = MAX(sensorB.measures.size(), maxLength);
        while (sensorB.measures.pop(prev, curr)) {
            auto time = time_diff(curr, startUs);
            auto rpm = 60000000 / time_diff(curr, prev);
            itoa(rpm, line, 10);
            puts(line);
            if (time > 3000000L) { // sec
                printf("final rpm %d\n", rpm);
                printf("max length %d\n", maxLength);
                display(lcd, line);
                return;
            }
        }
        display(lcd, line);
        //sleep_ms(50);
    }
}

bool lastB = false;
void handle_interrupt(uint gpio, uint32_t event_mask) {
    if (gpio == sensorB.pin) {
        bool b = gpio_get(sensorB.pin);
        if (b && !lastB) {
            // TODO measure systick 
            // https://forums.raspberrypi.com/viewtopic.php?f=145&t=304201&p=1820770&hilit=Hermannsw+systick#p1822677
            sensorB.measures.push(time_us_32());
        }
        lastB = b;
    }
}

void setup_sensor_interrupts() {
    //sensorA.init(19);
    sensorB.init(20);
    //sensorC.init(21);
    gpio_set_irq_callback(&handle_interrupt);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

int main() {
    stdio_init_all();

    // Initialize LED pin
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    // ads1115 ADC address 0x48 (72)
    
    // init display
    i2c_setup(i2c0, 12, 13);
    sleep_ms(250);
    SSD1306 lcd = SSD1306(i2c0, 0x3C, Size::W128xH32);
    lcd.setOrientation(0);
    
    buttons.init();
    servo.init();

    // interrups on second core
    multicore_launch_core1(&setup_sensor_interrupts);
    
    // ads1115 ADC address 0x48 (72)

    display(lcd, "0");
    bool lastThrottle = false;
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
        sleep_ms(100);
    }
}
