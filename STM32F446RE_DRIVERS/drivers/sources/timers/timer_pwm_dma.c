#include "timer_pwm_dma.h"

timer_status_t timer_pwm_dma_start(const timer_pwm_dma_cfg_t *cfg)
{
    if (!cfg || !cfg->timer || !cfg->buffer || !cfg->length)   return TIM_INVALID_PARAM;

    TIM_TypeDef *timer  =   cfg->timer;
    timer_pwm_channel_t pwm_channel = cfg->channel;

    /* Enable the channel's DMA request (CCxDE, DIER bits 12:9 - CH1 = bit 9,
     * NOT bit 8). Bit 8 is UDE (update-event DMA request), a different
     * trigger source entirely: arming UDE instead of CCxDE would fire DMA
     * on every timer overflow rather than on this channel's own compare
     * match, which is the wrong trigger for per-channel PWM waveform
     * generation. */
    timer->DIER |=  (1U << (pwm_channel + TIM_DIER_CC1DE_Pos));

    /* DMA stream configuration intentionally left external - wire it up
     * with this project's own dma.h API before calling this function:
     *   dma_config_transfer(hdma, (uint32_t)&timer->CCR1 + channel*4,
     *                        (uint32_t)cfg->buffer, cfg->length);
     * with direction = DMA_DIR_MEM_TO_PERIPH, mode = DMA_MODE_CIRCULAR if
     * cfg->circular, then dma_start(hdma) BEFORE calling this function so
     * the DMA stream is already armed and waiting when the CCxDE request
     * starts arriving. */
   timer_pwm_enable(timer, pwm_channel);

    return TIM_OK;
}

timer_status_t timer_pwm_dma_stop(TIM_TypeDef *timer, timer_pwm_channel_t channel)
{
    if (!timer) return TIM_INVALID_PARAM;


    timer->DIER &= ~(1U << (channel + TIM_DIER_CC1DE_Pos));
    timer_pwm_disable(timer, channel);


    return TIM_OK;
}
