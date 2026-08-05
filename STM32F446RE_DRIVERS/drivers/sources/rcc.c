/**
 * @file rcc.c
 * @brief Clock tree control: oscillators (HSE/HSI), PLL, bus prescalers,
 *        system clock mux, and per-peripheral clock gating.
 * @note  STM32H755 port: this whole file's approach (one flat AHB1/APB1/
 *        APB2-style enable-register map) does not carry over. H7 splits
 *        peripherals across three power/clock domains (D1/D2/D3), each
 *        with its own enable registers (AHB1ENR/AHB3ENR/... are joined by
 *        D2CCIP*R "kernel clock" muxes that pick which clock source feeds
 *        each peripheral - e.g. USART1 can run from PCLK2, HSI, CSI or
 *        LSE). rcc_clock_control()'s table-driven design pattern is worth
 *        keeping; the table contents and RCC_Bus_t enum need a full
 *        rewrite. See PORTING_NOTES_H755.md.
 */
#include "rcc.h"

/* =====================================================================
* User configuration
* =====================================================================*/
#ifndef RCC_TIMEOUT_DELAY
    #define RCC_TIMEOUT_DELAY 0x000FFFFFUL   /* ~1M polls - not a calibrated
                                               * time bound, just "long enough
                                               * to not spin forever on a dead
                                               * oscillator". Override before
                                               * #include if you need a real
                                               * timed bound (e.g. tied to a
                                               * known core clock). */
#endif

/* =====================================================================
* RCC Peripheral Clock Map
* =====================================================================
* This table is the SINGLE SOURCE OF TRUTH for:
* - Which bus a peripheral belongs to
* - Which enable bit controls its clock
* =====================================================================
*/
static RCC_ClockDesc_t rcc_clock_map[RCC_PERIPH_COUNT] =
{
    /* =====================
    * AHB1 peripherals
    * ===================== */
    [RCC_GPIOA]     = { RCC_BUS_AHB1, RCC_AHB1ENR_GPIOAEN,  &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_GPIOB]     = { RCC_BUS_AHB1, RCC_AHB1ENR_GPIOBEN,  &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_GPIOC]     = { RCC_BUS_AHB1, RCC_AHB1ENR_GPIOCEN,  &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_GPIOD]     = { RCC_BUS_AHB1, RCC_AHB1ENR_GPIODEN,  &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_GPIOE]     = { RCC_BUS_AHB1, RCC_AHB1ENR_GPIOEEN,  &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_GPIOF]     = { RCC_BUS_AHB1, RCC_AHB1ENR_GPIOFEN,  &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_GPIOG]     = { RCC_BUS_AHB1, RCC_AHB1ENR_GPIOGEN,  &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_GPIOH]     = { RCC_BUS_AHB1, RCC_AHB1ENR_GPIOHEN,  &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_CRC]       = { RCC_BUS_AHB1, RCC_AHB1ENR_CRCEN,    &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_BKPSRAM]   = { RCC_BUS_AHB1, RCC_AHB1ENR_BKPSRAMEN,&RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_DMA1]      = { RCC_BUS_AHB1, RCC_AHB1ENR_DMA1EN,   &RCC->AHB1ENR, &RCC->AHB1RSTR   },
    [RCC_DMA2]      = { RCC_BUS_AHB1, RCC_AHB1ENR_DMA2EN,   &RCC->AHB1ENR, &RCC->AHB1RSTR   },

    /* =====================
    * APB1 peripherals
    * ===================== */
    [RCC_TIM2]      = { RCC_BUS_APB1, RCC_APB1ENR_TIM2EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_TIM3]      = { RCC_BUS_APB1, RCC_APB1ENR_TIM3EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_TIM4]      = { RCC_BUS_APB1, RCC_APB1ENR_TIM4EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_TIM5]      = { RCC_BUS_APB1, RCC_APB1ENR_TIM5EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_TIM6]      = { RCC_BUS_APB1, RCC_APB1ENR_TIM6EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_TIM7]      = { RCC_BUS_APB1, RCC_APB1ENR_TIM7EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_TIM12]     = { RCC_BUS_APB1, RCC_APB1ENR_TIM12EN,  &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_TIM13]     = { RCC_BUS_APB1, RCC_APB1ENR_TIM13EN,  &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_TIM14]     = { RCC_BUS_APB1, RCC_APB1ENR_TIM14EN,  &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_WWDG]      = { RCC_BUS_APB1, RCC_APB1ENR_WWDGEN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_SPI2]      = { RCC_BUS_APB1, RCC_APB1ENR_SPI2EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_SPI3]      = { RCC_BUS_APB1, RCC_APB1ENR_SPI3EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_USART2]    = { RCC_BUS_APB1, RCC_APB1ENR_USART2EN, &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_USART3]    = { RCC_BUS_APB1, RCC_APB1ENR_USART3EN, &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_UART4]     = { RCC_BUS_APB1, RCC_APB1ENR_UART4EN,  &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_UART5]     = { RCC_BUS_APB1, RCC_APB1ENR_UART5EN,  &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_I2C1]      = { RCC_BUS_APB1, RCC_APB1ENR_I2C1EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_I2C2]      = { RCC_BUS_APB1, RCC_APB1ENR_I2C2EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_I2C3]      = { RCC_BUS_APB1, RCC_APB1ENR_I2C3EN,   &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_PWR]       = { RCC_BUS_APB1, RCC_APB1ENR_PWREN,    &RCC->APB1ENR, &RCC->APB1RSTR   },
    [RCC_DAC]       = { RCC_BUS_APB1, RCC_APB1ENR_DACEN,    &RCC->APB1ENR, &RCC->APB1RSTR   },

    /* =====================
    * APB2 peripherals
    * ===================== */
    [RCC_TIM1]      = { RCC_BUS_APB2, RCC_APB2ENR_TIM1EN,   &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_TIM8]      = { RCC_BUS_APB2, RCC_APB2ENR_TIM8EN,   &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_USART1]    = { RCC_BUS_APB2, RCC_APB2ENR_USART1EN, &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_USART6]    = { RCC_BUS_APB2, RCC_APB2ENR_USART6EN, &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_ADC1]      = { RCC_BUS_APB2, RCC_APB2ENR_ADC1EN,   &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_ADC2]      = { RCC_BUS_APB2, RCC_APB2ENR_ADC2EN,   &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_ADC3]      = { RCC_BUS_APB2, RCC_APB2ENR_ADC3EN,   &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_SPI1]      = { RCC_BUS_APB2, RCC_APB2ENR_SPI1EN,   &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_SPI4]      = { RCC_BUS_APB2, RCC_APB2ENR_SPI4EN,   &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_SYSCFG]    = { RCC_BUS_APB2, RCC_APB2ENR_SYSCFGEN, &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_TIM9]      = { RCC_BUS_APB2, RCC_APB2ENR_TIM9EN,   &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_TIM10]     = { RCC_BUS_APB2, RCC_APB2ENR_TIM10EN,  &RCC->APB2ENR, &RCC->APB2RSTR   },
    [RCC_TIM11]     = { RCC_BUS_APB2, RCC_APB2ENR_TIM11EN,  &RCC->APB2ENR, &RCC->APB2RSTR   }
};

