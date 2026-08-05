#ifndef ADC_H
#define ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "device.h"          /* Your CMSIS / register definitions */
#include "timer_sync.h"

/* ================================================================
 * ADC STATUS
 * ================================================================ */
typedef enum
{
    ADC_OK               = 0,
    ADC_INVALID_PARAM    = 1,
    ADC_BUSY             = 2,
    ADC_ERROR            = 3,
    ADC_TIMEOUT          = 4
} adc_status_t;

/* ================================================================
 * ADC EVENTS
 * ================================================================ */
typedef enum
{
    ADC_EVENT_EOC,
    ADC_EVENT_JEOC,
    ADC_EVENT_OVR,
    ADC_EVENT_AWD
} adc_event_t;
/* ================================================================
 * ADC CALLBACKS
 * ================================================================ */
/**
 * @brief Callback type used by the ADC driver
 * @param context   User-defined pointer (usually the handle or your buffer)
 */
typedef void (*ADC_Callback_t)(void *context);

/* ================================================================
 * ADC BASIC TYPES
 * ================================================================ */

/**
 * @brief ADC resolution (CR1 RES[1:0])
 * Higher resolution → slower conversion time
 */
typedef enum
{
    ADC_RES_12BIT = 0U,   /* 15 ADCCLK cycles (default) */
    ADC_RES_10BIT = 1U,
    ADC_RES_8BIT  = 2U,
    ADC_RES_6BIT  = 3U
} adc_resolution_t;

/**
 * @brief Data alignment in DR register (CR2 ALIGN)
 */
typedef enum
{
    ADC_ALIGN_RIGHT = 0U,
    ADC_ALIGN_LEFT  = 1U
} adc_align_t;

/**
 * @brief Sampling time for each channel (SMPR1/SMPR2)
 * Longer sampling time = better accuracy with high-impedance sources
 */
typedef enum
{
    ADC_SAMPLETIME_3CYCLES   = 0U,
    ADC_SAMPLETIME_15CYCLES  = 1U,
    ADC_SAMPLETIME_28CYCLES  = 2U,
    ADC_SAMPLETIME_56CYCLES  = 3U,
    ADC_SAMPLETIME_84CYCLES  = 4U,
    ADC_SAMPLETIME_112CYCLES = 5U,
    ADC_SAMPLETIME_144CYCLES = 6U,
    ADC_SAMPLETIME_480CYCLES = 7U
} adc_sampletime_t;

/**
 * @brief ADC clock frequency prescaler
 */
typedef enum
{
    ADC_PRESCALER_DIV2,
    ADC_PRESCALER_DIV4,
    ADC_PRESCALER_DIV6,
    ADC_PRESCALER_DIV8
} adc_prescaler_t;

/**
 * @brief External trigger edge for regular group (CR2 EXTEN[1:0])
 */
typedef enum
{
    ADC_EXTTRIG_NONE         = 0U,   /* Software trigger only */
    ADC_EXTTRIG_RISING       = 1U,
    ADC_EXTTRIG_FALLING      = 2U,
    ADC_EXTTRIG_BOTH_EDGES   = 3U
} adc_exttrig_edge_t;

/**
 * @brief External trigger sources for regular group (CR2 EXTSEL[3:0])
 * Only the most common ones are listed. You can still write raw values in your .c file.
 */
typedef enum
{
    ADC_TRIGGER_TIM1_CC1     = 0U,
    ADC_TRIGGER_TIM1_CC2     = 1U,
    ADC_TRIGGER_TIM1_CC3     = 2U,
    ADC_TRIGGER_TIM2_CC2     = 3U,
    ADC_TRIGGER_TIM2_CC3     = 4U,
    ADC_TRIGGER_TIM2_CC4     = 5U,
    ADC_TRIGGER_TIM2_TRGO    = 6U,
    ADC_TRIGGER_TIM3_CC1     = 7U,
    ADC_TRIGGER_TIM3_CC3     = 8U,
    ADC_TRIGGER_TIM4_CC4     = 9U,
    ADC_TRIGGER_TIM5_CC1     = 10U,
    ADC_TRIGGER_TIM5_CC2     = 11U,
    ADC_TRIGGER_TIM5_CC3     = 12U,
    ADC_TRIGGER_TIM8_CC1     = 13U,
    ADC_TRIGGER_TIM8_TRGO    = 14U,
    ADC_TRIGGER_EXTI_LINE11  = 15U
} adc_trigger_source_t;

/**
 * @brief ADC channels (0..18)
 * 0-15  = external pins
 * 16    = temperature sensor
 * 17    = VREFINT
 * 18    = VBAT
 * Some channels are shared between ADCs. Check rm page 353
 */
