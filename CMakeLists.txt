cmake_minimum_required(VERSION 3.20)
# Cross build for STM32F407 (Cortex-M4F). Configure with the toolchain file:
#   cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
project(cortex_m_flight_controller C ASM)
set(CMAKE_C_STANDARD 11)

set(FC_FW_VERSION "1.0.0" CACHE STRING "application version MAJOR.MINOR.PATCH")
option(FC_ENABLE_FAULT_INJECTION "compile debug fault-injection commands into the app" ON)
option(FC_STRICT "warnings as errors for first-party code" ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(ROOT ${CMAKE_SOURCE_DIR})

# ---- dev signing key + bootloader public key (generated once, never overwritten) ----
if(NOT EXISTS ${ROOT}/bootloader/inc/pubkey.h OR NOT EXISTS ${ROOT}/keys/dev_private.key)
  message(STATUS "Generating development signing key (keys/, bootloader/inc/pubkey.h)")
  execute_process(COMMAND ${Python3_EXECUTABLE} ${ROOT}/tools/keygen.py RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "keygen failed - run: pip install cryptography")
  endif()
endif()

string(REPLACE "." ";" VER_LIST ${FC_FW_VERSION})
list(GET VER_LIST 0 VMAJ)
list(GET VER_LIST 1 VMIN)
list(GET VER_LIST 2 VPAT)
math(EXPR FC_VERSION_NUM "(${VMAJ} << 24) | (${VMIN} << 16) | ${VPAT}")

set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
set(COMMON_DEFS STM32F407xx)
set(COMMON_OPTS ${CPU_FLAGS} -ffunction-sections -fdata-sections -fno-common -g3 -Os)
set(WARN_OPTS -Wall -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wundef -Wconversion -Wno-sign-conversion)
if(FC_STRICT)
  list(APPEND WARN_OPTS -Werror)
endif()

set(VENDOR_INC ${ROOT}/third_party/cmsis_device_f4 ${ROOT}/third_party/cmsis_core)
set(OWN_INC ${ROOT}/bsp ${ROOT}/drivers ${ROOT}/common ${ROOT}/config)

# ---------------------------------------------------------------- libraries
add_library(bsp STATIC bsp/bsp.c bsp/startup.c bsp/dbg.c bsp/fault.c)
add_library(drivers STATIC drivers/gpio.c drivers/uart.c drivers/spi.c drivers/i2c.c drivers/can.c drivers/adc.c
                           drivers/pwm.c drivers/flash.c drivers/iwdg.c drivers/imu_mpu6500.c drivers/baro_bmp280.c)
add_library(fc_common STATIC common/crc.c common/sha256.c common/frame.c common/flashlog.c common/image_verify.c
                             common/boot_meta.c common/mode_fsm.c common/wdg_mgr.c common/fault_policy.c)
add_library(monocypher STATIC third_party/monocypher/monocypher.c third_party/monocypher/monocypher-ed25519.c)
add_library(freertos STATIC third_party/freertos/tasks.c third_party/freertos/queue.c third_party/freertos/list.c
                            third_party/freertos/timers.c third_party/freertos/event_groups.c third_party/freertos/stream_buffer.c
                            third_party/freertos/portable/ARM_CM4F/port.c third_party/freertos/portable/MemMang/heap_4.c)

foreach(t bsp drivers fc_common monocypher freertos)
  target_compile_definitions(${t} PRIVATE ${COMMON_DEFS})
  target_compile_options(${t} PRIVATE ${COMMON_OPTS})
  target_include_directories(${t} SYSTEM PRIVATE ${VENDOR_INC})
  target_include_directories(${t} PRIVATE ${OWN_INC})
endforeach()
foreach(t bsp drivers fc_common)
  target_compile_options(${t} PRIVATE ${WARN_OPTS})
endforeach()
target_include_directories(drivers PRIVATE ${ROOT}/app/inc)
target_include_directories(freertos SYSTEM PUBLIC ${ROOT}/third_party/freertos/include ${ROOT}/third_party/freertos/portable/ARM_CM4F)
target_include_directories(freertos PRIVATE ${ROOT}/app/inc)
target_compile_options(freertos PRIVATE -w)           # vendor code: not held to our warning policy

function(link_fw tgt ldscript)
  target_link_options(${tgt} PRIVATE ${CPU_FLAGS} -T${ROOT}/bsp/${ldscript} -L${ROOT}/bsp -nostartfiles
                      -specs=nano.specs -specs=nosys.specs -Wl,--gc-sections -Wl,-Map=${CMAKE_BINARY_DIR}/${tgt}.map -Wl,--print-memory-usage)
  target_link_libraries(${tgt} PRIVATE -Wl,--start-group bsp drivers fc_common monocypher m -Wl,--end-group)
  set_target_properties(${tgt} PROPERTIES LINK_DEPENDS ${ROOT}/bsp/${ldscript} SUFFIX ".elf")
  add_custom_command(TARGET ${tgt} POST_BUILD COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${tgt}> ${CMAKE_BINARY_DIR}/${tgt}.bin
                     COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${tgt}> VERBATIM)
endfunction()

# ---------------------------------------------------------------- bootloader
add_executable(bootloader bootloader/src/main.c)
target_compile_definitions(bootloader PRIVATE ${COMMON_DEFS})
target_compile_options(bootloader PRIVATE ${COMMON_OPTS} ${WARN_OPTS})
target_include_directories(bootloader SYSTEM PRIVATE ${VENDOR_INC})
target_include_directories(bootloader PRIVATE ${OWN_INC} ${ROOT}/bootloader/inc)
link_fw(bootloader bootloader.ld)

# ---------------------------------------------------------------- application (built for slot A and slot B)
set(APP_SRC app/src/main.c app/src/tasks.c app/src/comms.c app/src/housekeeping.c app/src/sensors.c
            app/src/latency.c app/src/irq.c app/src/kalman.c app/src/sim_sensors.c)

# add_app(<name> <slot A|B> <version> <sec_counter> [extra definitions...])
function(add_app name slot version sec)
  string(REPLACE "." ";" VL ${version})
  list(GET VL 0 a)
  list(GET VL 1 b)
  list(GET VL 2 c)
  math(EXPR vnum "(${a} << 24) | (${b} << 16) | ${c}")
  add_executable(${name} ${APP_SRC})
  target_compile_definitions(${name} PRIVATE ${COMMON_DEFS} FC_VERSION=${vnum}u FC_SECURITY_COUNTER=${sec}u
                             FC_ENABLE_FAULT_INJECTION=$<BOOL:${FC_ENABLE_FAULT_INJECTION}> ${ARGN})
  target_compile_options(${name} PRIVATE ${COMMON_OPTS} ${WARN_OPTS})
  target_include_directories(${name} SYSTEM PRIVATE ${VENDOR_INC})
  target_include_directories(${name} PRIVATE ${OWN_INC} ${ROOT}/app/inc)
  target_link_libraries(${name} PRIVATE freertos)
  if(slot STREQUAL "A")
    link_fw(${name} app_slot_a.ld)
  else()
    link_fw(${name} app_slot_b.ld)
  endif()
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${Python3_EXECUTABLE} ${ROOT}/tools/fw_packager.py --bin ${CMAKE_BINARY_DIR}/${name}.bin --slot ${slot}
            --version ${version} --sec-counter ${sec} --out ${CMAKE_BINARY_DIR}/${name}.signed.bin VERBATIM)