/* ==============================
* Local helper function
* =============================== */
static RCC_Status rcc_wait_for_flag(volatile uint32_t *reg,
                                    uint32_t flag_mask,
                                    uint32_t expected_state)
{
    // 0x100000 is roughly 1 million loops.
    uint32_t timeout = RCC_TIMEOUT_DELAY; 

    while (timeout > 0) {
        if ((*reg & flag_mask) == expected_state) {
            return RCC_OK;
        }
        timeout--;
    }
    
    return RCC_TIMEOUT;
}

/*1. Enable the HSE oscillator*/
RCC_Status rcc_enable_hse(void)    {
    /*enable hse on the rcc control register*/
    RCC -> CR |= RCC_CR_HSEON;

    /*wait for the hse to be stable*/
    return rcc_wait_for_flag(&RCC->CR,
                            RCC_CR_HSERDY_Msk,
                            RCC_CR_HSERDY);
}


/*2. Enable the HSI oscillator*/
RCC_Status rcc_enable_hsi(void)    {
    /*enable hsi on the rcc control register*/
    RCC -> CR |= RCC_CR_HSION;

    /*wait for the hsi to be stable*/
    return rcc_wait_for_flag(&RCC->CR,
                            RCC_CR_HSIRDY_Msk,
                            RCC_CR_HSIRDY);
}

RCC_Status rcc_enable_pll(void)    {
    RCC -> CR |= RCC_CR_PLLON;
    
    return rcc_wait_for_flag(&RCC->CR,
                            RCC_CR_PLLRDY_Msk,
                            RCC_CR_PLLRDY);
}

RCC_Status rcc_disable_pll(void)   {
    RCC -> CR &= ~RCC_CR_PLLON;
    return rcc_wait_for_flag(&RCC->CR,
                            RCC_CR_PLLRDY_Msk,
                            0UL);
}

