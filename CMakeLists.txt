cmake_minimum_required(VERSION 3.13)

# Include the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(flipper_cec_module C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Add the executable
add_executable(flipper_cec_module
    main.c
)

# Link the required libraries
target_link_libraries(flipper_cec_module 
    pico_stdlib
    pico_time
    hardware_gpio
    hardware_uart
    hardware_i2c
    hardware_pwm
    pico_unique_id
    pico_bootrom
)

# Enable USB output, enable UART output
pico_enable_stdio_usb(flipper_cec_module 1)
pico_enable_stdio_uart(flipper_cec_module 1)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(flipper_cec_module)

# Set program information
pico_set_program_name(flipper_cec_module "Flipper CEC Module")
pico_set_program_version(flipper_cec_module "1.0.0")
pico_set_program_description(flipper_cec_module "CEC Test Tool for Flipper Zero Game Module")
pico_set_program_url(flipper_cec_module "https://github.com/YOUR_USERNAME/flipper-cec-game-module")

# Compile definitions
target_compile_definitions(flipper_cec_module PRIVATE
    PICO_DEFAULT_UART=1
    PICO_DEFAULT_UART_TX_PIN=0
    PICO_DEFAULT_UART_RX_PIN=1
)
