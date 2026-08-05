#ifndef NVIC_H
#define NVIC_H

#include <stdint.h>
#include "device.h"

/**
 * @file nvic.h
 * @brief NVIC priority grouping + per-IRQ priority configuration.
 * @note  The STM32F446 (Cortex-M4) implements 4 priority bits per
 *        interrupt (__NVIC_PRIO_BITS == 4 in the CMSIS device header), so
 *        only 16 distinct priority levels exist regardless of which
 *        preempt/sub-priority split is chosen below - NVIC_GROUP_4 (4
 *        preempt bits) uses all 16 as preempt priorities with no
 *        sub-priority, while NVIC_GROUP_0 (4 sub bits) uses all 16 as
 *        sub-priorities within a single preempt level (no real preemption
 *        between IRQs at all in that configuration).
 * @note  Call nvic_set_priority_grouping() exactly ONCE, early (see
 *        system_init() in system/init.c), before any nvic_set_priority()
 *        call - changing the grouping after IRQ priorities have already
 *        been programmed re-interprets those same priority values under a
 *        different preempt/sub split, silently changing behavior.
 */

/* ===================== Priority grouping ===================== */
typedef enum {
    NVIC_GROUP_0 = 0x7, /* 0 preempt bits, 4 sub bits*/
    NVIC_GROUP_1 = 0x6, /* 1 preempt bits, 3 sub bits*/
    NVIC_GROUP_2 = 0x5, /* 2 preempt bits, 2 sub bits*/
    NVIC_GROUP_3 = 0x4, /* 3 preempt bits, 1 sub bits*/
    NVIC_GROUP_4 = 0x3  /* 4 preempt bits, 0 sub bits*/
} nvic_group_t;


/* ===================== IRQ control ===================== */

__STATIC_FORCEINLINE void nvic_enable_irq(IRQn_Type irq)
{
    NVIC_EnableIRQ(irq);
}

__STATIC_FORCEINLINE void nvic_disable_irq(IRQn_Type irq)
{
    NVIC_DisableIRQ(irq);
}

/* ===================== API ===================== */
void nvic_set_priority_grouping(nvic_group_t group);

void nvic_set_priority(IRQn_Type irq,
                       uint8_t preempt_prio,
                       uint8_t sub_prio);

#endif /* NVIC_H */
