#ifndef __RCC_H
#define __RCC_H

#include <stdint.h>
#include "device.h"

/**
 * @file rcc.h
 * @brief RCC (Reset and Clock Control) driver: oscillators, PLL, bus
 *        prescalers, system clock source selection, per-peripheral clock
 *        gating/reset.
 * @note  Typical bring-up order: rcc_enable_hse() (or _hsi) -> optionally
 *        rcc_hse_bypass_enable() if using an external oscillator rather
 *        than a crystal -> rcc_config_pll() -> rcc_set_prescalers() (AHB/
 *        APB1/APB2, BEFORE switching to a faster source) -> rcc_enable_pll()
 *        (done implicitly by rcc_set_sysclock(RCC_PLL)) -> rcc_set_sysclock().
 *        See platform/systeminit.c's SystemInit() for the actual sequence
 *        used at boot, including the Flash-latency step that must happen
 *        before the clock speeds up.
 */
/*RCC handle drivers*/
/* =====================================================================
* Status codes
* ===================================================================== */
typedef enum
{
    RCC_OK,
    RCC_INVALID_PARAM,
    RCC_TIMEOUT
} RCC_Status;

/* =====================================================================
* Clock sources
* ===================================================================== */
typedef enum
{
    RCC_HSI = 0,
    RCC_HSE = 1,
    RCC_PLL = 2
} RCC_ClockSource;


/* =====================================================================
* Peripheral clock enable/disable
* ===================================================================== */
typedef enum
{
    RCC_CLOCK_DISABLE = 0,
    RCC_CLOCK_ENABLE,
    RCC_CLOCK_RESET
} RCC_ClockState_t;

/* ========================================================================== */
/* PLL configuration */
/* ========================================================================== */

/*Phase Lock Loop clock souce-  (0: HSI, 1: HSE)*/
typedef enum {
    RCC_PLLSRC_HSI = 0U,
    RCC_PLLSRC_HSE = 1U
} RCC_PLLSource;

/*PLLP division factors - main system clock*/
typedef enum {
    RCC_PLLP_DIV2 = 0,
    RCC_PLLP_DIV4 = 1,
    RCC_PLLP_DIV6 = 2,
    RCC_PLLP_DIV8 = 3
} RCC_PLLP_Div;

/* =====================================================================
* PLL configuration structure
* ===================================================================== */
/*
    •f(VCO clock) = f(PLL clock input) × (PLLN / PLLM)
    •f(PLL general clock output) = f(VCO clock) / PLLP
    •f(USB OTG FS, SDIO) = f(VCO clock) / PLLQ
*/
typedef struct    {
    uint8_t         PLLM;                           /*Division factor for main PLL input clock (2 <= PLLM <= 63)*/
    uint16_t        PLLN;                           /*Multiplication factor for the main PLL VCO (50 <= PLLN <= 432)*/
    RCC_PLLP_Div    PLLP;                           /*Division factor for the main system clock (PLLP = 2, 4, 6, or 8)*/
    RCC_PLLSource   PLLSRC;                         /*Main PLL and audio PLL entry clock source (0: HSI, 1: HSE)*/
    uint8_t         PLLQ;                           /*Division factor for USB OTG FS and SDIOclock (2 <= PLLQ <= 15)*/
    uint8_t         PLLR;                           /*Division factor for I2Ss, SAIs, SYSTEM and SPDIF-Rx clocks (2 <= PLLR <= 7)*/
} RCC_PLLConfig;

/* ========================================================================== */
/* Prescalers */
/* ========================================================================== */
/*Prescaler for the core and peripheral buses*/
typedef struct  {
    uint32_t HPRE;                           /*AHB prescaler (1, 2, 4, 8, 16, 64, 128, 256, 512) using bits 4 - 7 */
    uint32_t PPRE1;                          /*APB Low speed prescaler (APB1 - 1, 2, 4, 8, 16) using bits 10 - 12*/
    uint32_t PPRE2;                          /*APB high-speed prescaler (APB2 - 1, 2, 4, 8, 16) using bits 13 - 15*/
} RCC_Prescaler_Config;

/* ========================================================================== */
/* Peripherals and ports*/
/* ========================================================================== */
/* =====================================================================
* Internal bus types (used by clock map)
* ===================================================================== */
typedef enum
{
    RCC_BUS_AHB1 = 0,
    RCC_BUS_AHB2,
    RCC_BUS_APB1,
    RCC_BUS_APB2
} RCC_Bus_t;

/* =====================================================================
* Peripheral clock descriptor (internal mapping)
* ===================================================================== */
typedef struct { 
    RCC_Bus_t bus; 
    uint32_t enable_bit; 
    volatile uint32_t *enable_reg; 
    volatile uint32_t *reset_reg; 
} RCC_ClockDesc_t;

typedef enum
{
    /*AHB1*/
    RCC_GPIOA,  RCC_GPIOB,  RCC_GPIOC,  RCC_GPIOD,
    RCC_GPIOE,  RCC_GPIOF,  RCC_GPIOG,  RCC_GPIOH,
    RCC_CRC,    RCC_BKPSRAM,RCC_DMA1,   RCC_DMA2,

    /* APB1 */
    RCC_TIM2,   RCC_TIM3,   RCC_TIM4,   RCC_TIM5,
    RCC_TIM6,   RCC_TIM7,   RCC_TIM12,  RCC_TIM13,
    RCC_TIM14,  RCC_WWDG,   RCC_SPI2,   RCC_SPI3,
    RCC_USART2, RCC_USART3, RCC_UART4,  RCC_UART5,
    RCC_I2C1,   RCC_I2C2,   RCC_I2C3,   RCC_PWR,
    RCC_DAC,

    /* APB2 */
    RCC_TIM1,   RCC_TIM8,   RCC_USART1, RCC_USART6,
    RCC_ADC1,   RCC_ADC2,   RCC_ADC3,   RCC_SPI1,
    RCC_SPI4,   RCC_SYSCFG, RCC_TIM9,   RCC_TIM10,
    RCC_TIM11,

    RCC_PERIPH_COUNT
} RCC_Peripheral_t;

/* ========================================================================== */
/* API */ /*Core RCC API functions*/
/* ========================================================================== */
/*1. Enable the HSE oscillator*/
RCC_Status rcc_enable_hse(void);

/*2. Enable the HSI oscillator*/
RCC_Status rcc_enable_hsi(void);

RCC_Status rcc_hse_bypass_enable(void);
RCC_Status rcc_hse_bypass_disable(void);

/*3. Configure and enable PLL*/
RCC_Status rcc_config_pll(const RCC_PLLConfig *cfg);
RCC_Status rcc_enable_pll(void);
RCC_Status rcc_disable_pll(void);

/*4. Set prescalers*/
void rcc_set_prescalers(const RCC_Prescaler_Config *cfg);

/*5. select system clock source*/
RCC_Status rcc_set_sysclock(RCC_ClockSource src);

/*6. GET system clock source*/
RCC_ClockSource rcc_get_sysclock(void);

/*7. Control clock over peripherals*/
RCC_Status rcc_clock_control(RCC_Peripheral_t peripheral, RCC_ClockState_t state);


#endif /* __RCC_H */





