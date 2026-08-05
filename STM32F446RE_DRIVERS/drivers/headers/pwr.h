#ifndef __PWR_H
#define __PWR_H

#include <stdint.h>
#include "device.h"

/**
 * @file pwr.h
 * @brief PWR (Power Control) driver: voltage scaling, overdrive, low-power
 *        modes.
 * @note  PWR's registers are only accessible once its APB1 clock is
 *        enabled (rcc_clock_control(RCC_PWR, RCC_CLOCK_ENABLE)) - see
 *        SystemInit() in platform/systeminit.c, which does this before any
 *        other PWR call.
 * @note  STM32H755 port: PWR is the single biggest gotcha going from F4 to
 *        H7. H7 uses an internal SMPS step-down converter (or an LDO, or a
 *        combination) that MUST be configured correctly at reset via
 *        PWR->CR3, or the core simply never leaves reset / hangs before
 *        main() - there is no equivalent concept on F4. Voltage scaling
 *        also changes from 3 levels (VOS1-3) to 4 (VOS0-3), with VOS0
 *        needing a separate "boost" step to reach the highest clock
 *        speeds. See PORTING_NOTES_H755.md before touching PWR on H7.
 */

/* ========================================================================== */
/* API */ /*Core PWR API CONSTANTS*/
/* ========================================================================== */
typedef enum
{
    PWR_VOLTAGE_SCALE_3,
    PWR_VOLTAGE_SCALE_2,
    PWR_VOLTAGE_SCALE_1
} PWR_VoltageScale_t;

typedef enum
{
    PWR_OK,
    PWR_TIMEOUT,
    PWR_INVALID_PARAM
} PWR_Status;


/* ========================================================================== */
/* API */ /*Core PWR API functions*/
/* ========================================================================== */

PWR_Status pwr_enable_overdrive(void);
PWR_Status pwr_disable_overdrive(void);

/* Voltage scaling */
PWR_Status pwr_select_voltage_scale(PWR_VoltageScale_t scale);

/* Low power modes */
void pwr_enter_sleep_mode(void);
void pwr_enter_stop_mode(void);
void pwr_enter_standby_mode(void);

#endif /* __PWR_H */
