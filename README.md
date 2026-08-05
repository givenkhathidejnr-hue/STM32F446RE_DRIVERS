# STM32F446 Driver Library

A hand-rolled, register-level peripheral driver library for the STM32F446
(no HAL/LL dependency) plus a demo application exercising most of it
together on one board.

## Drivers

RCC (table-driven clock gating), GPIO, EXTI, NVIC, SysTick, PWR (sleep
mode), ADC (regular channels, DMA, overrun handling), DMA (stream/flag
management), debounced button input, and a timer subsystem split into
base/PWM/advanced (BDTR, complementary outputs, dead-time)/encoder &
input-capture/master-slave sync/motor-control layers.

## Layout

```
app/                 Demo application (main.c) - see its header comment
                      for wiring and what it exercises
drivers/headers/      Public API per peripheral
drivers/sources/      Implementations
drivers/*/timers/     Timer subsystem (base, PWM, advanced, encoder, sync, motor)
exceptions/           HardFault handler
platform/             Linker script, startup file, clock bring-up (SystemInit-equivalent)
system/               NVIC priority grouping + SysTick bring-up, called once from main()
CMSIS/                ARM CMSIS core + ST device headers (vendor, not modified)
os/                   Empty - reserved for a future RTOS integration
```

## Building

```sh
make            # -> bin/firmware.elf / .bin
make flash      # flash via st-flash
make gdb        # gdb-multiarch against the built ELF
make clean
```

Builds clean with `-Wall -Wextra -Wshadow` (zero warnings) using
`arm-none-eabi-gcc`.
