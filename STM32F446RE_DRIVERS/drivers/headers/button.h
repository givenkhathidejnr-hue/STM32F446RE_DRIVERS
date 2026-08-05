#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include "device.h"
#include "systick.h"
#include "gpio.h"
#include "exti.h"

/* ===================== Button events ===================== */

typedef enum {
    BUTTON_EVENT_PRESS,
    BUTTON_EVENT_RELEASE,
    BUTTON_EVENT_LONG_PRESS
} button_event_t;

/* ===================== Callback ===================== */

typedef void (*button_callback_t)(button_event_t event, void *ctx);

/* ===================== Button descriptor ===================== */
typedef struct {
    GPIO_TypeDef        *port;
    gpio_pin_mask_t     pin;
    gpio_pull_t         pull;
    uint8_t             active_level;   // 0 = active low, 1 = active high
    uint32_t            debounce_ms;
    uint32_t            long_press_ms;

    /* runtime state */
    volatile uint32_t            last_change_ms;   // When EXTI fired
    volatile uint32_t            press_start_ms;  // When press was confirmed
    volatile uint8_t             stable_level;    // Last confirmed pin level
    volatile uint8_t             debouncing;      // Are we waiting for debounce?
    uint8_t                      long_event_sent;  // Flag to avoid repeated long press


    /* EXTI configuration*/
    exti_trigger_t      trigger; 
    uint8_t             preempt_prio;
    uint8_t             sub_prio;
    exti_interrupts_t   line;

    /* Function callback */
    button_callback_t   callback;
    void                *context;
} button_t;

void button_init(button_t *btn);
void button_handler(void);

#endif
