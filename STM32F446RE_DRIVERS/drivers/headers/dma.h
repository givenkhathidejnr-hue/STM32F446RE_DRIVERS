#ifndef DMA_H
#define DMA_H

/**
 * DMA CONTROLLER
 * Moves data between peripheral and memory and between memory and memory, without CPU action
 * STM32F446 has 2 dma controllers - DMA1 and DMA2
 * 8 streams per DMA with 8 channels(requests) per stream
 */

 /** TERMINOLOGY 
  * A beat = one transfer of a data item (byte, half-word, or word) between source and destination.
            The size of a beat depends on the configured data width (PSIZE and MSIZE)
  * Burst size = how many beats are grouped together in one burst.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "device.h"   /* Adjust if using another STM32 family */

/* Macros
 * @note __POS__ is parenthesized below even though every current call site
 *       passes a single _Pos macro or literal 0U - without it, a future call
 *       with a compound expression (e.g. "a + b") would silently mis-shift
 *       due to operator precedence (<< binds tighter than +). Cheap to guard
 *       against now. */
#define     __DMA_CLEAR(__STREAM_REG__, __MASK__)           (__STREAM_REG__ &= ~(__MASK__))
#define     __DMA_SET(__STREAM_REG__, __BITS__, __POS__)    (__STREAM_REG__ |= (uint32_t)(__BITS__) << (__POS__))

/*======================================================================
 * DMA TYPES
 *====================================================================*/
typedef enum
{
    DMA_OK,
    DMA_INVALID_PARAM,
    DMA_BUSY,
    DMA_ERROR
} dma_status_t;

typedef enum
{
    DMA_EVENT_HALF,
    DMA_EVENT_COMPLETE,
    DMA_EVENT_ERROR
} dma_event_t;

/**
 * @brief DMA transfer direction
 */
typedef enum
{
    DMA_DIR_PERIPH_TO_MEM = 0,
    DMA_DIR_MEM_TO_PERIPH = 1,
    DMA_DIR_MEM_TO_MEM    = 2
} dma_direction_t;

/**
 * @brief DMA stream channel/request priority level
 * If two requests have the same software priority level, the stream with the
lower number takes priority over the stream with the higher number
 */
typedef enum
{
    DMA_PRIORITY_LOW = 0U,
    DMA_PRIORITY_MEDIUM,
    DMA_PRIORITY_HIGH,
    DMA_PRIORITY_VERY_HIGH
} dma_priority_t;

/**
 * @brief Peripheral increment mode
 * Increments by the data size eg. if data_size = word, address increment by 4
 */
typedef enum
{
    DMA_PINC_DISABLE = 0U,
    DMA_PINC_ENABLE
} dma_pinc_t;

/**
 * @brief Memory increment mode
 * Increments by the data size eg. if data_size = half_word, address increment by 2
 */
typedef enum
{
    DMA_MINC_DISABLE = 0U,
    DMA_MINC_ENABLE
} dma_minc_t;

/**
 * @brief Data width
 * @note Independent for both source and destination
 * When unequal, the DMA controller packs/unpacks the transfers - ONLY IN FIFO MODE
 * When half_word or word, the peripheral and memory/peripheral addresses have to be 
   aligned on halfword or word address boundary
 */
typedef enum
{
    DMA_DATA_BYTE = 0U,
    DMA_DATA_HALFWORD,
    DMA_DATA_WORD
} dma_datasize_t;

/**
 * @brief DMA operating mode
 */
typedef enum
{
    DMA_MODE_NORMAL = 0U,
    DMA_MODE_CIRCULAR
} dma_mode_t;

/**
 * @brief FIFO mode/ Direct mode
 * 32 16-bytes FIFO buffers per stream
 */
typedef enum
{
    DMA_DIRECT_MODE = 0U,      /* Only 1 data is preloaded from memory too the internal FIFO */
    DMA_FIFO_MODE
} dma_fifo_mode_t;

