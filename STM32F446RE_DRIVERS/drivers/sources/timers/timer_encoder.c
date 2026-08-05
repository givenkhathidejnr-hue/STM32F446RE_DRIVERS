/* ========================= timer_encoder.c ========================= */


#include "timer_encoder.h"


/* ------------------------------------------------------------
* Encoder mode
* ------------------------------------------------------------ */
/* Helper: check if timer supports encoder mode (not on basic timers) */
static bool timer_supports_encoder(const TIM_TypeDef *tim) {
    // Most families: TIM1,2,3,4,5,8,21,22,... support encoder
    // TIM6,7,9–14,16,17 usually do NOT
    if (tim == TIM6 || tim == TIM7) return false;
#if defined(TIM9) || defined(TIM10) || defined(TIM11) || defined(TIM12) || defined(TIM13) || defined(TIM14)
    if (tim == TIM9  || tim == TIM10 || tim == TIM11 ||
        tim == TIM12 || tim == TIM13 || tim == TIM14) return false;
#endif
    return true;
}

timer_status_t timer_encoder_init(const timer_encoder_config_t *cfg)
{
    if (!cfg || !cfg->timer)  return TIM_INVALID_PARAM;

    TIM_TypeDef *timer          = cfg->timer;

    if (!timer_supports_encoder(timer))   return TIM_INVALID_PARAM;

    timer_encoder_mode_t mode   = cfg->mode;

    // Clear SMS bits
    timer->SMCR &= ~TIM_SMCR_SMS;

    // Set encoder mode (SMS = 001, 010 or 011)
    timer->SMCR |= ((uint32_t)mode << TIM_SMCR_SMS_Pos);

    // Optional: basic input filtering (IC1F/IC2F = some value e.g. 0010 = fSAMPLING=fCK_INT, N=4)


    return TIM_OK;
}

/* Input capture API */
timer_status_t timer_input_capture_init(const timer_ic_config_t *cfg)
{
    if (!cfg || !cfg->timer)  return TIM_INVALID_PARAM;

    TIM_TypeDef *timer              = cfg->timer;
    timer_ic_channel_t channel      = cfg->channel;
    timer_ic_prescaler_t prescaler  = cfg->prescaler;

    /* 1. Select input capture direct mode (CCxS = 01). CH1/CH2 live in
     * CCMR1, CH3/CH4 in CCMR2 - the boundary is channel <= CHANNEL_2 (i.e.
     * index 1), not index 2 (that would wrongly route CH3 into CCMR1 with
     * an out-of-range 16-bit shift, leaving it unconfigured). */
    if (channel <= TIMER_IC_CHANNEL_2) {
        timer->CCMR1 &= ~(TIM_CCMR1_CC1S << (channel*8));
        timer->CCMR1 |= (TIM_CCMR1_CC1S_0 << (channel*8));   // 01 = TIxFPx
    } else {
        timer->CCMR2 &= ~(TIM_CCMR2_CC3S << ((channel - 2)*8));
        timer->CCMR2 |= (TIM_CCMR2_CC3S_0 << ((channel - 2)*8));
    }

    /* 2. configure input capture prescaler (ICxPSC, 2 bits) */
    __IO uint32_t *ccmr = (channel <= TIMER_IC_CHANNEL_2) ? &timer->CCMR1 : &timer->CCMR2;
    uint32_t psc_shift = (channel % 2 == 0) ? 2 : 10;   // CH1/3: bits 3:2, CH2/4: 11:10

    *ccmr   &=  ~(3U << psc_shift);
    *ccmr   |=  (uint32_t)prescaler << psc_shift;

    /* 3. input filter (ICxF, 4 bits) */
    uint32_t filter_shift = (channel % 2 == 0) ? 4 : 12;
    uint32_t filter = ((uint32_t)cfg->filter > 15U) ? 15U : (uint32_t)cfg->filter;
    *ccmr &= ~(0xFU << filter_shift);
    *ccmr |= filter << filter_shift;

    /* 4. Edge detection (CCxP + CCxNP). Within each channel's 4-bit CCER
     * group, CCxP is relative bit 1 and CCxNP is relative bit 3 - they are
     * NOT adjacent (bit 2 is CCxNE, the complementary-output-enable bit on
     * advanced-timer channels 1-3, a wholly unrelated feature). Clear/set
     * them individually via the named masks shifted by channel*4, instead
     * of a blind contiguous 2-bit window that would clobber CCxNE and miss
     * CCxNP. */
    uint32_t channel_shift = (uint32_t)channel * 4U;
    timer->CCER &= ~((TIM_CCER_CC1P | TIM_CCER_CC1NP) << channel_shift);

    switch (cfg->edge) {
        case TIMER_IC_EDGE_RISING:
            /* CCxP = 0, CCxNP = 0 -> rising (already cleared above) */
            break;
        case TIMER_IC_EDGE_FALLING:
            timer->CCER |= (TIM_CCER_CC1P << channel_shift);
            break;
        case TIMER_IC_EDGE_BOTH:
            /* Both-edge capture needs CCxP AND CCxNP set together
             * (00=rising, 01=falling, 11=both; 10 is reserved) - setting
             * only CCxNP produces the reserved combination instead. */
            timer->CCER |= ((TIM_CCER_CC1P | TIM_CCER_CC1NP) << channel_shift);
            break;
    }

    return TIM_OK;
}

