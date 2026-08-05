#include "timer_pwm.h"

/* Note: an unused/duplicate timer_pwm_ccr() helper used to live here - it
 * did the same job as timer_pwm_set_pulse()'s pointer arithmetic (in
 * timer_pwm.h) but via a switch with no default case, so an out-of-range
 * channel fell off the end of a non-void function. Removed rather than
 * fixed since timer_pwm_set_pulse() already covers the need. */

timer_status_t timer_pwm_init(TIM_TypeDef *timer, const timer_pwm_cfg_t *cfg)
{
    if (!timer || !cfg) return TIM_INVALID_PARAM;

    timer_pwm_channel_t channel = cfg->channel;
    uint32_t channel_idx        = (uint32_t)channel;

    /* CCMR1 holds CH1/CH2, CCMR2 holds CH3/CH4, and each register uses the
     * *same* relative bit layout for its "first"/"second" channel (CCxS at
     * bits 1:0 or 9:8, OCxM at bits 6:4 or 14:12, OCxPE at bit 3 or 11) -
     * that's why ocm_shift/pe_shift below only need "even or odd channel
     * index", not "which CCMR register", and why the CC1S/CC1NE-family
     * named masks are reused with a plain numeric shift for every channel
     * throughout this function. */

    /* 1. Set channel as output (clear CCxS bits) */
    if (channel <= TIMER_PWM_CHANNEL_2) {
        timer->CCMR1 &= ~(TIM_CCMR1_CC1S << (channel_idx * 8));
    } else {
        timer->CCMR2 &= ~(TIM_CCMR2_CC3S << ((channel_idx - 2) * 8));
    }

    /* 2. Set PWM mode (OCxM bits) */
    uint32_t ocm_shift = (channel % 2 == 0) ? 4 : 12;
    volatile uint32_t *ccmr = (channel <= TIMER_PWM_CHANNEL_2) ? &timer->CCMR1 : &timer->CCMR2;
    *ccmr &= ~(7U << ocm_shift);
    *ccmr |= ((uint32_t)cfg->pwm_mode << ocm_shift);

    /* 3. Configure alignment on CR1 (DIR = bit 4, CMS = bits 6:5). Per the
     * reference manual, a non-zero CMS forces hardware up/down counting and
     * makes it ignore DIR entirely - so the edge-aligned branch must also
     * clear CMS, otherwise stale center-aligned bits from a previous config
     * silently override the direction requested here. counter_mode also
     * needs to land on DIR's actual bit (4), not bit 0 (CEN, the timer
     * enable bit) - OR'ing it in unshifted would spuriously start the
     * timer instead of setting direction. */
    switch (cfg->alignment)
    {
        case PWM_ALIGNMENT_EDGE:
            timer->CR1  &=  ~(TIM_CR1_DIR | TIM_CR1_CMS);
            timer->CR1  |=  ((uint32_t)cfg->counter_mode << TIM_CR1_DIR_Pos);
            break;

        default:
            timer->CR1  &=  ~TIM_CR1_CMS;
            timer->CR1  |=  ((uint32_t)cfg->alignment << TIM_CR1_CMS_Pos);
            break;
    }

    /* 4. OC preload enable (OCxPE) */
    uint32_t pe_shift = (channel % 2 == 0) ? 3 : 11;
    if (cfg->oc_preload) {
        *ccmr |= (1U << pe_shift);
    } else {
        *ccmr &= ~(1U << pe_shift);
    }

    /* 5. Polarity (CCxP bit) */
    uint32_t pol_shift = channel_idx * 4 + 1;   // CC1P=1, CC2P=5, CC3P=9, CC4P=13
    if (cfg->polarity == PWM_POLARITY_ACTIVE_LOW) {
        timer->CCER |= (TIM_CCER_CC1P << pol_shift);
    } else {
        timer->CCER &= ~(TIM_CCER_CC1P << pol_shift);
    }

    // 6. Set initial compare value
    timer_pwm_set_pulse(timer, channel, cfg->pulse);

    // Note: We do NOT enable the channel here — let user call timer_pwm_enable()
    //       This is safer — user controls when outputs actually appear
    return TIM_OK;
}

