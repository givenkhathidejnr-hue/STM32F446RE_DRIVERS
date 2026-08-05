/**
 * @file systeminit.c
 * @brief Board clock bring-up (SystemInit, called from the startup file
 *        before main()) and clock-tree introspection (SystemCoreClockUpdate,
 *        which back-calculates every bus frequency from live RCC register
 *        state rather than assuming compile-time constants).
 * @note  HSE_CLOCK here is this specific board's crystal frequency (8MHz) -
 *        if you reuse this file on different hardware, update it to match
 *        that board's actual HSE, or SystemCoreClockUpdate()'s PLL branch
 *        will silently compute a wrong SystemCoreClock (and everything
 *        downstream that derives timing from it - SysTick, timer
 *        prescalers, UART baud rates - will be wrong too).
 */
#include "systeminit.h"

/* Define constants*/
#define     HSE_CLOCK   8000000UL
#define     HSI_CLOCK   16000000UL

/*Global CMSIS variable SystemCoreClock*/
__DATA__    uint32_t        SystemCoreClock ;
__CLOCK__   clock_info_t    clock_info;

/* internal function helpers*/
/* configure bus prescalers to following values : typically AHB /1, APB1 /1, APB2 /1*/
__STATIC_FORCEINLINE void config_prescalers(uint32_t hpre, uint32_t ppre1, uint32_t ppre2)  {
    const RCC_Prescaler_Config prescaler_cfg = {
                    .HPRE  = hpre,        
                    .PPRE1 = ppre1,        
                    .PPRE2 = ppre2,       
    };

    /* RCC function used to modify prescaler registers*/
    rcc_set_prescalers(&prescaler_cfg);
}

/*Error handler function prototype*/
static void error_handler(void);

