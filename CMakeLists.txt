# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

#include_directories("${PICO_SDK_PATH}/src/common/pico_stdlib/include/") 

# Set name of project (as PROJECT_NAME) and C/C++ standards
project(dyno-pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

add_subdirectory(pico-ssd1306) # clone https://github.com/Harbys/pico-ssd1306.git

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} 
    dyno.cpp
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
    pico_multicore
    hardware_i2c
    hardware_pwm
    hardware_adc 
    pico_ssd1306
)

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
