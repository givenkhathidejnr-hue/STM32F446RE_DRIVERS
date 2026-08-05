/* ========================= timer_advanced.h ========================= */


#ifndef TIMER_ADVANCED_H
#define TIMER_ADVANCED_H


#include "timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PWM channel selection */
typedef enum {
    TIMER_ADV_CHANNEL_1 = 0,
    TIMER_ADV_CHANNEL_2 = 1,
    TIMER_ADV_CHANNEL_3 = 2,
    TIMER_ADV_CHANNEL_4 = 3
} timer_advanced_channel_t;

/* Polarity for break input */
typedef enum {
    TIMER_BREAK_POL_ACTIVE_LOW  = 0,   // break when low (default)
    TIMER_BREAK_POL_ACTIVE_HIGH = 1    // break when high
} timer_break_polarity_t;

/* Polarity for outputs */
typedef enum {
    TIMER_OUTPUT_POL_ACTIVE_HIGH = 0,
    TIMER_OUTPUT_POL_ACTIVE_LOW  = 1
} timer_output_polarity_t;

/* lock for break output */
typedef enum {
    TIMER_LOCK_OFF      = 0,
    TIMER_LOCK_LEVEL_1  = 1,
    TIMER_LOCK_LEVEL_2  = 2,
    TIMER_LOCK_LEVEL_3  = 3
} timer_break_lock_level_t;

/* Dead-time configuration */
typedef struct {
    TIM_TypeDef     *timer;
    uint8_t         dead_time;
} timer_deadtime_config_t;

/* Complementary output configuration */
typedef struct {
    TIM_TypeDef                 *timer;
    timer_advanced_channel_t    channel;
    timer_output_polarity_t     main_polarity;      // CHx polarity
    timer_output_polarity_t     comp_polarity;      // CHxN polarity
    bool                        enable_complementary;
} timer_complementary_config_t;


/* Break input configuration */
typedef struct {
    TIM_TypeDef              *timer;
    bool                      enable_break;
    timer_break_polarity_t    break_polarity;
    bool                      automatic_output_enable;   // AOE
    timer_break_lock_level_t  lock_configuration;        // LOCK bits (recommended)
} timer_break_config_t;

/**
 * @brief Set MOE (Main Output Enable) - complementary/PWM outputs only
 *        actually drive their pins once this bit is set.
 * @note  Safety ordering: configure dead-time, break input and lock level
 *        (timer_advanced_deadtime_config / _break_config, or the one-shot
 *        timer_advanced_bdtr_config) BEFORE calling this. Enabling outputs
 *        before break/dead-time protection is armed is how you get a
 *        shoot-through event on a half-bridge the first time this runs.
 */
__STATIC_FORCEINLINE void timer_advanced_enable(TIM_TypeDef *timer)
{
    timer->BDTR |= TIM_BDTR_MOE;
}


__STATIC_FORCEINLINE void timer_advanced_disable(TIM_TypeDef *timer)
{
    timer->BDTR &= ~TIM_BDTR_MOE;
}

/* channel is 0-indexed (TIMER_ADV_CHANNEL_1 == 0), matching CCER's 4-bit-
 * per-channel layout directly - no "-1" needed here, unlike a bug that used
 * to exist in the .c-side polarity configuration (see REVIEW_NOTES.md). */
__STATIC_FORCEINLINE void timer_advanced_enable_complementary(TIM_TypeDef *timer, timer_advanced_channel_t channel)
{
    uint32_t shift = ((uint32_t)channel) * 4;
    timer->CCER |= (TIM_CCER_CC1NE << shift);
}

__STATIC_FORCEINLINE void timer_advanced_disable_complementary(TIM_TypeDef *timer, timer_advanced_channel_t channel)
{
    uint32_t shift = ((uint32_t)channel) * 4;
    timer->CCER &= ~(TIM_CCER_CC1NE << shift);
}

/* API */
timer_status_t timer_advanced_deadtime_config(const timer_deadtime_config_t *cfg);
timer_status_t timer_advanced_complementary_config(const timer_complementary_config_t *cfg);
timer_status_t timer_advanced_break_config(const timer_break_config_t *cfg);

/* Optional: one-shot full BDTR configuration */
timer_status_t timer_advanced_bdtr_config(
    TIM_TypeDef              *timer,
    bool                      moe,
    bool                      aoe,
    bool                      bke,
    timer_break_polarity_t    break_polarity,
    uint8_t                   deadtime_dtg,
    uint8_t                   lock_level   // 0 = off, 1 = level 1, 2 = level 2, 3 = level 3
);

#ifdef __cplusplus
}
#endif

#endif  /* TIMER_ADVANCED_H */