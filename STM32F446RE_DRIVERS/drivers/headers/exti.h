#ifndef EXTI_H
#define EXTI_H

#include <stdint.h>
#include "device.h"
#include "nvic.h"
#include "rcc.h"

/**
 * @file exti.h
 * @brief External interrupt line configuration + a per-line callback
 *        registry, so application code never needs to touch EXTIx_IRQn
 *        vectors directly.
 * @note  exti_init() enables the SYSCFG clock itself (needed for the
 *        EXTICR port-select registers), but NOT the GPIO port's own clock
 *        - configure the pin as an input with gpio_init() (and enable that
 *        port's RCC clock) before calling exti_init() for it.
 * @note  Lines 0-15 map 1:1 to a GPIO pin *number* (not a specific port) -
 *        e.g. line 3 can be routed from PA3, PB3, ... PH3, but only one of
 *        those at a time, and only one GPIO pin total can ever use EXTI
 *        line 3. Two buttons that happen to share a pin number on
 *        different ports cannot both use interrupts simultaneously.
 */


/* =====================================================================
 * Macros
 * ===================================================================== */
#define EXTI_LINE_MASK(line)     (1UL << (line))

/* =====================================================================
* LIST OF EXTERNAL INTERRUPTS 
* STM32F446 has 16 external interrupts associated with GPIO pins
* Only pin number x can be selected as external interrupt number x 
* (in EXTIx) 
* Eg. Only PA3, PB3, PC3, ... PH3 can trigger EXTI3, but cannot be used a
* at the same time
* ===================================================================== */
typedef enum    {
    EXT_INT_0 = 0,  EXT_INT_1 = 1,  EXT_INT_2 = 2,  EXT_INT_3 = 3,  
    EXT_INT_4 = 4,  EXT_INT_5 = 5,  EXT_INT_6 = 6,  EXT_INT_7 = 7,  
    EXT_INT_8 = 8,  EXT_INT_9 = 9,  EXT_INT_10 = 10,EXT_INT_11 = 11, 
    EXT_INT_12 = 12,EXT_INT_13 = 13,EXT_INT_14 = 14,EXT_INT_15 = 15,
} exti_interrupts_t;

/* =====================================================================
* Configure the interrupts to trigger on rising, falling edge or both
* ===================================================================== */
typedef enum    {
    EXTI_TRIGGER_RISING,
    EXTI_TRIGGER_FALLING,
    EXTI_TRIGGER_BOTH
} exti_trigger_t;

/* =====================================================================
* GPIO Ports
* ===================================================================== */
typedef enum    {
    EXTI_PORT_A=0,
    EXTI_PORT_B=1,
    EXTI_PORT_C=2,
    EXTI_PORT_D=3,
    EXTI_PORT_E=4,
    EXTI_PORT_F=5,
    EXTI_PORT_G=6,
    EXTI_PORT_H=7
} exti_port_t;

/* =====================================================================
* Interrupt configuration struct
* ===================================================================== */
/*Callback function for the user to define*/
typedef void (*exti_callback_t)(void *context);

typedef struct 
{
    exti_port_t         port;           /*GPIO port eg. GPIOA*/
    exti_interrupts_t   line;            /*GPIO pin eg GPIO pin 1*/
    exti_trigger_t      trigger;        /*Trigger type: Rising or Falling edge*/
    uint8_t             preempt_prio;
    uint8_t             sub_prio;
    exti_callback_t     callback;       /*Function to be executed during ISR*/
    void               *context;        /*Parameter to the callback function. NULL if not used*/
} exti_config_t;

/* =====================================================================
* Enable and Disable interrrupt and event masks
* ===================================================================== */
/*Enable interrupt mask*/
__STATIC_FORCEINLINE void exti_enable_line(exti_interrupts_t line)    {
    EXTI->IMR |= EXTI_LINE_MASK(line);
}

/*Disable interrupt mask*/
__STATIC_FORCEINLINE void exti_disable_line(exti_interrupts_t line)    {
    EXTI->IMR &= ~EXTI_LINE_MASK(line);
}

/*Enable event mask*/
__STATIC_FORCEINLINE void exti_enable_event(exti_interrupts_t line)    {
    EXTI->EMR |= EXTI_LINE_MASK(line);
}

__STATIC_FORCEINLINE void exti_clear_pending(exti_interrupts_t line)    {
    EXTI->PR = EXTI_LINE_MASK(line);
}

/* =====================================================================
* Configure Interupts
* ===================================================================== */
void exti_init(const exti_config_t *config);

/*Used internally by ISR handlers*/
void exti_handle_irq(exti_interrupts_t line);

#endif /* EXTI_H */