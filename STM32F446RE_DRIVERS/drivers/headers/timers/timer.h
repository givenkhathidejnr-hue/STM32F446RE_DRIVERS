/* ============================================================================
* timer.h - Core Timer Driver (Hardware-Agnostic Timer Base Layer)
* ----------------------------------------------------------------------------
* Scope:
* - Core time‑base configuration only
* - Counter control
* - Update interrupt handling
* - Atomic CNT/ARR access helpers
*
* Explicitly NOT included:
* - PWM / Output Compare
* - Input Capture
* - Synchronization / Trigger routing
* - Encoder mode
* - Advanced BDTR features
* - RCC/NVIC configuration (application responsibility)
* ==========================================================================*/


#ifndef TIMER_H
#define TIMER_H


#include <stdint.h>
#include <stdbool.h>
#include "device.h" /* CMSIS device header providing TIM_TypeDef */


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief -  Counter control
* Time base configuration
* Update interrupt
* Atomic CNT / ARR access
* Minimal general features (OPM, URS)
 */

/* =====================================================================
 * status
 * ===================================================================== */
typedef enum    {
    TIM_OK, 
    TIM_INVALID_PARAM,
    TIM_TIMEOUT
} timer_status_t;

/* =====================================================================
 * Public types
 * ===================================================================== */

/* -------------------------------
 * Timer capability flags
 * ------------------------------- */
typedef enum {
    TIMER_CAP_NONE      = 0U,
    TIMER_CAP_GENERAL   = (1U << 0),
    TIMER_CAP_ADVANCED  = (1U << 1),
    /* future:
     * TIMER_CAP_PWM    = (1U << 2),
     * TIMER_CAP_DMA    = (1U << 3),
     * TIMER_CAP_ENCODER= (1U << 4),
     */
} timer_capability_t;




/* =====================================================================
* Timebase Configuration
* =====================================================================*/
typedef struct {
    uint32_t                prescaler;              /* PSC value */
    uint32_t                period;            /* ARR value */

    bool                    period_preload;
    bool                    one_pulse_mode;         /* OPM */

    bool                    disable_update_event;          /* UDIS */
    bool                    update_on_overflow_only;     /* URS */
} timer_base_config_t;


/* =====================================================================
* Initialization API
* ===================================================================== */
/* Core API */
timer_status_t timer_base_init        (TIM_TypeDef *tim, const timer_base_config_t *cfg);
timer_status_t timer_base_deinit      (TIM_TypeDef *tim);

/* =====================================================================
* Counter Control (Inline — performance critical)
* ===================================================================== */
__STATIC_FORCEINLINE void timer_start(TIM_TypeDef *timer)
{
    timer->CR1 |= TIM_CR1_CEN;
}


__STATIC_FORCEINLINE void timer_stop(TIM_TypeDef *timer)
{
    timer->CR1 &= ~TIM_CR1_CEN;
}


__STATIC_FORCEINLINE void timer_generate_update(TIM_TypeDef *timer)
{
    timer->EGR |= TIM_EGR_UG;
}

/* =====================================================================
* Atomic Register Access Helpers
* =====================================================================*/
/* Counter access */
__STATIC_FORCEINLINE uint32_t timer_get_count(TIM_TypeDef *timer)  { return timer->CNT; }
__STATIC_FORCEINLINE void timer_set_count(TIM_TypeDef *timer, uint32_t value)  { timer->CNT  = value; }

/* Auto reload access*/
__STATIC_FORCEINLINE uint32_t timer_get_period(TIM_TypeDef *timer) { return timer->ARR; }
__STATIC_FORCEINLINE void timer_set_period(TIM_TypeDef *timer, uint32_t value) { timer->ARR = value; } 

/* Prescaler access*/
__STATIC_FORCEINLINE uint32_t timer_get_prescaler(TIM_TypeDef *timer) { return timer->PSC; }
__STATIC_FORCEINLINE void timer_set_prescaler(TIM_TypeDef *timer, uint32_t value) { timer->PSC = value; } 


/**
 * @brief Very lightweight "is 32-bit timer?" helper — extend per family.
 * @note  On STM32F446, only TIM2 and TIM5 have 32-bit CNT/ARR/PSC-adjacent
 *        (PSC is always 16-bit on every timer) counters; every other
 *        general-purpose/advanced timer is 16-bit. base_configure_timer()
 *        uses this to decide whether a period > 0xFFFF is a valid request
 *        or a caller error.
 * @note  STM32H755 port: TIM2/TIM5 remain 32-bit on H7 too, but the
 *        broader timer family list changes (more instances, some with
 *        different capabilities) - re-verify against the H7 reference
 *        manual's timer feature table rather than assuming this function
 *        is complete as-is.
 */
__STATIC_FORCEINLINE bool timer_is_32bit(const TIM_TypeDef *timer) {
#if defined(TIM2) || defined(TIM5)
    if (timer == TIM2 || timer == TIM5) return true;
#endif
    // Add TIM1/TIM8 etc. if your family has 32-bit advanced timers
    return false;
}
#ifdef __cplusplus
}
#endif


#endif /* TIMER_H */
