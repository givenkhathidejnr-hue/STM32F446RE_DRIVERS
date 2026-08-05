#include "adc.h"

/**
 * @brief Configure an ADC instance (resolution, alignment, trigger, DMA,
 *        regular-channel sequence and sample times, interrupt sources).
 * @note  Does NOT set ADON - call adc_start() once configuration is
 *        complete, matching the rest of this library's "init configures,
 *        a separate call starts" convention.
 */
adc_status_t adc_init(adc_handle_t *hadc, const adc_config_t *config)
{
    if (!hadc || !config || !config->instance ||
        config->num_channels == 0U || config->num_channels > 16U)
        return ADC_INVALID_PARAM;

    ADC_TypeDef *instance = config->instance;

    /* Resolution (RES, CR1 bits 25:24) */
    instance->CR1   &= ~ADC_CR1_RES;
    instance->CR1   |= ((uint32_t)config->resolution << ADC_CR1_RES_Pos);

    /* Data alignment (ALIGN, CR2 bit 11) */
    switch (config->alignment)
    {
        case ADC_ALIGN_RIGHT:   instance->CR2   &= ~ADC_CR2_ALIGN;  break;
        case ADC_ALIGN_LEFT:    instance->CR2   |= ADC_CR2_ALIGN;   break;
    }

    /* Continuous conversion (CONT, CR2 bit 1) */
    if (config->continuous) { instance->CR2  |= ADC_CR2_CONT; }
    else                    { instance->CR2  &= ~ADC_CR2_CONT; }

    /* Scan mode lives in CR1 bit 8 (SCAN), NOT CR2 - CR2 bit 8 is DMA
     * enable, a completely different feature. Required for any
     * multi-channel regular sequence. */
    if (config->scan_mode)  { instance->CR1  |= ADC_CR1_SCAN; }
    else                    { instance->CR1  &= ~ADC_CR1_SCAN; }

    /* DMA requests (DMA, CR2 bit 8) + DDS (CR2 bit 9: keep issuing DMA
     * requests after the last channel in the sequence, needed for
     * continuous/circular DMA capture) */
    if (config->dma_mode)    {
        instance->CR2  |= ADC_CR2_DMA;
        if (config->dma_continuous) instance->CR2   |= ADC_CR2_DDS;
        else                        instance->CR2   &= ~ADC_CR2_DDS;
    }
    else                    { instance->CR2 &= ~ADC_CR2_DMA; }

    /* External trigger edge (EXTEN, CR2 bits 29:28) and trigger source
     * (EXTSEL, CR2 bits 27:24) - both are multi-bit fields and MUST be
     * shifted into place, unlike the single-bit flags above. */
    instance->CR2   &= ~ADC_CR2_EXTEN;
    instance->CR2   |= ((uint32_t)config->ext_trigger_edge << ADC_CR2_EXTEN_Pos);
    instance->CR2   &= ~ADC_CR2_EXTSEL;
    instance->CR2   |= ((uint32_t)config->ext_trigger_source << ADC_CR2_EXTSEL_Pos);

    /* Regular sequence length (L, SQR1 bits 23:20), encoded as (count - 1) */
    instance->SQR1  &= ~ADC_SQR1_L;
    instance->SQR1  |= ((uint32_t)(config->num_channels - 1U) << ADC_SQR1_L_Pos);

    /* Regular sequence channel order. Each SQx field is 5 bits (channel
     * numbers go up to 18) and the 16 possible sequence positions are
     * split across three registers:
     *   SQR3 = positions 0..5   (SQ1..SQ6),  offset  pos      * 5
     *   SQR2 = positions 6..11  (SQ7..SQ12), offset (pos - 6) * 5
     *   SQR1 = positions 12..15 (SQ13..SQ16),offset (pos - 12)* 5
     * Clear the channel bits before writing (leaving the L field just set
     * in SQR1 alone) so re-running adc_init() can't OR stale channel
     * numbers from a previous configuration into the new sequence. */
    instance->SQR3 = 0U;
    instance->SQR2 = 0U;
    instance->SQR1 &= ADC_SQR1_L;

    for (uint32_t idx = 0U; idx < config->num_channels; idx++)
    {
        uint32_t ch = (uint32_t)config->channels[idx];
        if (idx < 6U)        instance->SQR3 |= ch << (idx * 5U);
        else if (idx < 12U)  instance->SQR2 |= ch << ((idx - 6U) * 5U);
        else                 instance->SQR1 |= ch << ((idx - 12U) * 5U);
    }

    /* Sample time (SMPRx, 3 bits per *physical ADC channel number*, not
     * per sequence position!). SMPR2 covers channels 0-9, SMPR1 covers
     * 10-18. Index by config->channels[idx] (the real channel number),
     * not idx itself - otherwise a sequence that samples channels out of
     * numeric order silently assigns the sample time to the wrong
     * channel. */
    instance->SMPR1 = 0U;
    instance->SMPR2 = 0U;
    for (uint32_t idx = 0U; idx < config->num_channels; idx++)
    {
        uint32_t ch = (uint32_t)config->channels[idx];
        uint32_t st = (uint32_t)config->sample_times[idx];
        if (ch < 10U)   instance->SMPR2 |= st << (ch * 3U);
        else            instance->SMPR1 |= st << ((ch - 10U) * 3U);
    }

    /* Interrupts (EOC/AWD/JEOC/OVR) - adc_interrupt_t values in adc.h are
     * already pre-shifted bit masks, so no extra _Pos shift needed here. */
    instance->CR1   |= config->interrupts;

    /* Runtime handle */
    hadc->instance          = instance;
    hadc->config            = *config;
    hadc->eos_callback      = NULL;
    hadc->overrun_callback  = NULL;
    hadc->awd_callback      = NULL;
    hadc->user_context      = NULL;

    return ADC_OK;
}

