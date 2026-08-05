/* ============================================================
* TIMER ENCODER / INPUT CAPTURE MODULE
* Separate from timer core per architecture
* ============================================================ */


/* ========================= timer_encoder.h ========================= */


#ifndef TIMER_ENCODER_H
#define TIMER_ENCODER_H


#include "timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * Encoder interface modes 
 * for TIM9 - TIM14, encoder mode is unavailable */
typedef enum {
    TIMER_ENCODER_TI2           = 1, /* TI1 */
    TIMER_ENCODER_TI1           = 2, /* TI2 */
    TIMER_ENCODER_TI1_AND_TI2   = 3 /* TI1+TI2 */
} timer_encoder_mode_t;

/* Encoder configuration */
typedef struct {
    TIM_TypeDef          *timer;
    timer_encoder_mode_t  mode;
    bool                  enable_filter;   // optional: basic filtering on both inputs
} timer_encoder_config_t;

__STATIC_FORCEINLINE void timer_encoder_reset(TIM_TypeDef *timer)
{
    timer->CNT = 0;
}

__STATIC_FORCEINLINE int32_t timer_encoder_get_position(TIM_TypeDef *timer)
{
    return (int32_t)(int16_t)timer->CNT;
}


/* Input capture channel */
typedef enum {
    TIMER_IC_CHANNEL_1 = 0,
    TIMER_IC_CHANNEL_2 = 1,
    TIMER_IC_CHANNEL_3 = 2,
    TIMER_IC_CHANNEL_4 = 3
} timer_ic_channel_t;

/* Input capture edge selection */
typedef enum {
    TIMER_IC_EDGE_RISING  = 0,   // CCxP = 0, CCxNP = 0
    TIMER_IC_EDGE_FALLING = 1,   // CCxP = 1, CCxNP = 0
    TIMER_IC_EDGE_BOTH    = 3    // CCxP = 1, CCxNP = 1 (10 is reserved/invalid)
} timer_ic_edge_t;

/* Input capture prescaler (ICxPSC) */
typedef enum {
    TIMER_IC_PSC_DIV_1   = 0,   // capture every event
    TIMER_IC_PSC_DIV_2   = 1,
    TIMER_IC_PSC_DIV_4   = 2,
    TIMER_IC_PSC_DIV_8   = 3
} timer_ic_prescaler_t;

/* Input capture configuration */
typedef struct {
    TIM_TypeDef             *timer;
    timer_ic_channel_t      channel; /* 1..4 */
    timer_ic_edge_t         edge;
    timer_ic_prescaler_t    prescaler;
    uint8_t                 filter;       // ICxF[3:0] 0..15 (0 = no filter)
} timer_ic_config_t;

__STATIC_FORCEINLINE uint32_t timer_ic_get_capture(TIM_TypeDef *timer, uint8_t channel)
{
    switch (channel) {
        case TIMER_IC_CHANNEL_1:      return timer->CCR1;
        case TIMER_IC_CHANNEL_2:      return timer->CCR2;
        case TIMER_IC_CHANNEL_3:      return timer->CCR3;
        case TIMER_IC_CHANNEL_4:      return timer->CCR4;
        default: return 0;
    }
}

/* enable capture interrupt per channel */
__STATIC_FORCEINLINE void timer_ic_enable_irq(TIM_TypeDef *timer, timer_ic_channel_t channel) {
    timer->DIER |= (TIM_DIER_CC1IE << ((uint32_t)channel));
}

__STATIC_FORCEINLINE void timer_ic_disable_irq(TIM_TypeDef *timer, timer_ic_channel_t channel) {
    timer->DIER &= ~(TIM_DIER_CC1IE << ((uint32_t)channel));
}

/* PWM input mode configuration */
typedef struct {
    TIM_TypeDef             *timer;
    uint8_t                  filter;         // IC1F / IC2F value (0 = no filter, 2–15 recommended)
    timer_ic_prescaler_t     prescaler;      // IC1PSC / IC2PSC (usually DIV_1)
    bool                     reset_on_rising; // optional: reset CNT on each rising edge (for period measurement)
} timer_pwm_input_config_t;

/* Get measured values (user calls these after capture events) */
__STATIC_FORCEINLINE uint32_t timer_pwm_input_get_period(TIM_TypeDef *timer) {
    return timer->CCR1;   // usually period (time between rising edges)
}

__STATIC_FORCEINLINE uint32_t timer_pwm_input_get_pulse_width(TIM_TypeDef *timer) {
    return timer->CCR2;   // high time (duty)
}

/* Calculate duty cycle in percent (0–100) */
__STATIC_FORCEINLINE float timer_pwm_input_get_duty_percent(TIM_TypeDef *timer) {
    uint32_t period = timer->CCR1;
    if (period == 0) return 0.0f;
    return (float)timer->CCR2 * 100.0f / (float)period;
}

/* PWM input mode API */
timer_status_t timer_pwm_input_init(const timer_pwm_input_config_t *cfg);

/* Encoder API */
timer_status_t timer_encoder_init(const timer_encoder_config_t *cfg);

/* Input capture API */
timer_status_t timer_input_capture_init(const timer_ic_config_t *cfg);


#ifdef __cplusplus
}
#endif
#endif  /* TIMER_ENCODER_H */