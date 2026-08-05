#ifndef TIMER_IRQ_H
#define TIMER_IRQ_H

#include "timer.h"

/**
 * @file timer_irq.h
 * @brief Generic per-timer, per-event callback registration + dispatch, so
 *        application code never writes a TIMx_IRQHandler by hand - just
 *        timer_set_event_callback()/timer_register_callbacks() plus the
 *        matching *_irq_enable() below, and enable the IRQ in NVIC.
 * @note  Weak default handlers that call timer_irq_handler(TIMx) already
 *        exist for every timer vector in timer_irq_handlers.c - you only
 *        need to write your own TIMx_*Handler if you want to bypass this
 *        dispatch entirely for that timer.
 */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus*/

typedef enum    {
    TIMER_EVENT_UPDATE  = 0,
    TIMER_EVENT_CC1     = 1,
    TIMER_EVENT_CC2     = 2,
    TIMER_EVENT_CC3     = 3,
    TIMER_EVENT_CC4     = 4,
    TIMER_EVENT_TRIGGER = 5,
    
    // advanced timer specific (TIM1 & TIM8 only)
    TIMER_EVENT_BREAK    = 6,       // Break input (BIF / B2IF)
    TIMER_EVENT_COMMUT   = 7,       // Commutation event (COMIF)

    TIMER_EVENT_COUNT
} timer_event_t;

/* =====================================================================
* Interrupt Callback
* ===================================================================== */
typedef void (*timer_callback_t)(TIM_TypeDef *timer);

/* callbacks per timer, per event */
typedef struct {
    timer_callback_t events[TIMER_EVENT_COUNT];
} timer_callbacks_t;

/* Public API */
timer_status_t timer_register_callbacks(TIM_TypeDef *timer, const timer_callbacks_t *callbacks);
timer_status_t timer_set_event_callback(TIM_TypeDef *timer, timer_event_t event, timer_callback_t callback);

void timer_irq_handler(TIM_TypeDef *tim);   // called from each TIMx_IRQHandler

/* =====================================================================
* Interrupt Control
* ===================================================================== */
#define __CLEAR_FLAG(__REGISTER__, __FLAG__)    (__REGISTER__ &= ~__FLAG__)

__STATIC_FORCEINLINE void timer_update_irq_enable(TIM_TypeDef *timer)       { timer->DIER   |=  TIM_DIER_UIE; }
__STATIC_FORCEINLINE void timer_update_irq_disable(TIM_TypeDef *timer)      { timer->DIER   &=  ~TIM_DIER_UIE_Msk; }
__STATIC_FORCEINLINE void timer_cc1_irq_enable(TIM_TypeDef *timer)          { timer->DIER   |=  TIM_DIER_CC1IE; }
__STATIC_FORCEINLINE void timer_cc1_irq_disable(TIM_TypeDef *timer)         { timer->DIER   &=  ~TIM_DIER_CC1IE_Msk; }
__STATIC_FORCEINLINE void timer_cc2_irq_enable(TIM_TypeDef *timer)          { timer->DIER   |=  TIM_DIER_CC2IE; }
__STATIC_FORCEINLINE void timer_disable_cc2_irq(TIM_TypeDef *timer)         { timer->DIER   &=  ~TIM_DIER_CC2IE_Msk; }
__STATIC_FORCEINLINE void timer_cc3_irq_enable(TIM_TypeDef *timer)          { timer->DIER   |=  TIM_DIER_CC3IE; }
__STATIC_FORCEINLINE void timer_disable_cc3_irq(TIM_TypeDef *timer)         { timer->DIER   &=  ~TIM_DIER_CC3IE_Msk; }
__STATIC_FORCEINLINE void timer_cc4_irq_enable(TIM_TypeDef *timer)          { timer->DIER   |=  TIM_DIER_CC4IE; }
__STATIC_FORCEINLINE void timer_disable_cc4_irq(TIM_TypeDef *timer)         { timer->DIER   &=  ~TIM_DIER_CC4IE_Msk; }
__STATIC_FORCEINLINE void timer_trigger_irq_enable(TIM_TypeDef *timer)      { timer->DIER   |=  TIM_DIER_TIE; }
__STATIC_FORCEINLINE void timer_disable_trigger_irq(TIM_TypeDef *timer)     { timer->DIER   &=  ~TIM_DIER_TIE_Msk; }

/* ADVANCE TIMER INTERRUPTS CONTROL */
__STATIC_FORCEINLINE void timer_break_irq_enable(TIM_TypeDef *timer)        { timer->DIER |= TIM_DIER_BIE; }
__STATIC_FORCEINLINE void timer_break_irq_disable(TIM_TypeDef *timer)       { timer->DIER &= ~TIM_DIER_BIE; }
__STATIC_FORCEINLINE void timer_commutation_irq_enable(TIM_TypeDef *timer)  { timer->DIER |= TIM_DIER_COMIE; }
__STATIC_FORCEINLINE void timer_commutation_irq_disable(TIM_TypeDef *timer) { timer->DIER &= ~TIM_DIER_COMIE; }



#ifdef __cplusplus
}
#endif  /* __cplusplus*/

#endif  /* TIMER_IRQ_H */