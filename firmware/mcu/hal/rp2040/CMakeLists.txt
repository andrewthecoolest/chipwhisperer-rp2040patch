cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH must be set in the environment or passed via -DPICO_SDK_PATH=
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# RP2040_TARGET is passed from Makefile.rp2040 (e.g. simpleserial-aes-CW308_RP2040)
# FIRMWAREPATH is the root of the CW firmware directory
if(NOT DEFINED RP2040_TARGET)
    message(FATAL_ERROR "RP2040_TARGET not set. Pass -DRP2040_TARGET=<target_name>")
endif()
if(NOT DEFINED FIRMWAREPATH)
    message(FATAL_ERROR "FIRMWAREPATH not set. Pass -DFIRMWAREPATH=/path/to/firmware")
endif()

project(${RP2040_TARGET} C CXX ASM)

pico_sdk_init()

# Collect app sources — the app Makefile passes its source files via
# RP2040_APP_SRCS, or we fall back to globbing the firmware mcu directory.
# TODO: wire up the actual app source list from the CW build system.
# For now, list sources explicitly when invoking cmake:
#   cmake ... -DRP2040_APP_SRCS="path/to/foo.c;path/to/bar.c"
if(NOT DEFINED RP2040_APP_SRCS)
    message(WARNING "RP2040_APP_SRCS not set — no app sources will be compiled")
    set(RP2040_APP_SRCS "")
endif()

add_executable(${RP2040_TARGET}
    ${CMAKE_CURRENT_LIST_DIR}/rp2040_hal.c
    ${RP2040_APP_SRCS}
)

target_include_directories(${RP2040_TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${FIRMWAREPATH}/mcu
    ${FIRMWAREPATH}/mcu/simpleserial
)

# pico-sdk libraries: hardware_uart and hardware_gpio are not needed since
# rp2040_hal.c uses direct register access. pico_stdlib pulls in boot2,
# the startup, and the linker script.
target_link_libraries(${RP2040_TARGET}
    pico_stdlib
)

# Produce .bin and .uf2 alongside the .elf
pico_add_extra_outputs(${RP2040_TARGET})

# Disable USB and UART stdio (SimpleSerial owns the UART)
pico_enable_stdio_usb(${RP2040_TARGET} 0)
pico_enable_stdio_uart(${RP2040_TARGET} 0)