/*3. Configure PLL*/
RCC_Status rcc_config_pll(const RCC_PLLConfig *cfg)    {
    /*Assert the struct is not empty*/
    if (cfg == NULL)
        return RCC_INVALID_PARAM;
        
    /*Check is pll values are within correct boundaries*/
    if (cfg->PLLM < 2U  || cfg->PLLM > 63U  ||
        cfg->PLLN < 50U || cfg->PLLN > 432U ||
        cfg->PLLQ < 2U  || cfg->PLLQ > 15U  ||
        cfg->PLLR < 2U  || cfg->PLLR > 7U   ||
        cfg->PLLP > RCC_PLLP_DIV8)
        return RCC_INVALID_PARAM;

    /*Disable pll*/
    if (rcc_disable_pll() != RCC_OK)     return RCC_TIMEOUT;

    /* Configure PLL */
    RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLM_Msk |
                    RCC_PLLCFGR_PLLN_Msk   |
                    RCC_PLLCFGR_PLLP_Msk   |
                    RCC_PLLCFGR_PLLSRC_Msk |
                    RCC_PLLCFGR_PLLQ_Msk   |
                    RCC_PLLCFGR_PLLR_Msk);

    /*Set the PLL values in the PLLCFGR*/
    RCC->PLLCFGR |= ((uint32_t)cfg->PLLM << RCC_PLLCFGR_PLLM_Pos)       |
                    ((uint32_t)cfg->PLLN << RCC_PLLCFGR_PLLN_Pos)       |
                    ((uint32_t)cfg->PLLP << RCC_PLLCFGR_PLLP_Pos)       |
                    ((uint32_t)cfg->PLLSRC << RCC_PLLCFGR_PLLSRC_Pos)   |
                    ((uint32_t)cfg->PLLQ << RCC_PLLCFGR_PLLQ_Pos)       |
                    ((uint32_t)cfg->PLLR << RCC_PLLCFGR_PLLR_Pos);
    return RCC_OK;    
}

/*4. Set prescalers*/
void rcc_set_prescalers(const RCC_Prescaler_Config *cfg) {
    RCC->CFGR &= ~(RCC_CFGR_HPRE_Msk   |
                RCC_CFGR_PPRE1_Msk     |
                RCC_CFGR_PPRE2_Msk);

    /*Configure the AHB and APB prescalers*/
    RCC->CFGR |= ((uint32_t)cfg->HPRE & RCC_CFGR_HPRE_Msk)  |
                ((uint32_t)cfg->PPRE1 & RCC_CFGR_PPRE1_Msk) |
                ((uint32_t)cfg->PPRE2 & RCC_CFGR_PPRE2_Msk);
}

/*5. select system clock source*/
RCC_Status rcc_set_sysclock(RCC_ClockSource src)   {
    RCC_Status status = RCC_OK;

    /* 1. Enable the requested source */
    switch (src)
    {
        case RCC_HSE:       status = rcc_enable_hse();       break;
        case RCC_HSI:       status = rcc_enable_hsi();       break;
        case RCC_PLL:       status = rcc_enable_pll();       break;
        default:            return RCC_INVALID_PARAM;  // Invalid source
    }  
    /* 2. check the status*/
    if (status != RCC_OK)       return status;

    /* 3. Switch the system clock source */
    RCC->CFGR &= ~RCC_CFGR_SW_Msk;
    RCC->CFGR |= ((uint32_t)src << RCC_CFGR_SW_Pos);

    /* 4. Wait for switch to complete*/
    return rcc_wait_for_flag(&RCC->CFGR,
                            RCC_CFGR_SWS_Msk,
                            ((uint32_t)src << RCC_CFGR_SWS_Pos));
}

/*6. Get system clock source*/
RCC_ClockSource rcc_get_sysclock(void)   {
    /*Get the clock source from the system clock switch status bits*/
    switch(RCC->CFGR & RCC_CFGR_SWS_Msk)    
    {
        case RCC_CFGR_SWS_HSI:     return RCC_HSI;
        case RCC_CFGR_SWS_HSE:     return RCC_HSE;
        case RCC_CFGR_SWS_PLL:     return RCC_PLL;
        default:    return RCC_HSI;
    }
}

RCC_Status rcc_clock_control(RCC_Peripheral_t peripheral, RCC_ClockState_t state)
{
    if (peripheral >= RCC_PERIPH_COUNT)  
        return RCC_INVALID_PARAM;

    RCC_ClockDesc_t *desc = &rcc_clock_map[peripheral];

    switch (state)
    {
        case RCC_CLOCK_ENABLE:      *desc->enable_reg |= desc->enable_bit;  break;
        case RCC_CLOCK_DISABLE:     *desc->enable_reg &= ~desc->enable_bit; break;
        case RCC_CLOCK_RESET:
            /* The peripheral's bit position happens to be identical in its
             * *ENR (clock enable) and *RSTR (peripheral reset) registers -
             * that's why desc->enable_bit is reused here instead of the
             * descriptor needing a separate reset-bit field. Assert then
             * immediately release: a synchronous reset pulse, not a
             * held-in-reset state. */
            *desc->reset_reg |= desc->enable_bit; // assert reset
            *desc->reset_reg &= ~desc->enable_bit; // release reset
            break;
        default:                    return RCC_INVALID_PARAM;
    }

    __DSB(); // Ensure memory sync - guarantees the write above has actually
              // reached the peripheral before this function returns, so the
              // caller can safely touch that peripheral's other registers
              // immediately afterward.
    return RCC_OK;
}

