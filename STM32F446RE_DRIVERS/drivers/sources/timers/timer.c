#include "timer.h"

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif


void error_handler(void);

/* =====================================================================
 * Internal helpers
 * ===================================================================== */
__STATIC_INLINE timer_status_t base_configure_timer(TIM_TypeDef *timer, const timer_base_config_t *cfg) {
    /* Prescaler setting */
    if  (cfg->prescaler <= 0xFFFFU)   
    {
        timer->PSC  =   cfg->prescaler;
    }
    else    return TIM_INVALID_PARAM;

    /* Auto-reload (period) */
    if (timer_is_32bit(timer)) {
        timer->ARR = (uint32_t)cfg->period;                     // 32-bit ok
    } else {
        if (cfg->period > 0xFFFFu) {
            return TIM_INVALID_PARAM;
        }
        timer->ARR = (uint16_t)cfg->period;
    }

    /* Auto reload preload setting */
    if  (cfg->period_preload)                   timer->CR1   |=  TIM_CR1_ARPE;
    else                                        timer->CR1   &=  ~TIM_CR1_ARPE;

    /* One-pulse mode (not available on all timers — silent ignore on basic) */
    if  (cfg->one_pulse_mode)                   timer->CR1   |=  TIM_CR1_OPM;
    else                                        timer->CR1   &=  ~TIM_CR1_OPM;

    /* Update disable setting */
    if  (cfg->disable_update_event)             timer->CR1   |=  TIM_CR1_UDIS;
    else                                        timer->CR1   &=  ~TIM_CR1_UDIS;
    
    /* Update request source */
    if  (cfg->update_on_overflow_only)          timer->CR1   |=  TIM_CR1_URS;
    else                                        timer->CR1   &=  ~TIM_CR1_URS;

    /* Return status OK - configuartion was done successfully */
    return  TIM_OK;
}


/* =====================================================================
 * Public API
 * ===================================================================== */

/**
 * @brief Configure a timer's time base (prescaler, period, one-pulse mode,
 *        update-event behavior) and latch it immediately.
 * @note  Does not call rcc_clock_control() or touch NVIC - enabling this
 *        timer's peripheral clock, and any interrupt priority/enable, are
 *        the caller's responsibility (see PWM/encoder/sync modules layered
 *        on top, which all assume the base is already running).
 * @note  timer_generate_update() forces PSC/ARR (which are shadow/preload
 *        registers) to take effect right away instead of waiting for the
 *        timer's natural next overflow - without it, a freshly-configured
 *        timer could run one full period at its *previous* settings before
 *        the new ones apply.
 */
timer_status_t timer_base_init(TIM_TypeDef *timer, const timer_base_config_t *cfg)
{
    timer_status_t status;

    /* Validation */
    if (!cfg || !timer)    return TIM_INVALID_PARAM;

    /* Disable timer */
    timer_stop(timer);

    /* Basic timer configuration */
    status = base_configure_timer(timer, cfg);
    if (status != TIM_OK)
    {
        error_handler();
    }
    /* generate update to load shadow registers immediately */
    timer_generate_update(timer);

    return TIM_OK;
 }

 /**
  * @note  ARR is unconditionally set to the 32-bit max here even for
  *        16-bit-only timers - harmless on purpose: those timers' ARR
  *        register is physically only 16 bits wide, so the write is
  *        silently truncated by hardware to 0xFFFF (their actual max),
  *        rather than corrupting anything.
  */
 timer_status_t timer_base_deinit(TIM_TypeDef *tim)
{
    if (!tim) return TIM_INVALID_PARAM;

    timer_stop(tim);
    tim->CR1  = 0;
    tim->DIER = 0;
    tim->SR   = 0;          // clear pending flags
    tim->PSC  = 0;
    tim->ARR  = 0xFFFFFFFFu; // max → safe default

    return TIM_OK;
}

void error_handler(void)    {
    while(1)    __asm volatile("nop");
}