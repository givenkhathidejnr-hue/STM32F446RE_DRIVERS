#include "exti.h"

/* =====================================================================
 * Internal storage
 * ===================================================================== */
#define MAX_EXTI_LINES 16
static exti_callback_t callbacks[MAX_EXTI_LINES]    = {0};
static void *contexts[MAX_EXTI_LINES]               = {0};

/* =====================================================================
 * Macros
 * ===================================================================== */
#ifdef DEBUG
#define EXTI_ASSERT(x) do { if (!(x)) while(1); } while(0)
#else
#define EXTI_ASSERT(x) do { (void)(x); } while(0)
#endif
/* =====================================================================
 * Internal helpers
 * ===================================================================== */

/* Configure rising / falling trigger */
__STATIC_INLINE void exti_configure_trigger(exti_trigger_t trigger, exti_interrupts_t line)    {
    
    /* Clear existing trigger configuration */
    EXTI->RTSR &= ~EXTI_LINE_MASK(line);
    EXTI->FTSR &= ~EXTI_LINE_MASK(line);
    
    switch(trigger) {
        case EXTI_TRIGGER_RISING:       EXTI -> RTSR  |=  EXTI_LINE_MASK(line);      break;
        case EXTI_TRIGGER_FALLING:      EXTI -> FTSR  |=  EXTI_LINE_MASK(line);      break;
        case EXTI_TRIGGER_BOTH:         
                EXTI -> FTSR  |=  EXTI_LINE_MASK(line);
                EXTI -> RTSR  |=  EXTI_LINE_MASK(line);
                break;
    }
}

/* Configure GPIO port to EXTI line mapping */
__STATIC_INLINE void exti_configure_gpio(exti_port_t port, exti_interrupts_t line)   {
    const uint32_t index = line >> 2U;          /* EXTICR index */
    const uint32_t shift = (line & 0x3U) << 2U;   /* Bit offset */

    /* Clear previous mapping */
    SYSCFG->EXTICR[index] &= ~(0xFUL << shift);
    /* Set new mapping */
    SYSCFG->EXTICR[index] |=  ((uint32_t)port << shift);
}


/**
 * @brief Configure one EXTI line end to end: GPIO->EXTI routing, trigger
 *        edge, callback registration, and NVIC enable.
 * @note  Ordering is deliberate: the line is masked and any stale pending
 *        flag cleared BEFORE the trigger/routing is (re)configured, and
 *        only unmasked (exti_enable_line) as the very last step - so
 *        reconfiguring an already-active line can never fire a spurious
 *        interrupt on half-applied settings.
 */
void exti_init(const exti_config_t *config)   {
    EXTI_ASSERT(config != NULL);

    EXTI_ASSERT(config->line <= EXT_INT_15);
    const exti_interrupts_t line = config->line;

    /* Enable SYSCFG clock */
    rcc_clock_control(RCC_SYSCFG, RCC_CLOCK_ENABLE);

    /* Mask EXTI line during configuration */
    exti_disable_line(line);

    /* Clear pending interrupt */
    exti_clear_pending(line);

    /* Configure GPIO → EXTI routing */
    EXTI_ASSERT(config->port <= EXTI_PORT_H);
    exti_configure_gpio(config->port, line);

    /* Configure trigger */
    exti_configure_trigger(config->trigger, line);

    /* Register callback and context */
    callbacks[line] = config->callback;
    contexts[line]  = config->context;

    /* Enable NVIC IRQ */
    IRQn_Type irq =
        (line <= 4)  ? (EXTI0_IRQn + line) :
        (line <= 9)  ? EXTI9_5_IRQn :
                       EXTI15_10_IRQn;

    nvic_set_priority(irq,
                      config->preempt_prio,
                      config->sub_prio);

    nvic_enable_irq(irq);

    /* Unmask EXTI line (LAST STEP) */
    exti_enable_line(line);
}

/**
 * @brief Shared dispatch used by every EXTIx_IRQHandler (see exti_irq.c) -
 *        checks and clears the pending flag for @p line, then invokes its
 *        registered callback (if any) with its registered context.
 */
void exti_handle_irq(exti_interrupts_t line)
{
    if (EXTI->PR & EXTI_LINE_MASK(line)) {
        EXTI->PR = EXTI_LINE_MASK(line);  // clear pending

        /* Call callback */
        if (line < MAX_EXTI_LINES && callbacks[line]) {
            callbacks[line](contexts[line]);
        }
    }

    __DSB();
}