endfunction()

add_app(app_a A ${FC_FW_VERSION} 1)                                   # golden image, slot A
add_app(app_b B ${FC_FW_VERSION} 1)                                   # same version in slot B (for A/B tests)
add_app(app_v2_b B 1.1.0 2)                                           # "OTA update": newer, good
add_app(app_v2_a A 1.1.0 2)
add_app(app_bad_b B 1.2.0 3 FC_DEMO_CRASH_ON_BOOT=1)                  # broken update: crashes, never confirms -> rollback
add_app(app_bad_a A 1.2.0 3 FC_DEMO_CRASH_ON_BOOT=1)

# ---------------------------------------------------------------- CAN power node (second machine in Renode)
add_executable(power_node node/main.c)
target_compile_definitions(power_node PRIVATE ${COMMON_DEFS})
target_compile_options(power_node PRIVATE ${COMMON_OPTS} ${WARN_OPTS})
target_include_directories(power_node SYSTEM PRIVATE ${VENDOR_INC})
target_include_directories(power_node PRIVATE ${OWN_INC} ${ROOT}/app/inc)
link_fw(power_node node.ld)

# negative-test images (deliberately corrupted) derived from the good v2 image
foreach(kind payload signature header)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/bad_${kind}_b.signed.bin
    COMMAND ${Python3_EXECUTABLE} ${ROOT}/tools/fw_packager.py --bin ${CMAKE_BINARY_DIR}/app_v2_b.bin --slot B --version 1.1.0
            --sec-counter 2 --corrupt ${kind} --out ${CMAKE_BINARY_DIR}/bad_${kind}_b.signed.bin
    DEPENDS app_v2_b VERBATIM)
  list(APPEND BAD_IMAGES ${CMAKE_BINARY_DIR}/bad_${kind}_b.signed.bin)
endforeach()
add_custom_target(negative_images ALL DEPENDS ${BAD_IMAGES})