/**
 * @brief Register callbacks
 */
void adc_register_callback(adc_handle_t *hadc,
                           adc_event_t event,           /* e.g. ADC_IT_EOC, ADC_IT_OVR ... */
                           ADC_Callback_t callback,
                           void *context)
{
    hadc->user_context = context;
    switch (event)
    {
        case ADC_EVENT_EOC: hadc->eos_callback      = callback; break;
        case ADC_EVENT_OVR: hadc->overrun_callback  = callback; break;
        case ADC_EVENT_AWD: hadc->awd_callback      = callback; break;
        /* ADC_EVENT_JEOC (injected end-of-conversion): the injected group
         * isn't configured anywhere in adc_init() (no JSQR/JOFFSETx
         * programming, no jeoc_callback slot in adc_handle_t) - known gap,
         * left as a documented no-op rather than a half-wired feature. */
        default: break;
    }
}

/**
 * @brief Configure settings shared by all ADC instances (ADC->CCR).
 * @note  ADC->CCR (the ADC123_COMMON block, aliased to ADC in CMSIS) is a
 *        single register shared by ADC1/2/3 - call this once at startup,
 *        not once per instance. Only the clock prescaler is wired up here;
 *        CCR also has dual/triple-ADC mode and common-DMA fields this
 *        project doesn't use (each ADC instance runs independently with
 *        its own DMA stream).
 */
void adc_common_init(adc_prescaler_t prescaler)
{
    ADC->CCR &= ~ADC_CCR_ADCPRE;
    ADC->CCR |= ((uint32_t)prescaler << ADC_CCR_ADCPRE_Pos);
}

/**
 * @brief IRQ handler — call from ADC1/2/3_IRQHandler()
 * @note  On the F446, ADC1/ADC2/ADC3 share a single vector (ADC_Handler in
 *        this project's startup file) - if more than one ADC is active, the
 *        shared handler must call this once per hadc whose SR is pending.
 */
void adc_irq_handler(adc_handle_t *hadc)
{
    uint32_t status_reg =  hadc->instance->SR;

    if (status_reg & ADC_SR_EOC)
    {
        /* clear the flag */
        hadc->instance->SR  &= ~ADC_SR_EOC;
        /* End-of-conversion callback */
        if (hadc->eos_callback)  hadc->eos_callback(hadc->user_context);
    }

    if (status_reg & ADC_SR_AWD)
    {
        /* clear the flag */
        hadc->instance->SR  &= ~ADC_SR_AWD;
        /* Analog watchdog callback */
        if (hadc->awd_callback)  hadc->awd_callback(hadc->user_context);
    }

    if (status_reg & ADC_SR_OVR)
    {
        /* clear the flag */
        hadc->instance->SR  &= ~ADC_SR_OVR;
        /* Overrun callback */
        if (hadc->overrun_callback)  hadc->overrun_callback(hadc->user_context);
    }
}