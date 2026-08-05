/* ============================= timer_sync.h ============================= */


#ifndef TIMER_SYNC_H
#define TIMER_SYNC_H


#include "timer.h"

/**
 * @file timer_sync.h
 * @brief Master/slave timer synchronization via TRGO (trigger output) and
 *        ITRx (internal trigger input) routing - lets one timer's events
 *        (update, enable, ...) drive another timer's behavior, or drive a
 *        non-timer peripheral like the ADC's external trigger, entirely in
 *        hardware with no CPU/interrupt involvement.
 * @note  This project's app/main.c demonstrates the most common pattern:
 *        a "master" timer's TRGO=UPDATE output wired to the ADC's
 *        EXTSEL=TIMx_TRGO external trigger, producing a hardware-clocked,
 *        jitter-free ADC sample rate. Slave-mode (timer_sync_slave_init)
 *        is for timer-to-timer chaining (e.g. one timer resets/gates/
 *        clocks another) rather than timer-to-ADC.
 */


/* Master trigger output selection (TRGO source) */
typedef enum {
    TIMER_TRGO_RESET = 0,   /* High when an UG event occurs */
    TIMER_TRGO_ENABLE = 1,  /* Trigger becomes high when CEN is set */
    TIMER_TRGO_UPDATE = 2   /* Trigger becomes high on every update event*/
} timer_trgo_t;


/* Slave synchronization mode */
typedef enum {
    TIMER_SLAVE_DISABLED = 0,
    TIMER_SLAVE_RESET_MODE = 4,
    TIMER_SLAVE_GATED_MODE = 5,
    TIMER_SLAVE_TRIGGER_MODE = 6,
    TIMER_SLAVE_EXTERNAL_CLK = 7
} timer_slave_mode_sync_t;


/* Internal trigger routing (ITRx lines) */
typedef enum {
    TIMER_ITR0 = 0,
    TIMER_ITR1,
    TIMER_ITR2,
    TIMER_ITR3
} timer_itr_t;


/* Master configuration */
typedef struct {
    TIM_TypeDef     *timer;
    timer_trgo_t    trigger_output;
} timer_master_sync_cfg_t;

/* Slave configuration */
typedef struct {
    TIM_TypeDef             *timer;
    timer_slave_mode_sync_t mode;
    timer_itr_t             internal_trigger;
} timer_slave_sync_cfg_t;


timer_status_t timer_sync_master_init(const timer_master_sync_cfg_t *cfg);
timer_status_t timer_sync_slave_init(const timer_slave_sync_cfg_t *cfg);

#endif  /* TIMER_SYNC_H */