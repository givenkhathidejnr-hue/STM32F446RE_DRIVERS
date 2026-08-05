#include "timer_advanced.h"

/* Helper: check if timer supports advanced features */
static inline bool is_advanced_timer(const TIM_TypeDef *tim)
{
    return (tim == TIM1) || (tim == TIM8);
}

/* API
 * @note Function names below intentionally match the timer_advanced_*
 *       prototypes declared in timer_advanced.h. (They previously didn't -
 *       see REVIEW_NOTES.md - which meant nothing declared in the header
 *       could actually be linked against.)
 */
timer_status_t timer_advanced_deadtime_config(const timer_deadtime_config_t *cfg)
{
    if (!cfg || !cfg->timer)            return TIM_INVALID_PARAM;
    if (!is_advanced_timer(cfg->timer)) return TIM_INVALID_PARAM;

    /* DTG[7:0], BDTR bits 7:0 - dead-time generator value. The actual
     * dead-time this produces is piecewise (see RM0390 "Bit 7:0 DTG"): for
     * DTG < 0x80 it's DTG * Tdtg where Tdtg = 1 x DTS period; above 0x80 the
     * encoding switches to coarser step sizes (2x/8x/16x) to reach longer
     * dead times without needing more bits. */
    cfg->timer->BDTR &= ~TIM_BDTR_DTG;
    cfg->timer->BDTR |= (uint32_t)(cfg->dead_time & 0xFF);

    return TIM_OK;
}

timer_status_t timer_advanced_complementary_config(const timer_complementary_config_t *cfg)
{
    if (!cfg || !cfg->timer)   return TIM_INVALID_PARAM;
    if (!is_advanced_timer(cfg->timer)) return TIM_INVALID_PARAM;
    if (cfg->channel > TIMER_ADV_CHANNEL_3) return TIM_INVALID_PARAM;   // CH4 has no complementary on most devices

    TIM_TypeDef *timer = cfg->timer;

    /* channel is already 0-indexed (TIMER_ADV_CHANNEL_1 == 0), so the CCER
     * nibble for this channel starts at channel*4 - no "-1" here. (The
     * previous "- 1" underflowed to 0xFFFFFFFF for channel 1, producing an
     * out-of-range/undefined shift amount.) */
    uint32_t shift = (uint32_t)cfg->channel * 4U;

    // Main channel polarity (CCxP)
    if (cfg->main_polarity == TIMER_OUTPUT_POL_ACTIVE_LOW) {
        timer->CCER |= (TIM_CCER_CC1P << shift);
    } else {
        timer->CCER &= ~(TIM_CCER_CC1P << shift);
    }

    // Complementary polarity (CCxNP)
    if (cfg->comp_polarity == TIMER_OUTPUT_POL_ACTIVE_LOW) {
        timer->CCER |= (TIM_CCER_CC1NP << shift);
    } else {
        timer->CCER &= ~(TIM_CCER_CC1NP << shift);
    }

    // Enable/disable complementary output
    if (cfg->enable_complementary) {
        cfg->timer->CCER |= (TIM_CCER_CC1NE << shift);
    } else {
        cfg->timer->CCER &= ~(TIM_CCER_CC1NE << shift);
    }

    return TIM_OK;
}

timer_status_t timer_advanced_break_config(const timer_break_config_t *cfg)
{
    if (!cfg || !cfg->timer)  return TIM_INVALID_PARAM;
    if (!is_advanced_timer(cfg->timer)) return TIM_INVALID_PARAM;

    TIM_TypeDef *timer = cfg->timer;

    if (cfg->enable_break)  timer->BDTR |= TIM_BDTR_BKE;
    else                    timer->BDTR &= ~TIM_BDTR_BKE;


    if (cfg->break_polarity == TIMER_BREAK_POL_ACTIVE_HIGH) timer->BDTR |= TIM_BDTR_BKP;
    else                                                    timer->BDTR &= ~TIM_BDTR_BKP;


    if (cfg->automatic_output_enable)                       timer->BDTR |= TIM_BDTR_AOE;
    else                                                    timer->BDTR &= ~TIM_BDTR_AOE;

    /* LOCK[1:0], BDTR bits 9:8 - write-protects BDTR/OSSI/OSSR/CCx config
     * against further software writes (level increases with the value; see
     * RM0390 "LOCK bits" table). Must be shifted into position 8, not
     * OR'd raw - raw values 1-3 would otherwise land in DTG's bits 0:1. */
    timer->BDTR &= ~TIM_BDTR_LOCK;
    timer->BDTR |= ((uint32_t)cfg->lock_configuration << TIM_BDTR_LOCK_Pos);


    return TIM_OK;
}

/**
 * @brief One-shot BDTR configuration: sets deadtime, break, break polarity,
 *        automatic-output-enable, lock level and MOE in a single register
 *        write.
 * @note  Unlike the piecemeal *_config() functions above, this writes BDTR
 *        exactly once. That matters because a non-zero lock level
 *        write-protects some of BDTR's own fields from further writes -
 *        doing this as one shot avoids a partially-locked-out
 *        read-modify-write sequence.
 */
timer_status_t timer_advanced_bdtr_config(
    TIM_TypeDef              *timer,
    bool                      moe,
    bool                      aoe,
    bool                      bke,
    timer_break_polarity_t    break_polarity,
    uint8_t                   deadtime_dtg,
    uint8_t                   lock_level)
{
    if (!timer)                                    return TIM_INVALID_PARAM;
    if (!is_advanced_timer(timer))                  return TIM_INVALID_PARAM;
    if (lock_level > (uint8_t)TIMER_LOCK_LEVEL_3)   return TIM_INVALID_PARAM;

    uint32_t bdtr = timer->BDTR;

    bdtr &= ~(TIM_BDTR_MOE | TIM_BDTR_AOE | TIM_BDTR_BKE | TIM_BDTR_BKP |
              TIM_BDTR_DTG | TIM_BDTR_LOCK);

    bdtr |= ((uint32_t)deadtime_dtg & TIM_BDTR_DTG);
    bdtr |= ((uint32_t)lock_level << TIM_BDTR_LOCK_Pos);

    if (bke)                                            bdtr |= TIM_BDTR_BKE;
    if (break_polarity == TIMER_BREAK_POL_ACTIVE_HIGH)  bdtr |= TIM_BDTR_BKP;
    if (aoe)                                            bdtr |= TIM_BDTR_AOE;
    /* MOE last (still in the same write): outputs should only actually go
     * live once break/lock/deadtime are already in place. */
    if (moe)                                            bdtr |= TIM_BDTR_MOE;

    timer->BDTR = bdtr;

    return TIM_OK;
}

