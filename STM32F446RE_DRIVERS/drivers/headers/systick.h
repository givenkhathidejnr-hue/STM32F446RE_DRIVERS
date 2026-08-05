#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>
#include "device.h"
#include "nvic.h"
#include "button.h"
#include "systeminit.h"

/* Constants */
#define SYSTICK_MAX_RELOAD   0x00FFFFFFUL

/* ===================== Types ===================== */

typedef uint32_t systick_time_ms_t;

typedef enum {
    SYSTICK_CLK_HCLK_DIV8,
    SYSTICK_CLK_HCLK
} systick_clk_t;

/* ===================== API ===================== */

/* Initialize SysTick to generate 1ms ticks */
void systick_init(const systick_clk_t clk,
                  uint8_t preempt_prio);

/* Blocking delay */
void systick_delay(systick_time_ms_t ms);

/* Monotonic system uptime (ms) */
systick_time_ms_t systick_get_ms(void);

#endif /* SYSTICK_H */
