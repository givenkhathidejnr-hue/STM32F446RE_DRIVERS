/* ============================================================================
* TIMER SYNCHRONIZATION MODULE (timer_sync.h / timer_sync.c)
* Provides master/slave timer synchronization using TRGO / ITR routing.
* Core timer must already be initialized via timer_init().
* RCC/NVIC/DMA remain application responsibility.
* ============================================================================ */
#include "timer_sync.h"

timer_status_t timer_sync_master_init(const timer_master_sync_cfg_t *cfg)
{
    if (!cfg || !cfg->timer)   return TIM_INVALID_PARAM;

    TIM_TypeDef *timer      = cfg->timer;
    timer_trgo_t trigger    = cfg->trigger_output;

    timer->CR2  &=  ~TIM_CR2_MMS;
    timer->CR2  |=  (trigger << TIM_CR2_MMS_Pos);

    return TIM_OK;
}

timer_status_t timer_sync_slave_init(const timer_slave_sync_cfg_t *cfg)
{
    if (!cfg || !cfg->timer)   return TIM_INVALID_PARAM;

    TIM_TypeDef *timer                  = cfg->timer;
    timer_slave_mode_sync_t slave_mode  = cfg->mode;
    timer_itr_t internal_trigger        = cfg->internal_trigger;

    timer->SMCR &=  ~TIM_SMCR_SMS;
    timer->SMCR |=  slave_mode << TIM_SMCR_SMS_Pos;

    if (slave_mode == TIMER_SLAVE_TRIGGER_MODE) { timer->SMCR &= ~TIM_SMCR_TS;  timer->SMCR |=  internal_trigger << TIM_SMCR_TS_Pos; }

    return TIM_OK;
}