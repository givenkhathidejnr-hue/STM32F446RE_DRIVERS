#include "nvic.h"

/* STM32F4 implements 4 priority bits */
#define NVIC_PRIO_MASK ((1U << __NVIC_PRIO_BITS) - 1U)

/* ===================== Priority grouping ===================== */

/**
 * @note  AIRCR is a "protected" register - any write that doesn't have
 *        0x5FA in its top 16 bits (VECTKEY) is silently ignored by the
 *        hardware, which is why every write here re-supplies VECTKEY even
 *        though it's only "unlocking" a register we already read from.
 *        The read-modify-write (reg = SCB->AIRCR; ...; SCB->AIRCR = reg)
 *        preserves the other fields in AIRCR (endianness, reset request
 *        bits) that this function isn't meant to touch.
 */
void nvic_set_priority_grouping(nvic_group_t group)
{
    uint32_t reg = SCB->AIRCR;

    reg &= ~SCB_AIRCR_PRIGROUP_Msk;
    reg |= (0x5FAUL << SCB_AIRCR_VECTKEY_Pos); /* Unlock */
    reg |= ((uint32_t)group << SCB_AIRCR_PRIGROUP_Pos);

    SCB->AIRCR = reg;
}

/* ===================== Priority encoding ===================== */

/**
 * @brief Encode a preempt/sub-priority pair into the single priority value
 *        NVIC_SetPriority() expects, honoring whatever grouping was last
 *        set by nvic_set_priority_grouping().
 * @note  IRQn_Type values <= 0 (SysTick_IRQn and the other core exceptions)
 *        don't participate in the preempt/sub-priority split at all - they
 *        take a flat priority value directly, handled by the early-out
 *        below.
 */
void nvic_set_priority(IRQn_Type irq, uint8_t preempt_prio, uint8_t sub_prio)
{
    /* Sub-priority ignored for system handlers. */
    if (irq <= SysTick_IRQn) {
        NVIC_SetPriority(irq, preempt_prio);
        return;
    }

    /* Get current priority grouping */
    uint32_t group          = (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) >> SCB_AIRCR_PRIGROUP_Pos;

    /* Calculate available bits */
    uint32_t preempt_bits, sub_bits;

    if (group > (7U - __NVIC_PRIO_BITS)) {
        preempt_bits = __NVIC_PRIO_BITS;
    } else {
        preempt_bits = 7U - group;
    }

    if (preempt_bits > __NVIC_PRIO_BITS)
        preempt_bits = __NVIC_PRIO_BITS;

    sub_bits = __NVIC_PRIO_BITS - preempt_bits;

    /* If no sub-priority bits, ignore sub_prio */
    if (sub_bits == 0) {
        sub_prio = 0;
    }

    /* Validate inputs. Traps (rather than returning an error) because this
     * is always a compile-time programming mistake - a priority value that
     * doesn't fit in the currently-configured number of bits - never
     * runtime/input data, so failing loudly and immediately during bring-up
     * is more useful than silently clamping or propagating a status code
     * nobody's likely to check. */
    if (preempt_prio        >= (1 << preempt_bits) || sub_prio >= (1 << sub_bits)) while(1) __asm volatile ("nop");


    uint32_t priority       = (preempt_prio << sub_bits) | sub_prio;

    // Clamp to implemented bits
    priority &= NVIC_PRIO_MASK;

    // CMSIS handles the shift into upper nibble
    NVIC_SetPriority(irq, priority);
}