/**
 * @brief FIFO threshold
 */
typedef enum
{
    DMA_FIFO_THRESH_1_4 = 0U,
    DMA_FIFO_THRESH_1_2,
    DMA_FIFO_THRESH_3_4,
    DMA_FIFO_THRESH_FULL
} dma_fifo_threshold_t;

/**
 * @brief DMA burst transfer configuration
 * Burst mode is only available when FIFO is enabled.
 */
typedef enum
{
    DMA_BURST_SINGLE = 0U,     // No burst, single transfer
    DMA_BURST_INCR4,           // Incremental burst of 4 transfers/beats
    DMA_BURST_INCR8,           // Incremental burst of 8 transfers
    DMA_BURST_INCR16           // Incremental burst of 16 transfers
} dma_burst_t;

/**
 * @brief DMA double buffer mode
 * When enabled, the circular mode is automatically enabled
 * At the end of each transaction the memory pointers are swapped
 * Double-buffer stream can work in both directions
 */
typedef enum
{
    DMA_DBM_DISABLE = 0U,
    DMA_DBM_ENABLE
} dma_double_buffer_t;

/**
 * @brief DMA flow controller
 * Manages the number of data items to be transferred
 * DMA flow controller:         1 - 65535 data items
 * Peripheral flow controller:  number of data items unknown
 */
typedef enum
{
    DMA_FLOW_CTRL_DMA = 0U,
    DMA_FLOW_CTRL_PERIPH        /* This feature is only supported for peripherals that are able to signal the end of the transfer, that is: SDIO.*/
} dma_flow_ctrl_t;

/**
 * @brief Peripheral increment offset size (PINCOS)
 */
typedef enum
{
    DMA_PINCOS_ALIGN_DATA = 0U,  // Increment by data size (PSIZE)
    DMA_PINCOS_ALIGN_WORD        // Always increment by 4 bytes
} dma_pincos_t;

/*======================================================================
 * DMA INTERRUPTS
 *====================================================================*/

typedef enum
{
    DMA_IT_DME  = (1U << 1),    /* Direct mode error */
    DMA_IT_TE   = (1U << 2),   /* Transfer error */
    DMA_IT_HT   = (1U << 3),   /* Half transfer */
    DMA_IT_TC   = (1U << 4),   /* Transfer complete */
    
    DMA_IT_FE   = (1U << 7)   /* FIFO error */
} dma_interrupt_t;

/*======================================================================
 * DMA CHANNELS
 *====================================================================*/
typedef enum
{
    DMA_CHANNEL_0 = 0,  DMA_CHANNEL_1 = 1,
    DMA_CHANNEL_2 = 2,  DMA_CHANNEL_3 = 3,
    DMA_CHANNEL_4 = 4,  DMA_CHANNEL_5 = 5,
    DMA_CHANNEL_6 = 6,  DMA_CHANNEL_7 = 7
} dma_channel_t;

/*======================================================================
 * DMA CONFIGURATION STRUCTURE
 *====================================================================*/

/**
 * @brief DMA configuration structure
 */
typedef struct
{
    DMA_TypeDef             *controller;
    DMA_Stream_TypeDef      *stream;
    dma_channel_t           channel;

    dma_direction_t         direction;
    dma_priority_t          priority;

    dma_pinc_t              periph_inc;
    dma_minc_t              mem_inc;

    dma_datasize_t          periph_data_size;
    dma_datasize_t          mem_data_size;

    dma_mode_t              mode;


    /* Optionals */
    dma_fifo_mode_t         fifo_mode;
    dma_fifo_threshold_t    fifo_threshold;

    dma_double_buffer_t     double_buffer;  // double buffer mode - Double-buffer mode requires setting two memory addresses (M0AR and M1AR) and enabling the DBM bit in DMA_SxCR.

    uint32_t                interrupts;
} dma_config_t;


/*======================================================================
 * DMA RUNTIME HANDLE
 *====================================================================*/

