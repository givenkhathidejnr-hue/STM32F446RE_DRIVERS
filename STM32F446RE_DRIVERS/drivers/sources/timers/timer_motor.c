/**
 * @file timer_motor.c
 * @brief 3-phase complementary PWM setup for brushless/BLDC-style motor
 *        control, built entirely on top of the already-existing
 *        timer_pwm.c and timer_advanced.c building blocks - no new
 *        register-level bit math here, just sequencing driver calls once
 *        per phase.
 * @note  Only TIM1/TIM8 (the two advanced timers with BDTR/complementary
 *        outputs) are valid here, enforced transitively by
 *        timer_advanced_deadtime_config()/_break_config()/
 *        _complementary_config(), which all reject any other timer.
 * @note  This file used to be an empty stub - timer_motor_init() and
 *        timer_motor_set_duty() were declared in timer_motor.h but never
 *        defined anywhere, a link error waiting to happen for anyone who
 *        called them. See REVIEW_NOTES.md.
 */
#include "timer_motor.h"

/**
 * @brief Configure one phase: PWM channel + its complementary output,
 *        both active-high, edge-aligned, preloaded CCR for glitch-free
 *        duty updates while the timer is running.
 */
static timer_status_t timer_motor_configure_phase(TIM_TypeDef *tim,
                                                   timer_pwm_channel_t channel)
{
    timer_pwm_cfg_t pwm_cfg = {0};
    pwm_cfg.channel      = channel;
    pwm_cfg.pwm_mode     = TIMER_PWM_MODE_1;
    pwm_cfg.alignment    = PWM_ALIGNMENT_EDGE;
    pwm_cfg.counter_mode = PWM_COUNTER_MODE_UP;
    pwm_cfg.pulse        = 0U;            /* start at 0% duty - motor idle until timer_motor_set_duty() */
    pwm_cfg.oc_preload   = true;
    pwm_cfg.polarity     = PWM_POLARITY_ACTIVE_HIGH;

    timer_status_t status = timer_pwm_init(tim, &pwm_cfg);
    if (status != TIM_OK) return status;

    timer_pwm_enable(tim, channel);

    /* timer_advanced_channel_t and timer_pwm_channel_t share the same
     * 0-indexed CH1..CH4 numbering - this cast just re-labels the same
     * numeric channel index for the advanced-timer-specific API. */
    timer_complementary_config_t comp_cfg = {0};
    comp_cfg.timer                = tim;
    comp_cfg.channel              = (timer_advanced_channel_t)channel;
    comp_cfg.main_polarity        = TIMER_OUTPUT_POL_ACTIVE_HIGH;
    comp_cfg.comp_polarity        = TIMER_OUTPUT_POL_ACTIVE_HIGH;
    comp_cfg.enable_complementary = true;

    return timer_advanced_complementary_config(&comp_cfg);
}

/**
 * @brief Configure a 3-phase complementary PWM motor driver: time base,
 *        three PWM+complementary channel pairs (phase A/B/C -> CH1/2/3),
 *        dead-time, and (optionally) the break input.
 * @note  No prescaler field exists in timer_motor_cfg_t on purpose - PWM
 *        frequency here is simply (timer input clock) / (pwm_period + 1).
 *        Pick pwm_period against your timer's actual APB clock (e.g.
 *        clock_info.apb2_timer_clk for TIM1/TIM8) to land on the switching
 *        frequency your motor driver hardware needs; there's no
 *        intermediate division stage.
 * @note  Deliberately does NOT call timer_advanced_enable()/timer_start() -
 *        call timer_motor_enable_outputs() (timer_motor.h) once you're
 *        ready to actually drive the outputs. This mirrors the rest of
 *        the library's "init configures, a separate call starts"
 *        convention, and for a motor driver specifically gives you a safe
 *        window to double check dead-time/break configuration before MOE
 *        (and therefore real current) goes live.
 */
timer_status_t timer_motor_init(const timer_motor_cfg_t *cfg)
{
    if (!cfg || !cfg->tim) return TIM_INVALID_PARAM;

    TIM_TypeDef *tim = cfg->tim;

    timer_base_config_t base_cfg = {0};
    base_cfg.prescaler      = 0U;
    base_cfg.period         = cfg->pwm_period;
    base_cfg.period_preload = true;
    timer_status_t status   = timer_base_init(tim, &base_cfg);
    if (status != TIM_OK) return status;

    timer_deadtime_config_t dt_cfg = {0};
    dt_cfg.timer     = tim;
    dt_cfg.dead_time = cfg->dead_time;
    status = timer_advanced_deadtime_config(&dt_cfg);
    if (status != TIM_OK) return status;

    if (cfg->break_enable) {
        timer_break_config_t brk_cfg = {0};
        brk_cfg.timer                   = tim;
        brk_cfg.enable_break            = true;
        brk_cfg.break_polarity          = cfg->break_polarity;
        /* AOE=false: after a break event trips, outputs stay disabled
         * until something explicitly re-arms them (timer_motor_enable_
         * outputs()) rather than the hardware silently resuming on its
         * own the moment the break condition clears. */
        brk_cfg.automatic_output_enable = false;
        brk_cfg.lock_configuration      = TIMER_LOCK_LEVEL_1;
        status = timer_advanced_break_config(&brk_cfg);
        if (status != TIM_OK) return status;
    }

    status = timer_motor_configure_phase(tim, TIMER_PWM_CHANNEL_1);
    if (status != TIM_OK) return status;
    status = timer_motor_configure_phase(tim, TIMER_PWM_CHANNEL_2);
    if (status != TIM_OK) return status;
    status = timer_motor_configure_phase(tim, TIMER_PWM_CHANNEL_3);
    if (status != TIM_OK) return status;

    return TIM_OK;
}

/**
 * @brief Set one phase's duty cycle, scaled 0-1000 (0 = always off, 1000 =
 *        always on) against the timer's currently configured period, so
 *        callers don't need to know the raw ARR value or recompute it if
 *        pwm_period ever changes.
 */
timer_status_t timer_motor_set_duty(TIM_TypeDef *tim,
                                    timer_motor_phase_t phase,
                                    uint16_t duty)
{
    if (!tim || phase > TIMER_MOTOR_PHASE_C || duty > 1000U)
        return TIM_INVALID_PARAM;

    uint32_t period = timer_get_period(tim) + 1U;   /* ARR holds (period - 1) */
    uint32_t pulse  = ((uint32_t)duty * period) / 1000U;

    timer_pwm_set_pulse(tim, (timer_pwm_channel_t)phase, pulse);

    return TIM_OK;
}
