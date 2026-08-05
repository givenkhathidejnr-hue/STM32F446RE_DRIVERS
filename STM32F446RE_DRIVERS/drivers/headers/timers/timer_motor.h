/* ========================= timer_motor.h ========================= */
#ifndef TIMER_MOTOR_H
#define TIMER_MOTOR_H


#include "timer.h"
#include "timer_pwm.h"
#include "timer_advanced.h"

/**
 * @file timer_motor.h
 * @brief Thin, opinionated 3-phase complementary PWM setup for BLDC/PMSM-
 *        style motor control on TIM1/TIM8, layered on timer_pwm.h and
 *        timer_advanced.h. Not a full field-oriented-control or six-step
 *        commutation implementation - just the timer/output plumbing
 *        (dead-time, break, three complementary PWM pairs) that any such
 *        higher-level control loop would sit on top of.
 */


/* Motor PWM channels */
typedef enum {
    TIMER_MOTOR_PHASE_A = 0,
    TIMER_MOTOR_PHASE_B,
    TIMER_MOTOR_PHASE_C
} timer_motor_phase_t;


/* Motor configuration */
typedef struct {
    TIM_TypeDef *tim;

    uint16_t pwm_period;
    uint8_t dead_time;

    bool break_enable;
    timer_break_polarity_t break_polarity;
} timer_motor_cfg_t;

__STATIC_FORCEINLINE void timer_motor_enable_outputs(TIM_TypeDef *tim)
{
    timer_advanced_enable(tim);
    timer_start(tim);
}
__STATIC_FORCEINLINE void timer_motor_disable_outputs(TIM_TypeDef *tim)
{
    timer_stop(tim);
    timer_advanced_disable(tim);
}

/* Initialization */
timer_status_t timer_motor_init(const timer_motor_cfg_t *cfg);


/* Duty cycle control (0–1000 scaled) */
timer_status_t timer_motor_set_duty(TIM_TypeDef *tim,
                                    timer_motor_phase_t phase,
                                    uint16_t duty);
#endif