#include "gpio.h"

/* ===================== Public API ===================== */
/**
 * @brief Initialize a GPIO pin
 * @note  The corresponding GPIO clock MUST be enabled before calling this function
 *        using rcc_clock_control()
 */

 /* ===================== Internal helpers ===================== */

/**
 * @brief Expand a 16-bit pin bitmask into the corresponding 32-bit mask for
 *        a 2-bits-per-pin register (MODER, OSPEEDR, PUPDR).
 * @note  __builtin_ctz(pins) finds the lowest set pin, and "pins &= pins-1"
 *        clears it - a standard bit-scan loop that visits only the pins
 *        that are actually set, rather than testing all 16 every time.
 */
__STATIC_INLINE uint32_t gpio_expand_2bit_mask(uint16_t pins)
{
    uint32_t mask = 0;

    while (pins) {
        uint32_t pin = __builtin_ctz(pins);
        mask |= (0x3u << (pin * 2));
        pins &= pins - 1;
    }

    return mask;
}

/**
 * @brief Expand a 16-bit pin bitmask into the corresponding 32-bit mask for
 *        a 4-bits-per-pin register half (AFR[0] = pins 0-7, AFR[1] = pins
 *        8-15). @p base is 0 or 8, selecting which half to consider.
 */
__STATIC_INLINE uint32_t gpio_expand_4bit_mask(uint16_t pins, uint8_t base)
{
    uint32_t mask = 0;

    pins >>= base;
    pins &= 0xFF;

    while (pins) {
        uint32_t pin = __builtin_ctz(pins);
        mask |= (0xFu << (pin * 4));
        pins &= pins - 1;
    }

    return mask;
}

/* ===================== Configuration ===================== */

/**
 * @brief Configure one or more pins on a single port with identical
 *        mode/otype/speed/pull/af settings.
 * @note  Every field of gpio_config_t applies to ALL pins in cfg->pins at
 *        once - to give two pins on the same port different settings
 *        (e.g. one output, one analog input), call gpio_init() once per
 *        pin/group rather than trying to pack them into one call.
 */
void gpio_init(const gpio_config_t *cfg)
{
    if (!cfg || !cfg->port || !cfg->pins)
        return;

    GPIO_TypeDef *gpio = cfg->port;
    gpio_pin_mask_t pins = cfg->pins;

    /* Mode (MODER, 2 bits/pin: 00=input 01=output 10=AF 11=analog) */
    uint32_t mask2 = gpio_expand_2bit_mask(pins);
    gpio->MODER &= ~mask2;

    for (uint32_t p = pins; p; p &= p - 1) {
        uint32_t pin = __builtin_ctz(p);
        gpio->MODER |= (cfg->mode << (pin * 2));
    }

    /* Output / AF config */
    if (cfg->mode == GPIO_MODE_OUTPUT || cfg->mode == GPIO_MODE_AF) {

        gpio->OTYPER &= ~pins;
        gpio->OTYPER |= (cfg->otype ? pins : 0);

        gpio->OSPEEDR &= ~mask2;
        for (uint32_t p = pins; p; p &= p - 1) {
            uint32_t pin = __builtin_ctz(p);
            gpio->OSPEEDR |= (cfg->speed << (pin * 2));
        }
    }

    /* Pull */
    if (cfg->mode == GPIO_MODE_INPUT || cfg->mode == GPIO_MODE_AF) {
        gpio->PUPDR &= ~mask2;
        for (uint32_t p = pins; p; p &= p - 1) {
            uint32_t pin = __builtin_ctz(p);
            gpio->PUPDR |= (cfg->pull << (pin * 2));
        }
    }

    /* AF */
    if (cfg->mode == GPIO_MODE_AF) {

        uint32_t m;

        m = gpio_expand_4bit_mask(pins, 0);
        gpio->AFR[0] &= ~m;

        for (uint32_t p = pins & 0xFF; p; p &= p - 1) {
            uint32_t pin = __builtin_ctz(p);
            gpio->AFR[0] |= (cfg->af << (pin * 4));
        }

        m = gpio_expand_4bit_mask(pins, 8);
        gpio->AFR[1] &= ~m;

        for (uint32_t p = (pins >> 8) & 0xFF; p; p &= p - 1) {
            uint32_t pin = __builtin_ctz(p);
            gpio->AFR[1] |= (cfg->af << (pin * 4));
        }
    }
}

