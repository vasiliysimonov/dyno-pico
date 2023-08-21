

# RC Motor Testing Stand with Flywheel - GitHub Readme

Welcome to my **RC Motor Testing Stand with Flywheel** pet project repository! This project showcases a versatile testing setup designed to accurately assess the performance of electric motors used in RC cars. The stand uses a flywheel attached to the motor's as a load to simulate RC car acceleration, allowing precise measurements of motor RPM and power output. This README will guide you through the project's purpose, features, components, and setup.

![Here's a picture of the testing stand](https://github.com/vasiliysimonov/dyno-pico/blob/main/dyno-stand.jpg)

## Features
- Accurate RPM Measurement: The stand employs a Raspberry Pi Pico microcontroller's Programmable IO (PIO) to measure motor RPM through sensor port using built-in hall sensors. This results in a remarkable measurement accuracy of 16ns, equivalent to 2 Pi Pico CPU cycles per measurement at a clock speed of 125MHz.

- Flywheel Simulated Testing: The lywheel, attached to the motor's shaft, allows motors to be tested under conditions closely resembling real-world scenarios, enabling a more realistic assessment of motor performance.

- Temperature Monitoring: The stand uses a 10KOhm thermistor embedded within the motor to accurately measure its temperature. Temperature has crucial affect on motor's performance.

- Display Interface: RPM and temperature data are displayed in real-time on an SSD1306 LCD connected via I2C interface. The use of a separate CPU core ensures smooth rendering without impacting measurement accuracy.

- Measured rotation periods are logged through the Pico Pico's serial USB interface allowing offline data analysis.

## Components
- Raspberry Pi Pico microcontroller
- SSD1306 LCD display
- Step down voltage regulator
- Sensored RC car bruchless motor
- Machined steel flywheel
- Split resistor resistor

## Usage
The RC Motor Testing Stand offers a comprehensive solution for testing competition Lipo batteries and tuning brushless motor timing as we as comparing different motors. Use the accurate RPM measurements and temperature monitoring to fine-tune motor and battery combinations for optimal performance on the track.

## License
This project is licensed under the [MIT License](LICENSE). Feel free to use, modify, and distribute the code according to the terms of the license.
