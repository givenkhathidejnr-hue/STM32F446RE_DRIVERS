/**
 * @file timer_irq.c
 * @brief Generic timer-event callback registry + IRQ dispatcher, so a
 *        TIMx_IRQHandler only ever needs one line: timer_irq_handler(TIMx).
 * @note  File renamed from timer_iqr.c (typo) - no functional change,
 *        purely a filename fix so it matches its own header (timer_irq.h)
 *        and its sibling timer_irq_handlers.c.
 */
#include "timer_irq.h"

/* Max simultaneous supported timer instances */
#define MAX_TIMER_INSTANCES  18

/* One timer_callbacks_t SLOT PER TIMER INSTANCE, indexed via
 * timer_get_index() below - NOT a dynamically allocated/linked list, so
 * registering callbacks for a timer that already has some just overwrites
 * that timer's single slot (see timer_register_callbacks()).
 *
 * @note  This used to be declared as an array of *pointers*
 *        (timer_callbacks_t *[...]), zero-initialized - meaning every
 *        entry was a NULL pointer that nothing ever pointed at real
 *        storage. timer_get_callbacks() would always hand back NULL, so
 *        timer_register_callbacks()/timer_set_event_callback() always hit
 *        their "!slot" check and returned TIM_INVALID_PARAM - the entire
 *        callback registry was a silent no-op, 100% of the time, for every
 *        timer. Declaring the table as actual storage (not pointers) and
 *        returning &timer_callback_table[idx] fixes it. */
static timer_callbacks_t timer_callback_table[MAX_TIMER_INSTANCES] = {0};

/* Map TIM_TypeDef* → table index. Returns -1 if not found/supported */
static inline int timer_get_index(TIM_TypeDef *timer)
{ 
    if (timer == TIM1)  return 0;
    if (timer == TIM2)  return 1;
    if (timer == TIM3)  return 2;
    if (timer == TIM4)  return 3;
    if (timer == TIM5)  return 4;
    if (timer == TIM6)  return 5;
    if (timer == TIM7)  return 6;
    if (timer == TIM8)  return 7;
    #if defined(TIM9)
        if (timer == TIM9)  return 8;
    #endif
    #if defined(TIM10)
        if (timer == TIM10) return 9;
    #endif
    #if defined(TIM11)
        if (timer == TIM11) return 10;
    #endif
    #if defined(TIM12)
        if (timer == TIM12) return 11;
    #endif
    #if defined(TIM13)
        if (timer == TIM13) return 12;
    #endif
        #if defined(TIM14)
        if (timer == TIM14) return 13;
    #endif
    #if defined(TIM15)
        if (timer == TIM15) return 14;
    #endif
    #if defined(TIM16)
        if (timer == TIM16) return 15;
    #endif
    #if defined(TIM17)
        if (timer == TIM17) return 16;
    #endif
    
    return -1;
}

/** @brief - searches for irq configuration, spcific to the given timer
 * @note - each timer has only one irq configuration stored
*/
static timer_callbacks_t * timer_get_callbacks(TIM_TypeDef *timer)
{
    int idx = timer_get_index(timer);
    if (idx < 0 || idx >= MAX_TIMER_INSTANCES) {
        return NULL;
    }
    return &timer_callback_table[idx];
}

/* Set ALL callbacks at once (most convenient when initializing) */
timer_status_t timer_register_callbacks(TIM_TypeDef *timer, const timer_callbacks_t *callbacks)
{
    timer_callbacks_t *slot = timer_get_callbacks(timer);
    if (!slot || !callbacks) {
        return TIM_INVALID_PARAM;
    }
    *slot = *callbacks;   // struct copy
    return TIM_OK;
}

/* Set single event callback (can be used multiple times) */
timer_status_t timer_set_event_callback(TIM_TypeDef *timer, timer_event_t event, timer_callback_t callback)
{
    if (event >= TIMER_EVENT_COUNT)     return TIM_INVALID_PARAM;

    timer_callbacks_t *slot = timer_get_callbacks(timer);
    if (!slot)      return TIM_INVALID_PARAM;

    slot->events[event] = callback;
    return TIM_OK;
}

/* ============================================================
 * IRQ dispatcher
 * ============================================================ */

