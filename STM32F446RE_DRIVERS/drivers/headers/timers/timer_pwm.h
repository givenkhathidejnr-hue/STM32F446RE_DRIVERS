#ifndef TIMER_PWM_H
#define TIMER_PWM_H

#include "timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CCMR1/CCMR2 + CCER bit-packing convention used across the PWM,
 *        advanced (BDTR), and encoder/input-capture timer modules.
 *
 * The four capture/compare channels are split across two 32-bit registers
 * with an identical per-channel layout repeated twice in each:
 *   - CCMR1 bits [7:0]  = CH1, bits [15:8]  = CH2
 *   - CCMR2 bits [7:0]  = CH3, bits [15:8]  = CH4
 *   each 8-bit half further splits into CCxS[1:0] (mode select, bits 1:0),
 *   OCxPE (preload enable, bit 3), OCxM[2:0] (output compare mode, bits
 *   6:4).
 *   - CCER packs all four channels' enable/polarity bits into one 32-bit
 *   register, 4 bits per channel: CCxE (bit 0), CCxP (bit 1), CCxNE (bit 2,
 *   advanced timers only), CCxNP (bit 3), at channel*4.
 *
 * Because the intra-register layout is identical for every channel, driver
 * code below only ever needs "even or odd channel index" (which register
 * half) and a numeric channel offset - never a fully separate code path per
 * channel. See timer_pwm.c's timer_pwm_init() for the canonical example.
 */

/* PWM mode TIMx_CCMRx register */
typedef enum {
    TIMER_PWM_MODE_1 = 6,   // OCxM = 110 → active until match (up)
    TIMER_PWM_MODE_2 = 7    // OCxM = 111 → inactive until match (up)
} timer_pwm_mode_t;

/* PWM channel selection */
typedef enum {
    TIMER_PWM_CHANNEL_1 = 0,
    TIMER_PWM_CHANNEL_2,
    TIMER_PWM_CHANNEL_3,
    TIMER_PWM_CHANNEL_4
} timer_pwm_channel_t;

/* Applies to TIM2 - TIM5, TIM1, and TIM8 */
typedef enum    {
    PWM_COUNTER_MODE_UP             = 0,
    PWM_COUNTER_MODE_DOWN           = 1,
} timer_counter_mode_t;

/**
 * @brief center aligned mode - counts up and down alternatively
 * Available only for timers TIM2 - TIM5, TIM1, and TIM8 */
typedef enum    {
    PWM_ALIGNMENT_EDGE         = 0,
    PWM_ALIGNMENT_CENTER_1     = 1,   // CMS = 01
    PWM_ALIGNMENT_CENTER_2     = 2,   // CMS = 10
    PWM_ALIGNMENT_CENTER_3     = 3    // CMS = 11
} timer_pwm_alignment_t;

/* Polarity for main output (CHx)*/
typedef enum    {
    PWM_POLARITY_ACTIVE_HIGH = 0,
    PWM_POLARITY_ACTIVE_LOW  = 1
} timer_pwm_polarity_t;

typedef struct {
    timer_pwm_channel_t     channel;
    timer_pwm_mode_t        pwm_mode;

    timer_pwm_alignment_t   alignment;
    timer_counter_mode_t    counter_mode;

    uint32_t                pulse;              /* CCRx value */
    bool                    oc_preload;            /* OCxPE */
    timer_pwm_polarity_t    polarity;      /*  CCxP bit in the TIMx_CCER */
} timer_pwm_cfg_t;


__STATIC_FORCEINLINE void timer_pwm_enable(TIM_TypeDef *timer, timer_pwm_channel_t channel)
{
    timer->CCER     |=      (TIM_CCER_CC1E << (channel * 4));
}
__STATIC_FORCEINLINE void timer_pwm_disable(TIM_TypeDef *timer, timer_pwm_channel_t channel)
{
    timer->CCER     &=      ~(TIM_CCER_CC1E << (channel * 4));
}
__STATIC_FORCEINLINE void timer_pwm_set_pulse(TIM_TypeDef *timer, timer_pwm_channel_t channel, uint32_t value)
{
    __IO uint32_t *ccr = &timer->CCR1 + (uint32_t)channel;
    *ccr = value;
}

/* ============================================================
 * public API
 * ============================================================ */
timer_status_t timer_pwm_init(TIM_TypeDef *timer, const timer_pwm_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
#endif  /* TIMER_PWM_H */