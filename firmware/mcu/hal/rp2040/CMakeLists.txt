cmake_minimum_required(VERSION 3.13)

# Tell cmake to test the cross-compiler by compiling only (no link).
# Without this, cmake tries to link a test binary and fails because
# arm-none-eabi-gcc needs crt0.o / libc which aren't available outside
# the pico-sdk linker script environment.
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# PICO_SDK_PATH must be set in the environment or passed via -DPICO_SDK_PATH=
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# RP2040_TARGET is passed from Makefile.rp2040 (e.g. simpleserial-aes)
# FIRMWAREPATH is the root of the CW firmware directory
if(NOT DEFINED RP2040_TARGET)
    message(FATAL_ERROR "RP2040_TARGET not set. Pass -DRP2040_TARGET=<target_name>")
endif()
if(NOT DEFINED FIRMWAREPATH)
    message(FATAL_ERROR "FIRMWAREPATH not set. Pass -DFIRMWAREPATH=/path/to/firmware")
endif()

project(${RP2040_TARGET} C CXX ASM)

pico_sdk_init()

# RP2040_APP_SRCS: semicolon-separated list of absolute source paths.
# Populated by Makefile.rp2040 from the CW SRC variable + rp2040_hal.c.
if(NOT DEFINED RP2040_APP_SRCS OR RP2040_APP_SRCS STREQUAL "")
    message(FATAL_ERROR "RP2040_APP_SRCS not set — pass all app sources via -DRP2040_APP_SRCS='a.c;b.c'")
endif()

add_executable(${RP2040_TARGET}
    ${RP2040_APP_SRCS}
)

target_include_directories(${RP2040_TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${FIRMWAREPATH}/mcu
    ${FIRMWAREPATH}/mcu/simpleserial
)

# Extra include dirs forwarded from CW's EXTRAINCDIRS
if(DEFINED RP2040_EXTRA_INCDIRS AND NOT RP2040_EXTRA_INCDIRS STREQUAL "")
    target_include_directories(${RP2040_TARGET} PRIVATE ${RP2040_EXTRA_INCDIRS})
endif()

# pico_stdlib pulls in boot2, the startup crt0.S, and the linker script.
# hardware_uart / hardware_gpio not needed — rp2040_hal.c uses direct register access.
target_link_libraries(${RP2040_TARGET}
    pico_stdlib
)

# Disable USB and UART stdio (SimpleSerial owns the UART)
pico_enable_stdio_usb(${RP2040_TARGET} 0)
pico_enable_stdio_uart(${RP2040_TARGET} 0)