/**
 * @brief Shared dispatch called from every TIMx_*Handler (see
 *        timer_irq_handlers.c) - checks each event flag that has both its
 *        status bit AND its DIER enable bit set, clears it, and invokes
 *        that event's registered callback (if any).
 * @note  Checking status AND enable together (rather than just status)
 *        matters on shared vectors: e.g. TIM1_BRK_TIM9_Handler calls this
 *        once for TIM1 and once for TIM9, and TIM9's SR bits are distinct
 *        registers from TIM1's, so this guard is really about not
 *        reacting to a flag that's set but whose interrupt was never
 *        actually asked for.
 */
void timer_irq_handler(TIM_TypeDef *timer)
{
    timer_callbacks_t *callbacks = timer_get_callbacks(timer);
    if (!callbacks) return;

    /* registers */
    volatile uint32_t status_register       = timer->SR;
    volatile uint32_t interrupt_register    = timer->DIER;

    /* COMMON EVENTS */
    /* update event */
    if ((status_register & TIM_SR_UIF) && (interrupt_register & TIM_DIER_UIE))
    {
        /* clear flag */
        __CLEAR_FLAG(timer->SR, TIM_SR_UIF);
        if (callbacks->events[TIMER_EVENT_UPDATE])  callbacks->events[TIMER_EVENT_UPDATE](timer);
    }
    
    /* Capture Compare 1 Event*/
    if ((status_register & TIM_SR_CC1IF) && (interrupt_register & TIM_DIER_CC1IE))
    {
        __CLEAR_FLAG(timer->SR, TIM_SR_CC1IF);
        if (callbacks->events[TIMER_EVENT_CC1])     callbacks->events[TIMER_EVENT_CC1](timer);  
    }

    /* Capture Compare 2 Event*/
    if ((status_register & TIM_SR_CC2IF) && (interrupt_register & TIM_DIER_CC2IE))
    {
        __CLEAR_FLAG(timer->SR, TIM_SR_CC2IF);
        if (callbacks->events[TIMER_EVENT_CC2])     callbacks->events[TIMER_EVENT_CC2](timer);  
    }
    /* Capture Compare 3 Event*/
    if ((status_register & TIM_SR_CC3IF) && (interrupt_register & TIM_DIER_CC3IE))
    {
        __CLEAR_FLAG(timer->SR, TIM_SR_CC3IF);
        if (callbacks->events[TIMER_EVENT_CC3])     callbacks->events[TIMER_EVENT_CC3](timer);  
    }

    /* Capture Compare 4 Event*/
    if ((status_register & TIM_SR_CC4IF) && (interrupt_register & TIM_DIER_CC4IE))
    {
        __CLEAR_FLAG(timer->SR, TIM_SR_CC4IF);
        if (callbacks->events[TIMER_EVENT_CC4])     callbacks->events[TIMER_EVENT_CC4](timer);  
    }

    /* Trigger Event*/
    if ((status_register & TIM_SR_TIF) && (interrupt_register & TIM_DIER_TIE))
    {
        __CLEAR_FLAG(timer->SR, TIM_SR_TIF);
        if (callbacks->events[TIMER_EVENT_TRIGGER]) callbacks->events[TIMER_EVENT_TRIGGER](timer);  
    }

    // ── Advanced timer only ──────────────────────────────────
    // These bits are reserved (usually read 0) on non-advanced timers → safe to check

    // Break event(s)
    if ((status_register & TIM_SR_BIF)  && (interrupt_register & TIM_DIER_BIE)) {     // B2IF/B2IE only on some newer series
        __CLEAR_FLAG(timer->SR, TIM_SR_BIF);              // clear both to be safe
        if (callbacks->events[TIMER_EVENT_BREAK]) callbacks->events[TIMER_EVENT_BREAK](timer);
    }

    // Commutation event (used in 6-step BLDC motor commutation)
    if ((status_register & TIM_SR_COMIF) && (interrupt_register & TIM_DIER_COMIE)) {
        __CLEAR_FLAG(timer->SR, TIM_SR_COMIF);
        if (callbacks->events[TIMER_EVENT_COMMUT]) callbacks->events[TIMER_EVENT_COMMUT](timer);
    }
}




