/**
 * @file timer_irq_handlers.c
 * @brief Default (weak) vector-table entry points for every timer IRQ on
 *        the F446, each just forwarding to the generic timer_irq_handler()
 *        dispatcher (see timer_irq.c). Since they're __WEAK, application
 *        code can still define its own e.g. TIM2_Handler to bypass the
 *        dispatcher entirely for that one timer if needed.
 * @note  Names match this project's custom startup file
 *        (platform/stm32f446_startup.s uses "..._Handler", not the ST
 *        CMSIS-default "..._IRQHandler") - if you ever swap in a
 *        ST-supplied startup file instead, every name here needs the
 *        "_IRQ" reinserted to actually be linked into the vector table.
 */
#include "timer_irq.h"

/**
 * Timer interrupt handler
 */
/* Weak definitions — user can override if needed, but normally just calls dispatcher */
__WEAK void TIM1_CC_Handler(void)       { timer_irq_handler(TIM1); }  // separate for advanced
__WEAK void TIM2_Handler(void)          { timer_irq_handler(TIM2); }
__WEAK void TIM3_Handler(void)          { timer_irq_handler(TIM3); }
__WEAK void TIM4_Handler(void)          { timer_irq_handler(TIM4); }
__WEAK void TIM5_Handler(void)          { timer_irq_handler(TIM5); }
__WEAK void TIM6_DAC_Handler(void)      { timer_irq_handler(TIM6); }
__WEAK void TIM7_Handler(void)          { timer_irq_handler(TIM7); }
__WEAK void TIM8_CC_Handler(void)       { timer_irq_handler(TIM8); }


/* ────────────────────────────────────────────────
   TIM1 shared handlers — check which timer triggered
   The F4's NVIC groups TIM1's break/update/trigger-comm vectors together
   with TIM9/10/11's single (update-only) vectors respectively - one
   physical IRQ line, two unrelated timers. Each handler below has to
   manually check status+enable bits for BOTH timers and dispatch whichever
   one(s) actually fired, since there's no hardware way to tell them apart
   before reading registers.
   ──────────────────────────────────────────────── */

__WEAK void TIM1_BRK_TIM9_Handler(void)
{
    // TIM1 Break interrupt?
    if (TIM1->SR & (TIM_SR_BIF) && TIM1->DIER & (TIM_DIER_BIE )) {
        timer_irq_handler(TIM1);   // or dedicated break callback later
    }

    // TIM9 global interrupt?
    if ((TIM9->SR & TIM_SR_UIF) && (TIM9->DIER & TIM_DIER_UIE)) {
        timer_irq_handler(TIM9);
    }
}

__WEAK void TIM1_UP_TIM10_Handler(void)
{
    // TIM1 Update?
    if ((TIM1->SR & TIM_SR_UIF) && (TIM1->DIER & TIM_DIER_UIE)) {
        timer_irq_handler(TIM1);
    }

    // TIM10 global (usually update only)?
    if ((TIM10->SR & TIM_SR_UIF) && (TIM10->DIER & TIM_DIER_UIE)) {
        timer_irq_handler(TIM10);
    }
}

__WEAK void TIM1_TRG_COM_TIM11_Handler(void)
{
    // TIM1 Trigger / Commutation?
    if (((TIM1->SR & TIM_SR_TIF) && (TIM1->DIER & TIM_DIER_TIE)) ||
        ((TIM1->SR & TIM_SR_COMIF) && (TIM1->DIER & TIM_DIER_COMIE))) {
        timer_irq_handler(TIM1);
    }

    // TIM11 global
    if ((TIM11->SR & TIM_SR_UIF) && (TIM11->DIER & TIM_DIER_UIE)) {
        timer_irq_handler(TIM11);
    }
}

/* ────────────────────────────────────────────────
   TIM8 shared handlers — same pattern as TIM1
   ──────────────────────────────────────────────── */

__WEAK void TIM8_BRK_TIM12_Handler(void)
{
    if (TIM8->SR & TIM_SR_BIF && TIM8->DIER & TIM_DIER_BIE) {
        timer_irq_handler(TIM8);
    }
    if (TIM12->SR & TIM_SR_UIF && TIM12->DIER & TIM_DIER_UIE) {
        timer_irq_handler(TIM12);
    }
}

__WEAK void TIM8_UP_TIM13_Handler(void)
{
    if (TIM8->SR & TIM_SR_UIF && TIM8->DIER & TIM_DIER_UIE) {
        timer_irq_handler(TIM8);
    }
    if (TIM13->SR & TIM_SR_UIF && TIM13->DIER & TIM_DIER_UIE) {
        timer_irq_handler(TIM13);
    }
}

__WEAK void TIM8_TRG_COM_TIM14_Handler(void)
{
    if (((TIM8->SR & TIM_SR_TIF) && (TIM8->DIER & TIM_DIER_TIE)) ||
        ((TIM8->SR & TIM_SR_COMIF) && (TIM8->DIER & TIM_DIER_COMIE))) {
        timer_irq_handler(TIM8);
    }
    if (TIM14->SR & TIM_SR_UIF && TIM14->DIER & TIM_DIER_UIE) {
        timer_irq_handler(TIM14);
    }
}