/*System clock upddate function*/
void SystemCoreClockUpdate(void)
{
    RCC_ClockSource sw  = rcc_get_sysclock();
    uint32_t pllp       = (RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos;
    pllp                = (pllp + 1) * 2;  // 00=/2, 01=/4, 10=/6, 11=/8

    /* Sysclock prescaler values*/
    static const uint16_t sysclock_prescalers[] =   {2, 4, 8, 16, 64, 128, 256, 512};

    uint32_t oscillator_clock;
    uint32_t plln, pllm, src;

    switch(sw)  {
        case RCC_PLL: // PLL
            plln   = (RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos;
            pllm   = (RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos;
            src    = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) ? HSE_CLOCK : HSI_CLOCK;
            oscillator_clock = ((uint64_t)src * plln) / (pllm * pllp);
            break;
        case RCC_HSE: 
            oscillator_clock = HSE_CLOCK; break;    // HSE
        default:
            oscillator_clock = HSI_CLOCK; break;    // HSI
    }
    clock_info.sysclock = oscillator_clock;

    /*Get prescaler values from RCC clock configuration register (RCC_CFGR)*/
    uint32_t hpre = (RCC -> CFGR & (RCC_CFGR_HPRE)) >> RCC_CFGR_HPRE_Pos;
    if (hpre < 8)     SystemCoreClock = oscillator_clock;
    else    {
        SystemCoreClock = oscillator_clock / sysclock_prescalers[hpre-8];
    }
    clock_info.hclk = SystemCoreClock;

    /* 3. APB1 prescaler → PCLK1 */
    static const uint8_t apb_presc_table[4] = {2, 4, 8, 16};

    uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;

    if (ppre1 < 4)
    {
        clock_info.pclk_1 = SystemCoreClock;
        clock_info.apb1_timer_clk = clock_info.pclk_1;;
    }
    else
    {
        clock_info.pclk_1 = SystemCoreClock / apb_presc_table[ppre1 - 4];
        clock_info.apb1_timer_clk = clock_info.pclk_1 * 2;
    }


    /* 4. APB2 prescaler → PCLK2 */
    uint32_t ppre2 = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;

    if (ppre2 < 4)
    {
        clock_info.pclk_2 = SystemCoreClock;
        clock_info.apb2_timer_clk = clock_info.pclk_2;
    }
    else
    {
        clock_info.pclk_2 = SystemCoreClock / apb_presc_table[ppre2 - 4];
        clock_info.apb2_timer_clk = clock_info.pclk_2 * 2;
    }
}

/**
 * @brief Called once from the startup file, before main() and before
 *        interrupts are globally re-enabled (see Reset_Handler in
 *        platform/stm32f446_startup.s: cpsid i ... SystemInit() ... cpsie
 *        i ... main()). Brings the core up from its reset clock (HSI
 *        16MHz) to the full 120MHz PLL/HSE configuration.
 * @note  Ordering here is load-bearing, not arbitrary: voltage scale is
 *        raised, then Flash latency/caches are set, BEFORE the clock
 *        actually speeds up (both must be true - the regulator must be
 *        able to supply the core at the target frequency, and Flash must
 *        insert enough wait states for the new frequency - before you
 *        switch SYSCLK to it, not after).
 */
void SystemInit (void)      {
    /* Disable all interrupts til the system initialization is done*/
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    /*1. Enable the Floating Point Unit (FPU) if using floating-point operations.*/
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
        SCB->CPACR |= ((3UL << 20) | (3UL << 22));
    #endif

    /* 2. Enable PWR clock */
    if (rcc_clock_control(RCC_PWR, RCC_CLOCK_ENABLE) != RCC_OK)     {
        error_handler();
    }
    
    /* 3. Set voltage scale to 1 and wait for ready */
    if (pwr_select_voltage_scale(PWR_VOLTAGE_SCALE_1) != PWR_OK)    { 
        error_handler();
    }

    /* 4. Configure Flash latency (wait states), and enable instruction cache, data cache, and prefetch.*/
    FLASH->ACR |= FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_3WS;
    /* Ensures flash timing active before frequency increase. */
    __DSB();
    __ISB();

    /* 5. Enable the HSE oscillator and wait until it is stable.*/
    if (rcc_enable_hse() != RCC_OK)   {
        error_handler();
    }

    /* 6. Configure PLL (from HSE)   */
    const RCC_PLLConfig pll_cfg = {
            .PLLM   = 8,                    // 8 MHz / 8 = 1 MHz (PLL input)
            .PLLN   = 240,                  // 1 MHz × 240 = 240 MHz (VCO output)
            .PLLP   = RCC_PLLP_DIV2,        // 240 MHz / 2 = 120 MHz → SYSCLK
            .PLLSRC = RCC_PLLSRC_HSE,       // Use HSE as source
            .PLLQ   = 8,                    // 240 MHz / 8 = 30 MHz → good for USB/RNG if needed
            .PLLR   = 7,                    // Optional: for I2S clocks (not critical if unused)
    };

    if (rcc_config_pll(&pll_cfg) != RCC_OK)   {
        error_handler();
    }

    /* Reset the TIMPRE bit in RCC_DCKCFGR such that TIM_CLK = 2xPCLK if APB_prescaler > 1*/
    RCC->DCKCFGR   &=   ~RCC_DCKCFGR_TIMPRE;

    /* 7. Set AHB and APB bus prescalers.*/
    config_prescalers(RCC_CFGR_HPRE_DIV1, RCC_CFGR_PPRE1_DIV4, RCC_CFGR_PPRE2_DIV2);

    /* 8. Switch to the PLL clock */
    if (rcc_set_sysclock(RCC_PLL) != RCC_OK)    {
        error_handler();
    }

    /*9. Update the SystemCoreClock variable to reflect the new core frequency.*/
    SystemCoreClockUpdate();  

    /* Ensure all memory writes are done */
    __DSB();
    __ISB();   

    /*9. restore interrupts */
    __set_PRIMASK(primask);
}

/*=======================================================================================================================================*/
/*=======================================================================================================================================*/
/*Error handler*/
static void error_handler(void)    {
    while(1)    { __NOP(); }
}