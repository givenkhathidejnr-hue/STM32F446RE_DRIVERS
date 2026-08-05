/** ============================================================================
* PWM + DMA MODULE (timer_pwm_dma.h / timer_pwm_dma.c)
* High‑performance waveform generation / motor control support.
* Assumes:
* - Timer core already configured
* - PWM channel already initialized (timer_pwm module)
* - DMA controller configured externally (RCC/NVIC left to app)
* ============================================================================ */


/* ========================== timer_pwm_dma.h ========================== */


#ifndef TIMER_PWM_DMA_H
#define TIMER_PWM_DMA_H


#include "timer_pwm.h"


/* DMA PWM configuration */
typedef struct {
    TIM_TypeDef         *timer;
    timer_pwm_channel_t channel;

    uint32_t            *buffer;
    uint16_t            length;

    bool                circular;
} timer_pwm_dma_cfg_t;


timer_status_t timer_pwm_dma_start(const timer_pwm_dma_cfg_t *cfg);
timer_status_t timer_pwm_dma_stop(TIM_TypeDef *tim, timer_pwm_channel_t ch);

#endif  /* TIMER_PWM_DMA_H */