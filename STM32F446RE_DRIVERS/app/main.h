#ifndef __MAIN_H
#define __MAIN_H

/**
 * @file main.h
 * @brief Umbrella include for the demo application - pulls in every
 *        driver module the demo in main.c touches. Individual driver
 *        headers already include what they each need internally (e.g.
 *        systick.h -> button.h); the list below is intentionally explicit
 *        rather than relying on those transitive includes, since main.c is
 *        meant to double as a "what does this library offer" reference.
 */

#include "rcc.h"
#include "gpio.h"
#include "pwr.h"
#include "systick.h"
#include "systeminit.h"
#include "init.h"
#include "button.h"
#include "adc.h"
#include "dma.h"
#include "timer.h"
#include "timer_irq.h"
#include "timer_pwm.h"
#include "timer_sync.h"

#include <stdint.h>

#endif /* __MAIN_H */