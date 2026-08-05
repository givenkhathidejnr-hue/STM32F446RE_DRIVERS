#ifndef     SYSTEMINIT_H
#define     SYSTEMINIT_H

#include "device.h"
#include "rcc.h"
#include "pwr.h"
#include "nvic.h"

/* Clock information will be stored in the clock section in ram*/
#define     __CLOCK__       __attribute__((section(".clock"), used))
#define     __DATA__        __attribute__((section (".data"), used))

/* struct to hold the clock information*/
typedef struct {
    uint32_t    sysclock;
    uint32_t    hclk;
    uint32_t    pclk_1;
    uint32_t    pclk_2;
    uint32_t    apb1_timer_clk;
    uint32_t    apb2_timer_clk;
} clock_info_t;


#endif      /* SYSTEMINIT_H */