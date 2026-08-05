#include "dma.h"
/*======================================================================
 * API FUNCTIONS
 *====================================================================*/

/**
 * @brief Initialize DMA peripheral with configuration
 * @note  Only copies/validates the configuration - the stream itself is
 *        left untouched (and disabled) until dma_start() is called.
 */
dma_status_t dma_init(dma_handle_t *hdma, const dma_config_t *config)
{
    if (!hdma || !config || !config->stream || !config->controller)    return DMA_INVALID_PARAM;

    hdma->config        = *config;
    hdma->xfer_cplt_cb  = NULL;
    hdma->half_cplt_cb  = NULL;
    hdma->error_cb      = NULL;
    hdma->user_context  = NULL;

    return DMA_OK;
}

/*======================================================================
 * INTERNAL: stream index -> status/clear register + bit-offset
 *====================================================================*/

/**
 * @brief Resolve a stream to its status (ISR) / clear (IFCR) register pair
 *        and the bit-offset of its 6-bit flag group within that register.
 *
 * @note  This is the single trickiest part of the F4/F7 DMA controller and
 *        the source of a whole class of "IRQ fires but the wrong callback
 *        runs" bugs: the 8 streams of a controller are NOT packed at a flat
 *        4-bit-per-stream stride inside LISR/HISR. Each stream owns a
 *        6-bit group - FEIF(0), reserved(1), DMEIF(2), TEIF(3), HTIF(4),
 *        TCIF(5) - but the groups themselves start at offsets
 *        {0, 6, 16, 22} within the 32-bit register (there's a 4-bit gap
 *        between stream 1's group and stream 2's). Streams 0-3 live in
 *        LISR/LIFCR, streams 4-7 in HISR/HIFCR, and the same {0,6,16,22}
 *        pattern repeats in each half (stream 4 uses the same relative
 *        offset as stream 0, stream 5 as stream 1, etc.). See RM0390
 *        "DMA low/high interrupt status register" for the bit map.
 */
static void dma_resolve_flags(DMA_TypeDef *controller, DMA_Stream_TypeDef *stream,
                               volatile uint32_t **isr, volatile uint32_t **ifcr,
                               uint32_t *shift)
{
    static const uint8_t flag_offset[4] = {0U, 6U, 16U, 22U};

    bool is_dma2    = ((uint32_t)controller == (uint32_t)DMA2);
    uintptr_t base  = is_dma2 ? (uintptr_t)DMA2_Stream0 : (uintptr_t)DMA1_Stream0;
    uint32_t idx    = (uint32_t)(((uintptr_t)stream - base) / sizeof(DMA_Stream_TypeDef));
    bool is_high    = (idx >= 4U);

    *isr   = is_high ? &controller->HISR  : &controller->LISR;
    *ifcr  = is_high ? &controller->HIFCR : &controller->LIFCR;
    *shift = flag_offset[idx & 3U];
}

/* START TRANSFER */
void dma_start(dma_handle_t *hdma)
{
    DMA_Stream_TypeDef *stream  = hdma->config.stream;

    /*1. disable the dma stream for configuration */
    dma_disable_stream(hdma);

    /*2. select channel in CR*/
    __DMA_CLEAR(stream->CR, DMA_SxCR_CHSEL);
    __DMA_SET(stream->CR, hdma->config.channel, DMA_SxCR_CHSEL_Pos);

    /*3. Select direction in the CR */
    __DMA_CLEAR(stream->CR, DMA_SxCR_DIR);
    __DMA_SET(stream->CR, hdma->config.direction, DMA_SxCR_DIR_Pos);

    /*4. configure channel priority */
    __DMA_CLEAR(stream->CR, DMA_SxCR_PL);
    __DMA_SET(stream->CR, hdma->config.priority, DMA_SxCR_PL_Pos);

    /*5. Configure the dma working mode */
    __DMA_CLEAR(stream->CR, DMA_SxCR_CIRC);   // default to normal
    if (hdma->config.mode == DMA_MODE_CIRCULAR) {
        __DMA_SET(stream->CR, DMA_SxCR_CIRC, 0U);
    }

    /*6. Set peripheral and memeory increment modes */
    switch (hdma->config.periph_inc)
    {
        case DMA_PINC_DISABLE:  __DMA_CLEAR(stream->CR, DMA_SxCR_PINC);         break;
        case DMA_PINC_ENABLE:   __DMA_SET(stream->CR, DMA_SxCR_PINC, 0U);       break;
    }
    switch (hdma->config.mem_inc)
    {
        case DMA_MINC_DISABLE:  __DMA_CLEAR(stream->CR, DMA_SxCR_MINC);         break;
        case DMA_MINC_ENABLE:   __DMA_SET(stream->CR, DMA_SxCR_MINC, 0U);       break;
    }

    /*7. Set the peripheral and memory data sizes*/
    __DMA_CLEAR(stream->CR, DMA_SxCR_PSIZE);
    __DMA_SET(stream->CR, hdma->config.periph_data_size, DMA_SxCR_PSIZE_Pos);

    __DMA_CLEAR(stream->CR, DMA_SxCR_MSIZE);
    __DMA_SET(stream->CR, hdma->config.mem_data_size, DMA_SxCR_MSIZE_Pos);

    /*8. Select FIFO mode */
    switch (hdma->config.fifo_mode)
    {
        case DMA_DIRECT_MODE:  __DMA_CLEAR(stream->FCR, DMA_SxFCR_DMDIS);      break;
        case DMA_FIFO_MODE:   __DMA_SET(stream->FCR, DMA_SxFCR_DMDIS, 0U);    break;
    }

    /*9. Set the fifo threshold */
    if (hdma->config.fifo_mode == DMA_FIFO_MODE)
    {
        __DMA_CLEAR(stream->FCR, DMA_SxFCR_FTH);
        __DMA_SET(stream->FCR, hdma->config.fifo_threshold, DMA_SxFCR_FTH_Pos);
    }

    /*10 Double buffer mode */
    if (hdma->config.double_buffer == DMA_DBM_ENABLE) {
        stream->CR |= DMA_SxCR_DBM;
        // CIRC is forced by hardware when DBMEN=1
    }

    /*10. Enable interrupts */
    dma_enable_interrupts(hdma, hdma->config.interrupts);

    /* 11. Enable stremam */
    dma_enable_stream(hdma);
}

