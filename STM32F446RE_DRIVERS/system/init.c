#include "init.h"

/**
 * @brief One-time application bring-up, called once from main() before any
 *        peripheral drivers are touched.
 * @note  Order matters: priority grouping must be settled before any
 *        interrupt (including SysTick's) has its priority programmed,
 *        since nvic_set_priority()'s sub-priority split depends on the
 *        grouping already being in effect.
 */
void system_init(void)
{
    /* Configure how NVIC priority bits split into preempt/sub-priority
     * BEFORE any IRQ priority is programmed (see nvic_set_priority_grouping
     * in nvic.c - it directly affects how nvic_set_priority() interprets
     * its preempt_prio/sub_prio arguments). */
    nvic_set_priority_grouping(NVIC_GROUP_4);

    /* Start the 1ms SysTick tick (drives systick_get_ms() / systick_delay()
     * and, indirectly, button_handler()'s debounce timing). Preempt
     * priority 15 = lowest priority on this 4-bit-priority part, so
     * time-critical IRQs (e.g. motor control) can still preempt it. */
    systick_init(SYSTICK_CLK_HCLK, 15);
}