/**
 * @note  Steps 9-10 below (update IRQ, CEN) are intentionally left as
 *        comments - matching timer_encoder_init()/timer_input_capture_init()
 *        and the rest of this library, initialization configures the
 *        peripheral and a separate explicit timer_start(timer) call (from
 *        timer.h) is what actually starts the counter.
 */
timer_status_t timer_pwm_input_init(const timer_pwm_input_config_t *cfg)
{
    if (!cfg || !cfg->timer) {
        return TIM_INVALID_PARAM;
    }

    TIM_TypeDef *timer = cfg->timer;

    // 1. Stop timer if running
    timer_stop(timer);

    // 2. Configure both channels as input capture from TI1
    // CH1: direct TI1 (CC1S = 01)
    timer->CCMR1 &= ~TIM_CCMR1_CC1S;
    timer->CCMR1 |= TIM_CCMR1_CC1S_0;   // 01 = TI1FP1

    // CH2: indirect TI1 (CC2S = 10 = filtered timer input 2)
    timer->CCMR1 &= ~TIM_CCMR1_CC2S;
    timer->CCMR1 |= TIM_CCMR1_CC2S_1;   // 10 = TI2FP2 (which is TI1 routed)

    // 3. Set prescaler for both channels (usually same)
    // IC1PSC bits 3:2
    timer->CCMR1 &= ~(0x3U << 2);
    timer->CCMR1 |= ((uint32_t)cfg->prescaler << 2);

    // IC2PSC bits 11:10
    timer->CCMR1 &= ~(0x3U << 10);
    timer->CCMR1 |= ((uint32_t)cfg->prescaler << 10);

    // 4. Set input filter (same for both channels usually)
    uint8_t f = (cfg->filter > 15) ? 15 : cfg->filter;
    timer->CCMR1 &= ~(0xFU << 4);    // IC1F
    timer->CCMR1 |= (f << 4);
    timer->CCMR1 &= ~(0xFU << 12);   // IC2F
    timer->CCMR1 |= (f << 12);

    // 5. Edge selection (CH1 rising, CH2 falling → typical PWM input)
    // CH1: rising edge (CC1P = 0, CC1NP = 0)
    timer->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP);

    // CH2: falling edge (CC2P = 1, CC2NP = 0)
    timer->CCER |= TIM_CCER_CC2P;
    timer->CCER &= ~TIM_CCER_CC2NP;

    // 6. Enable both capture channels
    timer->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E);

    // 7. Slave mode: PWM input mode 3 (SMS = 110)
    timer->SMCR &= ~TIM_SMCR_SMS;
    timer->SMCR |= (0x6U << TIM_SMCR_SMS_Pos);   // 110 = PWM mode 3

    // 8. Optional: reset counter on rising edge (TS = 101 → TI1FP1)
    if (cfg->reset_on_rising) {
        timer->SMCR &= ~TIM_SMCR_TS;
        timer->SMCR |= (0x5U << TIM_SMCR_TS_Pos);     // 101 = TI1FP1
        timer->SMCR |= TIM_SMCR_SMS_2;                // SMS bit 2 already set above
    }

    // 9. Enable update interrupt if you want overflow handling
    // tim->DIER |= TIM_DIER_UIE;

    // 10. Start counter (usually upcounting)
    // tim->CR1 &= ~TIM_CR1_DIR;      // up
    // tim->CR1 |= TIM_CR1_CEN;

    return TIM_OK;
}