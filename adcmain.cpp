
#define PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 1000

#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "pico-ads1x15/include/ads1x15/ads1x15.hpp"

const uint PIN_SERIAL_DATA = 20;
const uint PIN_SERIAL_CLOCK = 21;

void i2c_setup(i2c_inst_t *i2c, uint sda, uint scl) {
    i2c_init(i2c, 400 * 1000);
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_pull_up(scl);
}

int main() {
    stdio_init_all();
    i2c_setup(i2c0, PIN_SERIAL_DATA, PIN_SERIAL_CLOCK);
    sleep_ms(1000);
    PICO_ADS1115 ads;
    ads.setGain(ADSXGain_ONE);
    ads.setDataRate(RATE_ADS1115_128SPS);
    if (!ads.beginADSX(ADSX_ADDRESS_GND, i2c0, 400, PIN_SERIAL_DATA, PIN_SERIAL_CLOCK)) {
        printf("ADS1115 : Failed to initialize ADS.!\r\n");
        while (1);
    }
    while (true) {
        auto t = time_us_32();
        auto a0 = ads.computeVolts(ads.readADC_SingleEnded(ADSX_AIN0));
        auto a1 = ads.computeVolts(ads.readADC_SingleEnded(ADSX_AIN1));
        auto v0 = a0 * 10.97687251f; // in practice with divider resistors (4.7 + 47) / 4.7;
        auto half_ref = 1.635545f;
        auto amp = a1 * 200 / half_ref - 200;
        t = time_us_32() - t;
        fprintf(stdout, "%f\t%f\n", a0, a1);
        sleep_ms(250);
    }
}