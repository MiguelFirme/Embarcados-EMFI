cmake_minimum_required(VERSION 3.13)

# Point PICO_SDK_PATH at a local checkout of the official SDK.
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "Set PICO_SDK_PATH to the Raspberry Pi Pico SDK checkout")
endif()
if(DEFINED PICO_SDK_PATH)
    include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
else()
    include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
endif()

project(enfi_controller C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(enfi_controller
    src/main.c
    src/emfi.c
    src/protocol.c
    src/experiment.c
    src/target.c
)
target_include_directories(enfi_controller PRIVATE src)
target_link_libraries(enfi_controller pico_stdlib hardware_uart)
target_compile_definitions(enfi_controller PRIVATE SIMULATION_MODE=1)
pico_enable_stdio_usb(enfi_controller 1)
pico_enable_stdio_uart(enfi_controller 0)
pico_add_extra_outputs(enfi_controller)

add_executable(enfi_target target_firmware/main.c)
target_link_libraries(enfi_target pico_stdlib hardware_uart)
pico_enable_stdio_usb(enfi_target 0)
pico_enable_stdio_uart(enfi_target 0)
pico_add_extra_outputs(enfi_target)
