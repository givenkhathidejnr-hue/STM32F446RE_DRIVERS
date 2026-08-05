/**
 * @file button.c
 * @brief Interrupt-driven, mask-based button debouncing.
 *
 * State machine, per button:
 *   1. Any edge on the button's EXTI line fires button_exti_callback() -
 *      it immediately masks that EXTI line (so further bounces produce no
 *      more interrupts) and timestamps the event.
 *   2. Every 1ms SysTick tick, button_handler() checks each "debouncing"
 *      button; once debounce_ms has elapsed since the edge, it samples the
 *      pin level exactly once, decides press/release/long-press from that
 *      single sample, and only THEN re-enables the EXTI line.
 * This means the pin is never read from ISR context (only from
 * button_handler(), always called from SysTick_Handler) and a button can
 * generate at most one event per debounce_ms window, however noisy the
 * mechanical contact is.
 */
#include "button.h"

/* ===================== Button events ===================== */
#define         MAX_LINES       16
static button_t* buttons[MAX_LINES];

/* ===================== Internal helpers ===================== */

static inline uint8_t button_read_level(button_t *btn)
{
    return gpio_read_pin(btn->port, btn->pin);
}

/* ===================== EXTI callback ===================== */

/**
 * @brief EXTI ISR-context callback: registered as this button's exti_config_t
 *        callback in button_init(). Deliberately does almost nothing - just
 *        masks the line and records "when" - all actual debounce logic and
 *        pin sampling happens later in button_handler().
 */
static void button_exti_callback(void *ctx)
{
    button_t *btn = (button_t *)ctx;

    /* Clear pending first */
    exti_clear_pending((exti_interrupts_t)btn->line);
    exti_disable_line((exti_interrupts_t)btn->line);

    /* Mark as debouncing and timestamp the event */
    btn->debouncing     = 1;
    btn->last_change_ms = systick_get_ms();
}

/* ===================== Initialization ===================== */
/* GPIO and EXTI configuration*/
void button_init(button_t *btn)
{
    if (btn == NULL)    return;

    btn->line = (exti_interrupts_t)__builtin_ctz(btn->pin);

    /* GPIO init */
    gpio_config_t button = (gpio_config_t)  {
        .port   =   btn->port,
        .pins   =   btn->pin,
        .mode   =   GPIO_MODE_INPUT,
        .pull   =   btn->pull
    };

    /* Initialize the push button*/
    gpio_init(&button);

    /* Initialize runtime state */
    btn->stable_level   = button_read_level(btn);
    btn->debouncing     = 0;
    btn->press_start_ms = 0;

    /* EXTI config
     * .port derives the GPIOx port index straight from the port's base
     * address (GPIOA=AHB1PERIPH_BASE, GPIOB=+0x400, GPIOC=+0x800, ...) so
     * the caller never has to redundantly specify both btn->port and an
     * exti_port_t. Parenthesized as one expression before the final cast -
     * "(exti_port_t)x / 0x400" would (harmlessly, today, only because of
     * implicit int promotion) look like the cast applies to the whole
     * division when it doesn't.
     * @note STM32H7 note: GPIO moves off AHB1 onto AHB4, so both
     *       AHB1PERIPH_BASE and the assumption of a flat 0x400-per-port
     *       stride need re-deriving against the H7 memory map before this
     *       trick can be reused as-is. */
    exti_config_t exti = {
        .port          = (exti_port_t)(((uintptr_t)btn->port - AHB1PERIPH_BASE) / 0x400),
        .line          = btn->line,
        .trigger       = btn->trigger,
        .preempt_prio  = btn->preempt_prio,
        .sub_prio      = btn->sub_prio,
        .callback      = button_exti_callback,
        .context       = btn
    };

    exti_init(&exti);

    /* Add button to the list*/
    buttons[btn->line] = btn;
}


/**
 * @brief Called once per ms from SysTick_Handler() for every registered
 *        button; only does real work for buttons currently "debouncing".
 * @note  held_time computation intentionally uses unsigned wraparound
 *        (now - btn->press_start_ms): both are systick_get_ms() values, a
 *        monotonic counter free-running over the full 32-bit range, so the
 *        subtraction is correct even across a wrap - unlike, e.g., reading
 *        a small hardware auto-reload counter (see main.c's REVIEW_NOTES.md
 *        entry for a case where that assumption did NOT hold).
 */
void button_handler(void)
{
    for (uint32_t index = 0; index < MAX_LINES; index++) {
        button_t *btn = buttons[index];
        
        if (btn == NULL)    continue;

        if (!btn->debouncing)
            continue;

        uint32_t now = systick_get_ms();

        if ((now - btn->last_change_ms) < btn->debounce_ms)
            continue;

        // Debounce period finished
        uint8_t level = gpio_read_pin(btn->port, btn->pin);

        btn->debouncing = 0;

        // ────────────────────────────────────────────────
        // Process stable level FIRST
        // ────────────────────────────────────────────────

        if (level == btn->active_level &&
            btn->stable_level != btn->active_level) {

            btn->press_start_ms = now;
            btn->stable_level = level;

            if (btn->callback)
                btn->callback(BUTTON_EVENT_PRESS, btn->context);
        }
        else if (level != btn->active_level &&
                 btn->stable_level == btn->active_level) {

            uint32_t held_time = now - btn->press_start_ms;
            btn->stable_level = level;

            if (btn->callback) {
                if (held_time >= btn->long_press_ms)
                    btn->callback(BUTTON_EVENT_LONG_PRESS, btn->context);
                else
                    btn->callback(BUTTON_EVENT_RELEASE, btn->context);
            }
        }

        // ────────────────────────────────────────────────
        // ONLY NOW re-enable EXTI
        // ────────────────────────────────────────────────
        exti_enable_line((exti_interrupts_t)btn->line);
    }
}