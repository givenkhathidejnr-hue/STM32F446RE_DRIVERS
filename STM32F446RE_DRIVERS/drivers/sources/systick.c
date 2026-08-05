#include "systick.h"

/* System Core clock frequency*/
extern uint32_t SystemCoreClock;
/* ===================== Internal state ===================== */

static volatile systick_time_ms_t systick_ms    = 0;
static volatile systick_time_ms_t delay_counter = 0;

/* ===================== API =====================
 * @note  Uses CMSIS's own SysTick_CTRL_*_Msk macros (core_cm4.h) rather
 *        than re-declaring the same three bit positions locally - one less
 *        place for the two definitions to quietly drift apart. */

void systick_init(systick_clk_t clk,
                  uint8_t preempt_prio)
{
    uint32_t ticks = 0;

    /* Disable SysTick */
    SysTick->CTRL = 0;

    /* Calculate reload for 1ms tick */
    if (clk == SYSTICK_CLK_HCLK)    ticks = SystemCoreClock / 1000U;
    
    else                            ticks = (SystemCoreClock / 8U) / 1000U;

    if (ticks == 0 || ticks > SYSTICK_MAX_RELOAD) {
        while (1)   __asm volatile ("nop"); /* invalid config */
    }

    SysTick->LOAD = ticks - 1U;
    SysTick->VAL  = 0;

    /* Configure clock source */
    if (clk == SYSTICK_CLK_HCLK) {
        SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
    }
    else    SysTick->CTRL &= ~SysTick_CTRL_CLKSOURCE_Msk;

    /* NVIC configuration. SysTick has no sub-priority (system exceptions
     * use NVIC_SetPriority's flat priority field, see nvic_set_priority()'s
     * early-out for irq <= SysTick_IRQn), so the third argument is unused -
     * pass 0U rather than NULL: NULL is a null *pointer* constant, and
     * passing it where a uint8_t is expected relies on an implementation-
     * defined pointer-to-integer conversion that most compilers only warn
     * about rather than reject. */
    nvic_set_priority(SysTick_IRQn, preempt_prio, 0U);
    nvic_enable_irq(SysTick_IRQn);

    /* Enable SysTick */
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    __DSB();
}

/** 
 * @note WARNING: Non-reentrant. Do not call from ISR. 
 */
void systick_delay(systick_time_ms_t ms)
{
    delay_counter = ms;
    while (delay_counter != 0U) {
        __asm volatile ("nop");
    }
}

/**
 * @note  No PRIMASK critical section needed here: systick_ms is a naturally-
 *        aligned 32-bit word, and a 32-bit load on Cortex-M is always
 *        atomic with respect to the SysTick ISR (which only ever
 *        increments it by 1) - there's no way to observe a torn value.
 */
systick_time_ms_t systick_get_ms(void)
{
    return systick_ms;
}

/* ===================== ISR ===================== */

void SysTick_Handler(void)
{
    systick_ms++;
    button_handler();

    if (delay_counter > 0U) {
        delay_counter--;
    }
}