/**
 * @brief Stop the DMA stream and clear all status flags
 * @note After calling this, the stream is disabled and ready for reconfiguration
 */
void dma_stop(dma_handle_t *hdma)
{
    DMA_Stream_TypeDef *stream = hdma->config.stream;
    
    if (!stream) return;

    // 1. Disable the stream
    dma_disable_stream(hdma);

    // 2. Wait until the hardware actually disables it (recommended, some errata)
    //    Timeout ~ few µs in worst case
    uint32_t timeout = 0xFFFF;
    while ((stream->CR & DMA_SxCR_EN) && timeout--)
    {
        __NOP();
    }

    // 3. Clear every status flag for this stream (very important! a stale
    //    latched TCIF/TEIF will make the very next dma_start() look like it
    //    immediately completed/errored). Mask 0x3D = FEIF|DMEIF|TEIF|HTIF|TCIF
    //    (bit 1 of each 6-bit group is reserved, left alone).
    volatile uint32_t *isr, *ifcr;
    uint32_t shift;
    dma_resolve_flags(hdma->config.controller, stream, &isr, &ifcr, &shift);
    (void)isr;
    *ifcr = 0x3DU << shift;

    // Optional: disable interrupts in CR & FCR (good hygiene)
    dma_disable_interrupts(hdma, 0xFFFFFFFFU);
}

/**
 * @brief Configure transfer addresses and size
 * @note in case of a burst mode configured for memory
         - Length must be a multiple of (Mburst beat * Msize/Psize)
         - NDTR must also be a multiple of the Peripheral burst size 
         multiplied by the peripheral data size
 */
dma_status_t dma_config_transfer(dma_handle_t *hdma,
                         uint32_t src,
                         uint32_t dst,
                         uint16_t length)
{
    if (!hdma || !hdma->config.stream || !src || !dst || length == 0)  return DMA_INVALID_PARAM;

    DMA_Stream_TypeDef *stream  = hdma->config.stream;
    
    /* check to see if dma stream is busy */
    if (stream->CR & DMA_SxCR_EN) return DMA_BUSY;

    /*1. set the source address */
    stream->PAR = (uint32_t)src;

    /*2. set the destination address */
    stream->M0AR = (uint32_t)dst;
    
    /*3. Set the length on the NDTR */
    stream->NDTR = length & 0xFFFF;

    return DMA_OK;
}

/**
 * @brief Configure memory addresses for double-buffer mode
 * @note Must be called AFTER dma_init and BEFORE dma_start
 *       Double-buffer mode automatically enables circular mode
 * @pre  hdma->config.double_buffer == DMA_DBM_ENABLE (you should add this field)
 */
