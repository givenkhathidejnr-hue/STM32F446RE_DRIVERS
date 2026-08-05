#include "pwr.h"

#define PWR_TIMEOUT_DELAY 0xFFFFFFUL

static PWR_Status pwr_wait_flag(uint32_t flag, uint32_t expected)
{
    uint32_t delay = 0;
    while (((PWR->CSR & flag) ? 1 : 0) != expected) {
        delay++;
        if (delay >= PWR_TIMEOUT_DELAY )    return PWR_TIMEOUT;
    }
    return PWR_OK;
}

/* ========================================================================== */
 /*Core PWR FUNCTIONS*/
/* ========================================================================== */
/**
 * @brief Enable Over-drive mode - required to reach SYSCLK > 168MHz
 *        (up to 180MHz) at VOS scale 1. Must be enabled AFTER selecting
 *        voltage scale 1 (pwr_select_voltage_scale) and BEFORE switching
 *        SYSCLK to a PLL output above 168MHz.
 * @note  Unlike pwr_disable_overdrive(), this busy-waits unconditionally
 *        (no timeout) on both ODRDY and ODSWRDY - on real silicon these
 *        settle quickly, but a genuine hardware fault here hangs forever
 *        rather than returning PWR_TIMEOUT.
 */
PWR_Status pwr_enable_overdrive(void)    {
    /*set Overdriver bit (ODEN) in the pwr_cr*/
    PWR -> CR   |=  PWR_CR_ODEN;

    /*Wait for the overdrive ready flag to be set*/
    while ((PWR -> CSR & PWR_CSR_ODRDY) == 0U)  /*Wait*/;

    /*Set the overdrive switch bit*/
    PWR -> CR   |=  PWR_CR_ODSWEN;

    /*Wait for the overdrive switch flag to be set*/
    while ((PWR -> CSR & PWR_CSR_ODSWRDY) == 0U)  /*Wait*/;

    /*Return system status*/
    return PWR_OK;
}

PWR_Status pwr_disable_overdrive(void)   {
    /*Reset simultaneously the ODEN and the ODSW bits in the PWR_CR*/
    PWR -> CR   &=  ~(PWR_CR_ODEN  |   PWR_CR_ODSWEN);
    
    /*Return system status*/
    return pwr_wait_flag(PWR_CSR_ODSWRDY, 0U);
}

/**
 * @brief Select the internal regulator's output voltage scaling, which
 *        trades regulator power draw for maximum achievable SYSCLK:
 *        scale 1 = highest performance (needed for >144MHz, or >168MHz
 *        with overdrive), scale 3 = lowest power / lowest max frequency.
 * @note  Must be configured BEFORE raising SYSCLK to a frequency that
 *        requires it - see SystemInit()'s ordering (voltage scale, then
 *        Flash latency, then HSE/PLL, then the actual clock switch).
 */
PWR_Status pwr_select_voltage_scale(PWR_VoltageScale_t vscale) {
    /*clear VOS bits*/
    PWR->CR &= ~PWR_CR_VOS;

    /*Select the voltage scale by setting VOS bits in PWR_CR*/
    uint32_t vos = 0;

    switch (vscale) {
        case PWR_VOLTAGE_SCALE_1: vos = 2U << PWR_CR_VOS_Pos; break;  // 0b10
        case PWR_VOLTAGE_SCALE_2: vos = 1U << PWR_CR_VOS_Pos; break;  // 0b01
        case PWR_VOLTAGE_SCALE_3: vos = 0U; break;                     // 0b00
        default: return PWR_INVALID_PARAM;
    }

    PWR->CR     |=   vos;

    /* No wait needed on STM32F4 */
    return PWR_OK;
}


/* ========================================================================== */
/* Low power modes */
/* ========================================================================== */

/**
 * @brief Sleep mode: halts the CPU core only. All peripheral clocks (and
 *        their state - timers still counting, ADC/DMA still running, etc.)
 *        keep running; ANY enabled interrupt wakes the core, which resumes
 *        exactly where it left off. Cheapest, least invasive low-power
 *        mode - safe to call from a main loop that still needs its
 *        peripherals live between events (see app/main.c).
 */
void pwr_enter_sleep_mode(void)
{
    /* Sleep mode (not deep sleep) */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

    __WFI();    /* Wait for interrupt */
}

/**
 * @brief Stop mode: most clocks stopped (1.2V domain powered down, all
 *        clocks in the 1.2V domain stopped), SRAM/register contents
 *        retained. Wakes only on an EXTI-capable event (EXTI line, RTC
 *        alarm, etc.) - a plain timer interrupt CANNOT wake the core from
 *        Stop, because the timer's clock is itself stopped. After waking,
 *        the clock has reverted to HSI - the caller must re-run the
 *        PLL/clock-switch sequence (SystemInit()-style) to get back to
 *        full speed.
 */
void pwr_enter_stop_mode(void)
{
    /* Select Stop mode */
    PWR->CR &= ~PWR_CR_PDDS;    /* Stop, not Standby */
    PWR->CR |=  PWR_CR_LPDS;    /* Low-power regulator */

    /* Enable deep sleep */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    __WFI();    /* Wait for interrupt */
}

/**
 * @brief Standby mode: deepest sleep - SRAM and most register contents are
 *        LOST. Only a small set of wakeup sources survive (WKUP pin, RTC
 *        alarm/wakeup, IWDG reset, NRST). On wake, the MCU performs a full
 *        reset and re-runs Reset_Handler/SystemInit()/main() from
 *        scratch - this function never returns; check the WUF/SBF flags in
 *        PWR->CSR early in your reset path if you need to distinguish "woke
 *        from standby" from a normal power-on reset.
 */
void pwr_enter_standby_mode(void)
{
    /* Select Standby mode */
    PWR->CR |= PWR_CR_PDDS;

    /* Clear wakeup flag */
    PWR->CR |= PWR_CR_CWUF;

    /* Enable deep sleep */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    __WFI();    /* Wait for interrupt */
}