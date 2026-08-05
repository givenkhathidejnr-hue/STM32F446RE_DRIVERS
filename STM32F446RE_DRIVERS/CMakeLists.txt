cmake_minimum_required(VERSION 4.1)
project(Project_1 C)

set(CMAKE_C_STANDARD 11)

# NOTE: this CMakeLists.txt exists only for IDE indexing/code-completion
# (e.g. CLion). It does NOT cross-compile or link a flashable firmware
# image - use `make` (see ./Makefile) for that; it drives arm-none-eabi-gcc
# with the correct -mcpu/-mfpu flags, the custom startup file/linker
# script, and recursively discovers every source file under SRCDIRS, so its
# file list never goes stale the way this one previously did (it used to
# reference drivers/headers/timer.h et al, a path that hasn't existed since
# the timers/ subfolder split, and was missing the dma/adc drivers and most
# of the timer subsystem entirely).

include_directories(app)
include_directories(CMSIS/Core)
include_directories(CMSIS/Core/Include)
include_directories(CMSIS/Device/ST/STM32F4xx/Include)
include_directories(drivers/headers)
include_directories(drivers/headers/timers)
include_directories(platform)
include_directories(system)

add_executable(Project_1
        app/main.c
        app/main.h
        CMSIS/Core/Include/cmsis_compiler.h
        CMSIS/Core/Include/cmsis_gcc.h
        CMSIS/Core/Include/cmsis_version.h
        CMSIS/Core/Include/core_cm4.h
        CMSIS/Core/Include/mpu_armv7.h
        CMSIS/Device/ST/STM32F4xx/Include/stm32f446xx.h
        CMSIS/Device/ST/STM32F4xx/Include/stm32f4xx.h
        CMSIS/Device/ST/STM32F4xx/Include/system_stm32f4xx.h
        drivers/headers/adc.h
        drivers/headers/button.h
        drivers/headers/device.h
        drivers/headers/dma.h
        drivers/headers/exti.h
        drivers/headers/gpio.h
        drivers/headers/nvic.h
        drivers/headers/pwr.h
        drivers/headers/rcc.h
        drivers/headers/systick.h
        drivers/headers/timers/timer.h
        drivers/headers/timers/timer_advanced.h
        drivers/headers/timers/timer_encoder.h
        drivers/headers/timers/timer_irq.h
        drivers/headers/timers/timer_motor.h
        drivers/headers/timers/timer_pwm.h
        drivers/headers/timers/timer_pwm_dma.h
        drivers/headers/timers/timer_sync.h
        drivers/sources/adc.c
        drivers/sources/button.c
        drivers/sources/dma.c
        drivers/sources/exti.c
        drivers/sources/exti_irq.c
        drivers/sources/gpio.c
        drivers/sources/nvic.c
        drivers/sources/pwr.c
        drivers/sources/rcc.c
        drivers/sources/systick.c
        drivers/sources/timers/timer.c
        drivers/sources/timers/timer_advanced.c
        drivers/sources/timers/timer_encoder.c
        drivers/sources/timers/timer_irq.c
        drivers/sources/timers/timer_irq_handlers.c
        drivers/sources/timers/timer_motor.c
        drivers/sources/timers/timer_pwm.c
        drivers/sources/timers/timer_pwm_dma.c
        drivers/sources/timers/timer_sync.c
        exceptions/hardfault_handler.c
        platform/systeminit.c
        platform/systeminit.h
        system/init.c
        system/init.h)