/**
 * @brief DMA handle structure
 */
/* Callback type */
typedef void (*DMA_Callback_t)(void *context);

/* Driver handler */
typedef struct
{
    dma_config_t        config;

    DMA_Callback_t xfer_cplt_cb;
    DMA_Callback_t half_cplt_cb;
    DMA_Callback_t error_cb;

    void *user_context;

} dma_handle_t;

/**
 * @brief Start DMA transfer
 */
__STATIC_FORCEINLINE void dma_enable_stream(dma_handle_t *hdma)
{
    hdma->config.stream->CR    |=  DMA_SxCR_EN;
}

/**
 * @brief Stop DMA transfer
 */
__STATIC_FORCEINLINE void dma_disable_stream(dma_handle_t *hdma)
{
    hdma->config.stream->CR    &=  ~DMA_SxCR_EN;
}

#define CR_INTERRUPTS   (DMA_SxCR_DMEIE|DMA_SxCR_TEIE|DMA_SxCR_HTIE|DMA_SxCR_TCIE)

/**
 * @brief Enable DMA interrupt sources
 */
__STATIC_FORCEINLINE void dma_enable_interrupts(dma_handle_t *hdma, uint32_t interrupts)
{
    __DMA_SET(hdma->config.stream->CR, (interrupts & CR_INTERRUPTS), 0U);

    if (interrupts & DMA_SxFCR_FEIE) __DMA_SET(hdma->config.stream->FCR, DMA_SxFCR_FEIE, 0U);

    hdma->config.interrupts |= interrupts;
}

/**
 * @brief Disable DMA interrupt sources
 */
__STATIC_FORCEINLINE void dma_disable_interrupts(dma_handle_t *hdma, uint32_t interrupts)
{
    __DMA_CLEAR(hdma->config.stream->CR, (interrupts & CR_INTERRUPTS));

    if (interrupts & DMA_SxFCR_FEIE) __DMA_CLEAR(hdma->config.stream->FCR, DMA_SxFCR_FEIE);

    hdma->config.interrupts &= ~interrupts;
}

/*======================================================================
 * API FUNCTIONS
 *====================================================================*/

/**
 * @brief Initialize DMA peripheral with configuration
 */
dma_status_t dma_init(dma_handle_t *hdma, const dma_config_t *config);

/**
 * @brief Start and stop stream transfers
 */
void dma_start(dma_handle_t *hdma);
void dma_stop(dma_handle_t *hdma);

/**
 * @brief Registers callbacks for the given dma handler
 */
void DMA_RegisterCallback(dma_handle_t *hdma, 
                        dma_event_t event,
                        DMA_Callback_t callback,
                        void *context);


/**
 * @brief Configure transfer addresses and size
 */
dma_status_t dma_config_transfer(dma_handle_t *hdma,
                         uint32_t src,
                         uint32_t dst,
                         uint16_t length);
    
/**
 * @brief Configure double-buffer addresses
 * Must be called if double-buffer mode is enabled
 */
dma_status_t dma_config_double_buffer(dma_handle_t *hdma,
                              uint32_t mem0_addr,
                              uint32_t mem1_addr);

/**
 * @brief Configure burst settings
 * Only valid when FIFO mode is enabled
 */
dma_status_t dma_config_burst(dma_handle_t *hdma,
                      dma_burst_t mem_burst,
                      dma_burst_t periph_burst);

/**
 * @brief Set DMA flow controller
 */
dma_status_t dma_set_flow_controller(dma_handle_t *hdma, dma_flow_ctrl_t ctrl);

/**
 * @brief Set peripheral increment offset size (PINCOS)
 */
dma_status_t dma_set_periph_offset(dma_handle_t *hdma, dma_pincos_t offset);


/**
 * @brief IRQ handler helper
 */
void dma_irq_handler(dma_handle_t *hdma);

#ifdef __cplusplus
}
#endif

#endif /* DMA_H */