typedef enum
{
    ADC_CHANNEL_0  = 0U,   ADC_CHANNEL_1  = 1U,  ADC_CHANNEL_2  = 2U,
    ADC_CHANNEL_3  = 3U,   ADC_CHANNEL_4  = 4U,  ADC_CHANNEL_5  = 5U,
    ADC_CHANNEL_6  = 6U,   ADC_CHANNEL_7  = 7U,  ADC_CHANNEL_8  = 8U,
    ADC_CHANNEL_9  = 9U,   ADC_CHANNEL_10 = 10U, ADC_CHANNEL_11 = 11U,
    ADC_CHANNEL_12 = 12U,  ADC_CHANNEL_13 = 13U, ADC_CHANNEL_14 = 14U,
    ADC_CHANNEL_15 = 15U,
    ADC_CHANNEL_TEMPSENSOR = 16U,
    ADC_CHANNEL_VREFINT    = 17U,
    ADC_CHANNEL_VBAT       = 18U
} adc_channel_t;

/* ================================================================
 * ADC INTERRUPT SOURCES (for CR1)
 * ================================================================ */
typedef enum
{
    ADC_IT_EOC   = (1U << 5),   /* End of conversion (or EOS when EOCS=1) */
    ADC_IT_AWD   = (1U << 6),   /* Analog watchdog */
    ADC_IT_JEOC  = (1U << 7),   /* Injected end of conversion */
    ADC_IT_OVR   = (1U << 26)   /* Overrun (new in F4) */
} adc_interrupt_t;

/* ================================================================
 * CONFIGURATION STRUCTURE
 * ================================================================ */
/**
 * @brief ADC configuration (filled once at init)
 * You will mostly program the sequence registers (SQRx) and SMPRx yourself
 * in adc_init() — this struct just holds the high-level choices.
 */
typedef struct
{
    ADC_TypeDef             *instance;          /* ADC1, ADC2 or ADC3 */

    adc_resolution_t        resolution;
    adc_align_t             alignment;

    adc_prescaler_t         prescaler;

    bool                    continuous;         /* CONT bit in CR2 */
    bool                    scan_mode;          /* SCAN bit in CR1 — required for multi-channel + DMA */
    bool                    dma_mode;           /* Enables DMA requests (CR2 DMA bit) */
    bool                    dma_continuous;     /* DDS bit: keep requesting after last channel */

    adc_exttrig_edge_t      ext_trigger_edge;
    adc_trigger_source_t    ext_trigger_source; /* Only used if edge != NONE */

    uint32_t                interrupts;         /* Bitmask of adc_interrupt_t */

    /* Regular group channel sequence (scan mode) */
    uint8_t                 num_channels;       /* 1..16 */
    adc_channel_t           channels[16];       /* Order of conversions */
    adc_sampletime_t        sample_times[16];   /* One per channel */

    /* Optional: injected group (you can ignore for now) */
    bool                    injected_mode;

} adc_config_t;

/* ================================================================
 * RUNTIME HANDLE
 * ================================================================ */
typedef struct
{
    ADC_TypeDef        *instance;
    adc_config_t       config;

    /* Callbacks — with DMA you usually only need these for errors */
    ADC_Callback_t     eos_callback;       /* End of regular sequence (recommended) */
    ADC_Callback_t     overrun_callback;   /* Data lost — very useful to know */
    ADC_Callback_t     awd_callback;       /* Analog watchdog (optional) */

    void              *user_context;
} adc_handle_t;

/* ================================================================
 * PUBLIC API
 * ================================================================ */
adc_status_t adc_init(adc_handle_t *hadc, const adc_config_t *config);

/**
 * @brief Start the ADC (set ADON + enable trigger if needed)
 */
__STATIC_FORCEINLINE void adc_start(adc_handle_t *hadc)
{
    hadc->instance->CR2   |=  ADC_CR2_ADON;
}

/**
 * @brief Stop the ADC (clear ADON)
 */
__STATIC_FORCEINLINE void adc_stop(adc_handle_t *hadc)
{
    hadc->instance->CR2  &= ~ADC_CR2_ADON;
}

/**
 * @brief Software trigger for regular group (when trigger = NONE)
 */
void adc_start_conversion(adc_handle_t *hadc);

/**
 * @brief Register callbacks
 */
void adc_register_callback(adc_handle_t *hadc,
                           adc_event_t event,           /* e.g. ADC_IT_EOC, ADC_IT_OVR ... */
                           ADC_Callback_t callback,
                           void *context);

/**
 * @brief IRQ handler — call from ADC1/2/3_IRQHandler()
 */
void adc_irq_handler(adc_handle_t *hadc);

/**
 * @brief Configure settings shared by ALL ADC instances (the common
 *        ADC->CCR register - ADC123_COMMON, not any one ADCx block).
 * @note  Call once at startup, before adc_init() on any instance - CCR is
 *        a single shared register, not per-ADC. (This used to be declared
 *        void-argument with no implementation at all - a link error
 *        waiting to happen the moment anything called it; see
 *        REVIEW_NOTES.md.)
 */
void adc_common_init(adc_prescaler_t prescaler);

#ifdef __cplusplus
}
#endif

#endif /* ADC_H */