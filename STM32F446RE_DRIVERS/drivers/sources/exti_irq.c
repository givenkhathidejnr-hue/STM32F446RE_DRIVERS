/**
 * @file exti_irq.c
 * @brief Vector-table entry points for all 16 EXTI lines, forwarding to
 *        exti_handle_irq(). Lines 0-4 each have a dedicated NVIC vector;
 *        lines 5-9 and 10-15 share one vector apiece (matches the
 *        STM32F4's actual interrupt controller layout - only 7 NVIC lines
 *        cover all 16 EXTI lines), so those two handlers scan for which
 *        specific line(s) are both pending AND unmasked before dispatching.
 */
#include "exti.h"
#include "core_cm4.h"

/* =====================================================================
 * EXTI individual lines (0–4)
 * ===================================================================== */

void EXTI0_IRQHandler(void)
{
    exti_handle_irq(EXT_INT_0);
}

void EXTI1_IRQHandler(void)
{
    exti_handle_irq(EXT_INT_1);
}

void EXTI2_IRQHandler(void)
{
    exti_handle_irq(EXT_INT_2);
}

void EXTI3_IRQHandler(void)
{
    exti_handle_irq(EXT_INT_3);
}

void EXTI4_IRQHandler(void)
{
    exti_handle_irq(EXT_INT_4);
}

/* =====================================================================
 * EXTI shared lines (5–9)
 * ===================================================================== */

void EXTI9_5_IRQHandler(void)
{
    uint32_t pending = EXTI->PR & EXTI->IMR;  // Respect interrupt mask
    pending &= 0x3E0;  // Limit to lines 5–9

    while (pending) {
        uint32_t line = __builtin_ctz(pending);  // 0–31, will be 5–9 here
        exti_handle_irq((exti_interrupts_t)line);
        pending &= pending - 1;  // Clear lowest set bit
    }
}
/* =====================================================================
 * EXTI shared lines (10–15)
 * ===================================================================== */

void EXTI15_10_IRQHandler(void)
{
    uint32_t pending = EXTI->PR & EXTI->IMR;
    pending &= 0xFC00;  // Bits 10–15

    while (pending) {
        uint32_t line = __builtin_ctz(pending);  // 10–15
        exti_handle_irq((exti_interrupts_t)line);
        pending &= pending - 1;
    }
}