dma_status_t dma_config_double_buffer(dma_handle_t *hdma,
                                      uint32_t mem0_addr,
                                      uint32_t mem1_addr)
{
    DMA_Stream_TypeDef *stream = hdma->config.stream;
    
    if (!hdma || !stream || !mem0_addr || !mem1_addr) {
        return DMA_INVALID_PARAM;
    }
    
    if (stream->CR & DMA_SxCR_EN) {
        return DMA_BUSY;
    }

    // Set first memory address (always M0AR)
    stream->M0AR = mem0_addr;
    
    // Set second memory address
    stream->M1AR = mem1_addr;
    
    // Enable double buffer mode (DBM bit) + circular mode is forced
    stream->CR |= DMA_SxCR_DBM | DMA_SxCR_CIRC;
    
    // Optional: start with buffer 0
    __DMA_CLEAR(stream->CR, DMA_SxCR_CT);   // CT=0 → using M0AR
    
    return DMA_OK;
}

/**
 * @brief Configure burst sizes (only valid when FIFO mode is enabled)
 * @note Burst must match data size alignment rules (see RM)
 */
dma_status_t dma_config_burst(dma_handle_t *hdma,
                              dma_burst_t mem_burst,
                              dma_burst_t periph_burst)
{
    DMA_Stream_TypeDef *stream = hdma->config.stream;
    
    if (!hdma || !stream) return DMA_INVALID_PARAM;
    if (stream->CR & DMA_SxCR_EN) return DMA_BUSY;  //// Must disable stream before changing PAR/M0AR/NDTR
    if (hdma->config.fifo_mode == DMA_DIRECT_MODE) return DMA_INVALID_PARAM;

    // Clear old burst settings
    __DMA_CLEAR(stream->CR, (DMA_SxCR_MBURST | DMA_SxCR_PBURST));

    // Set memory burst
    if (mem_burst != DMA_BURST_SINGLE) {
        __DMA_SET(stream->CR, (uint32_t)mem_burst, DMA_SxCR_MBURST_Pos);
    }

    // Set peripheral burst
    if (periph_burst != DMA_BURST_SINGLE) {
        __DMA_SET(stream->CR, (uint32_t)periph_burst, DMA_SxCR_PBURST_Pos);
    }

    return DMA_OK;
}

/**
 * @brief Registers callbacks for the given dma handler
 */
void DMA_RegisterCallback(dma_handle_t *hdma, 
                        dma_event_t event,
                        DMA_Callback_t callback,
                        void *context)
{
    hdma->user_context = context;

    switch (event)
    {
        case DMA_EVENT_COMPLETE:    hdma->xfer_cplt_cb = callback;  break;
        case DMA_EVENT_HALF:        hdma->half_cplt_cb = callback;  break;
        case DMA_EVENT_ERROR:       hdma->error_cb     = callback;  break;
        
        default: break;
    }
}                  


/**
 * @brief IRQ handler helper - call from the stream's DMAx_StreamY_IRQHandler
 * @note  Flags are cleared up front (before dispatching callbacks) so that a
 *        higher-priority IRQ preempting this handler never re-observes the
 *        same already-handled event.
 */
void dma_irq_handler(dma_handle_t *hdma)
{
    DMA_TypeDef *controller        = hdma->config.controller;
    DMA_Stream_TypeDef *stream     = hdma->config.stream;

    volatile uint32_t *isr, *ifcr;
    uint32_t shift;
    dma_resolve_flags(controller, stream, &isr, &ifcr, &shift);

    /* Isolate this stream's 6-bit group: FEIF(0) DMEIF(2) TEIF(3) HTIF(4) TCIF(5) */
    uint32_t flags = (*isr >> shift) & 0x3FU;

    /* Clear every flag for this stream before dispatching (see note above) */
    *ifcr = 0x3DU << shift;

    /* FIFO error (FEIF, relative bit 0). FEIF can latch even when the user
     * never enabled FEIE (e.g. direct-mode underrun/overrun), so gate the
     * callback on whether they actually asked to hear about it. */
    if ((flags & (1U << 0)) && (hdma->config.interrupts & DMA_SxFCR_FEIE)) {
        if (hdma->error_cb) hdma->error_cb(hdma->user_context);
    }

    /* Transfer complete (TCIF, relative bit 5) */
    if (flags & (1U << 5))
    {
        if (hdma->xfer_cplt_cb)     hdma->xfer_cplt_cb(hdma->user_context);
    }

    /* Half transfer (HTIF, relative bit 4) */
    if (flags & (1U << 4))
    {
        if (hdma->half_cplt_cb)     hdma->half_cplt_cb(hdma->user_context);
    }

    /* Direct-mode error (DMEIF, relative bit 2) / transfer error (TEIF, relative bit 3) */
    if (flags & ((1U << 2) | (1U << 3)))
    {
        if (hdma->error_cb)         hdma->error_cb(hdma->user_context);
        dma_stop(hdma);  /* stops the stream on a serious (DMEIF/TEIF) error */
    }
}
