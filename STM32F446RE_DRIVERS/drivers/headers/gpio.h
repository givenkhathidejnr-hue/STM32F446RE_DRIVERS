#ifndef __GPIO_H
#define __GPIO_H

#include <stdint.h>
#include "device.h"
#include "core_cm4.h"

/**
 * @file gpio.h
 * @brief GPIO configuration + fast bit-level I/O.
 * @note  gpio_init() only programs a pin - it does NOT enable that port's
 *        AHB1 clock (RCC_AHB1ENR_GPIOxEN); call rcc_clock_control(RCC_GPIOx,
 *        RCC_CLOCK_ENABLE) first, or every register write here silently
 *        does nothing (reads/writes to an unclocked peripheral's registers
 *        are undefined on Cortex-M - typically read as 0/ignored, but not
 *        guaranteed).
 * @note  STM32H755 port: GPIO's register layout (MODER/OTYPER/OSPEEDR/
 *        PUPDR/IDR/ODR/BSRR/AFR) is unchanged on H7 - this whole module
 *        should port with just a clock-enable-register name change (AHB1
 *        becomes AHB4 for GPIO on H7).
 */

/* ===================== Bit helpers ===================== */
#define GPIO_PIN_MASK(pin)      (1UL    << (pin))
#define GPIO_2BIT_MASK(pin)     (0x3UL  << ((pin) * 2U))
#define GPIO_4BIT_MASK(shift)   (0xFUL  << (shift))

/* ===================== Types ===================== */
typedef uint16_t gpio_pin_mask_t;   /* Bitmask: (1 << pin) */
typedef uint16_t  gpio_pin_t;        /* Pin index: 0–15 */


/* ===================== Pin masks ===================== */
#define GPIO_PIN(n)        (1U << (n))
#define HIGH                1UL
#define LOW                 0UL

/* ===================== GPIO mode ===================== */
typedef enum {
    GPIO_MODE_INPUT  = 0x0,
    GPIO_MODE_OUTPUT = 0x1,
    GPIO_MODE_AF     = 0x2,
    GPIO_MODE_ANALOG = 0x3,
} gpio_mode_t;

/* ===================== Output type ===================== */
typedef enum {
    GPIO_OTYPE_PP = 0x0,
    GPIO_OTYPE_OD = 0x1,
} gpio_otype_t;

/* ===================== Speed ===================== */
typedef enum {
    GPIO_SPEED_LOW    = 0x0,
    GPIO_SPEED_MEDIUM = 0x1,
    GPIO_SPEED_FAST   = 0x2,
    GPIO_SPEED_HIGH   = 0x3,
} gpio_speed_t;

/* ===================== Pull-up/down ===================== */
typedef enum {
    GPIO_NOPULL     = 0x0,
    GPIO_PULL_UP    = 0x1,
    GPIO_PULL_DOWN  = 0x2,
} gpio_pull_t;

/* ===================== Alternate function ===================== */
typedef enum {
    GPIO_AF0  = 0,  GPIO_AF1  = 1,  GPIO_AF2  = 2,  GPIO_AF3  = 3,
    GPIO_AF4  = 4,  GPIO_AF5  = 5,  GPIO_AF6  = 6,  GPIO_AF7  = 7,
    GPIO_AF8  = 8,  GPIO_AF9  = 9,  GPIO_AF10 = 10, GPIO_AF11 = 11,
    GPIO_AF12 = 12, GPIO_AF13 = 13, GPIO_AF14 = 14, GPIO_AF15 = 15,
} gpio_af_t;

/* ===================== GPIO configuration ===================== */
typedef struct {
    GPIO_TypeDef *port;
    gpio_pin_t    pins;
    gpio_mode_t   mode;
    gpio_otype_t  otype;
    gpio_speed_t  speed;
    gpio_pull_t   pull;
    gpio_af_t     af;
} gpio_config_t;


/* ===================== Fast I/O ===================== */
__STATIC_FORCEINLINE void gpio_set(GPIO_TypeDef *port, gpio_pin_mask_t pins)
{
    port->BSRR = pins;
}

__STATIC_FORCEINLINE void gpio_reset(GPIO_TypeDef *port, gpio_pin_mask_t pins)
{
    port->BSRR = (uint32_t)pins << 16;
}

__STATIC_FORCEINLINE void gpio_write(GPIO_TypeDef *port, gpio_pin_mask_t pins, uint8_t value)
{
    if (value) gpio_set(port, pins);
    else       gpio_reset(port, pins);
}

/**
 * Atomically toggles pins in a GPIO port using the Cortex-M exclusive
 * monitor (LDREX/STREX): read-modify-write ODR as one atomic transaction,
 * retrying if another bus master (another core, DMA, or an interrupt that
 * also writes ODR) touched the exclusive monitor in between.
 * @note  Prefer plain gpio_toggle() (a single non-atomic ODR read-modify-
 *        write) unless something else can genuinely race this exact ODR
 *        write - e.g. another interrupt at a different priority also
 *        toggling ODR bits on the same port. If only BSRR-style set/reset
 *        is needed, gpio_set()/gpio_reset() are already atomic (BSRR is a
 *        write-only, bit-banded register) and cheaper than either toggle.
 */
__STATIC_FORCEINLINE void gpio_toggle_atomic(GPIO_TypeDef *port, gpio_pin_mask_t pins)
{
    uint32_t odr;

    do {
        // Exclusive load
        odr = __LDREXW(&port->ODR);
        odr ^= pins;  // toggle requested pins
    } while (__STREXW(odr, &port->ODR)); // Exclusive store, retry if failed

    __DMB(); // Data memory barrier to ensure write completes
}

/**
 * @brief Non-atomic ODR read-modify-write toggle.
 * @note  Safe from a single interrupt priority level / the main loop in
 *        isolation, but NOT safe if a higher-priority interrupt can
 *        preempt this exact read-modify-write and also touch ODR on the
 *        same port - use gpio_toggle_atomic() in that case.
 */
__STATIC_FORCEINLINE void gpio_toggle(GPIO_TypeDef *port, gpio_pin_mask_t pins)
{
    port->ODR ^= pins;
}

__STATIC_FORCEINLINE uint32_t gpio_read(GPIO_TypeDef *port)  
{
    return port->IDR;
}

__STATIC_FORCEINLINE uint8_t gpio_read_pin(GPIO_TypeDef *port, gpio_pin_mask_t pin)  
{
    return (port->IDR & pin);
}

/* ===================== API ===================== */
void gpio_init(const gpio_config_t *cfg);

#endif /* __GPIO_H */
