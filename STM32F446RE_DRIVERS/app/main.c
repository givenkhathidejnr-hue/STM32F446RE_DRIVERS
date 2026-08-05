/**
 * @file main.c
 * @brief Demo application exercising most of this project's driver stack
 *        together on one board, as a reference for how the pieces fit.
 *
 * Wiring assumed by this demo (adjust to your actual board):
 *   PB0        - heartbeat LED, toggled every 500ms from the main loop
 *                (SysTick-timed)
 *   PB1        - second heartbeat LED, toggled once per second from a
 *                TIM3 update-event interrupt (hardware-timer-timed, for
 *                contrast with PB0)
 *   PB2..PB7   - spare LEDs, configured as outputs but otherwise unused
 *   PA6        - TIM3_CH1 PWM output (AF2), a "breathing"/dimmable LED
 *   PC13       - user button, active-low, internal pull-up
 *   PA1        - ADC1_IN1 analog input
 *
 * Concepts demonstrated: RCC peripheral clock gating, GPIO (output/AF/
 * analog/input), EXTI + interrupt-driven debounced button handling,
 * SysTick monotonic timing, NVIC priority grouping/priorities, timer base
 * configuration, TIM2->TIM3 master/slave-style synchronization via TRGO,
 * PWM generation, timer update-event interrupts, ADC regular-channel
 * sampling triggered by a timer, ADC+DMA circular capture, ADC overrun
 * interrupt as a DMA safety net, and PWR sleep mode.
 *
 * Deliberately not shown here: advanced timer BDTR/motor control
 * (timer_advanced.h / timer_motor.h) and encoder/input-capture
 * (timer_encoder.h) - wiring both of those in on top of this file would
 * need more timers/pins than most boards have free at once. Both modules
 * are exercised in isolation by their own header/source pairs; see
 * REVIEW_NOTES.md.
 */
#include "main.h"

extern clock_info_t clock_info;

/* ---------- LED pin mask ---------- */
#define LED_PINS  (GPIO_PIN(0)|GPIO_PIN(1)|GPIO_PIN(2)|GPIO_PIN(3)|GPIO_PIN(4)|GPIO_PIN(5)|GPIO_PIN(6)|GPIO_PIN(7))
#define HEARTBEAT_SW_LED   GPIO_PIN(0)   /* SysTick-driven */
#define HEARTBEAT_HW_LED   GPIO_PIN(1)   /* TIM3-update-driven */

#define ADC_BUFFER_LEN     8U

/* ---------- Shared state ---------- */
static dma_handle_t adc_dma_handle;
static adc_handle_t hadc1;
static uint16_t     adc_buffer[ADC_BUFFER_LEN];

/* Latest values are 'volatile' - written from ISR context, read from the
 * main loop / a debugger, never both at once in a way that needs a lock
 * (each is a single aligned word, atomic on Cortex-M). */
static volatile uint16_t latest_adc_sample      = 0U;
static volatile uint32_t adc_dma_half_count     = 0U;
static volatile uint32_t adc_dma_complete_count = 0U;
static volatile uint32_t adc_overrun_count      = 0U;
static volatile uint32_t tim3_update_ticks      = 0U;

/* 25% / 50% / 75% of a 1000-count PWM period, cycled by button presses */
static const uint16_t pwm_duty_table[3] = {250U, 500U, 750U};
static uint8_t        pwm_duty_index    = 1U;   /* start at 50% */

static button_t user_button;

/* ===================== Interrupt-context callbacks ===================== */

/**
 * @brief Runs from button_handler() (itself called from SysTick_Handler) -
 *        never from EXTI context directly, see button.c's module comment.
 */
static void button_event_handler(button_event_t event, void *ctx)
{
    (void)ctx;

    switch (event) {
        case BUTTON_EVENT_PRESS:
            pwm_duty_index = (uint8_t)((pwm_duty_index + 1U) % 3U);
            timer_pwm_set_pulse(TIM3, TIMER_PWM_CHANNEL_1, pwm_duty_table[pwm_duty_index]);
            break;
        case BUTTON_EVENT_LONG_PRESS:
            pwm_duty_index = 1U;   /* long-press resets duty to 50% */
            timer_pwm_set_pulse(TIM3, TIMER_PWM_CHANNEL_1, pwm_duty_table[pwm_duty_index]);
            break;
        case BUTTON_EVENT_RELEASE:
        default:
            break;
    }
}

/** @brief TIM3 update-event callback - dispatched via timer_irq_handler(). */
static void tim3_update_handler(TIM_TypeDef *tim)
{
    (void)tim;
    if (++tim3_update_ticks >= 1000U) {   /* 1000 updates @ 1kHz = 1s */
        tim3_update_ticks = 0U;
        gpio_toggle(GPIOB, HEARTBEAT_HW_LED);
    }
}

/** @brief ADC overrun - fires if DMA ever falls behind the 1kHz sample rate. */
static void adc_overrun_handler(void *ctx)
{
    (void)ctx;
    adc_overrun_count++;
}

static void adc_dma_half_handler(void *ctx)
{
    (void)ctx;
    latest_adc_sample = adc_buffer[ADC_BUFFER_LEN / 2U - 1U];
    adc_dma_half_count++;
}

static void adc_dma_complete_handler(void *ctx)
{
    (void)ctx;
    latest_adc_sample = adc_buffer[ADC_BUFFER_LEN - 1U];
    adc_dma_complete_count++;
}

/* ===================== IRQ handlers =====================
 * Names must exactly match this project's custom startup file
 * (platform/stm32f446_startup.s) - it uses "..._Handler", not the ST
 * CMSIS-default "..._IRQHandler" naming.
 */

void DMA2_Stream0_Handler(void)
{
    dma_irq_handler(&adc_dma_handle);
}

void ADC_Handler(void)
{
    adc_irq_handler(&hadc1);
}

/* ===================== Setup helpers ===================== */

static inline void setup_leds(void)
{
    gpio_config_t led_config = {0};

    led_config.port  = GPIOB;
    led_config.pins  = LED_PINS;
    led_config.mode  = GPIO_MODE_OUTPUT;
    led_config.otype = GPIO_OTYPE_PP;
    led_config.speed = GPIO_SPEED_FAST;
    led_config.pull  = GPIO_NOPULL;

    gpio_init(&led_config);
}

static inline void setup_button(void)
{
    user_button.port         = GPIOC;
    user_button.pin           = GPIO_PIN(13);
    user_button.pull          = GPIO_PULL_UP;
    user_button.active_level  = 0U;      /* pressed = pulled low */
    user_button.debounce_ms   = 30U;
    user_button.long_press_ms = 800U;
    user_button.trigger       = EXTI_TRIGGER_BOTH;  /* need both edges: press and release */
    user_button.preempt_prio  = 5U;
    user_button.sub_prio      = 0U;      /* NVIC_GROUP_4 leaves 0 sub-priority bits */
    user_button.callback      = button_event_handler;
    user_button.context       = NULL;

    button_init(&user_button);
}

/** @brief TIM2: free-running 1kHz time base, TRGO-only - drives the ADC's sample rate. */
static inline void setup_master_timer(void)
{
    timer_base_config_t base_cfg = {0};
    base_cfg.prescaler = clock_info.apb1_timer_clk / 1000000U - 1U;  /* -> 1 MHz tick */
    base_cfg.period    = 1000U - 1U;                                  /* -> 1 kHz update event */
    timer_base_init(TIM2, &base_cfg);

    timer_master_sync_cfg_t master_cfg = {0};
    master_cfg.timer          = TIM2;
    master_cfg.trigger_output = TIMER_TRGO_UPDATE;   /* TRGO pulses on every update event */
    timer_sync_master_init(&master_cfg);

    timer_start(TIM2);
}

/** @brief TIM3: PWM "breathing" LED on PA6/CH1, plus a 1Hz update-IRQ heartbeat on PB1. */
static inline void setup_pwm_led(void)
{
    gpio_config_t pwm_pin = {0};
    pwm_pin.port  = GPIOA;
    pwm_pin.pins  = GPIO_PIN(6);      /* TIM3_CH1 = PA6, AF2 on STM32F446 */
    pwm_pin.mode  = GPIO_MODE_AF;
    pwm_pin.otype = GPIO_OTYPE_PP;
    pwm_pin.speed = GPIO_SPEED_FAST;
    pwm_pin.pull  = GPIO_NOPULL;
    pwm_pin.af    = GPIO_AF2;
    gpio_init(&pwm_pin);

    timer_base_config_t base_cfg = {0};
    base_cfg.prescaler      = clock_info.apb1_timer_clk / 1000000U - 1U;  /* -> 1 MHz tick */
    base_cfg.period         = 1000U - 1U;                                  /* -> 1 kHz PWM */
    base_cfg.period_preload = true;
    timer_base_init(TIM3, &base_cfg);

    timer_pwm_cfg_t pwm_cfg = {0};
    pwm_cfg.channel      = TIMER_PWM_CHANNEL_1;
    pwm_cfg.pwm_mode     = TIMER_PWM_MODE_1;
    pwm_cfg.alignment    = PWM_ALIGNMENT_EDGE;
    pwm_cfg.counter_mode = PWM_COUNTER_MODE_UP;
    pwm_cfg.pulse        = pwm_duty_table[pwm_duty_index];
    pwm_cfg.oc_preload   = true;   /* CCR writes from button_event_handler() only take
                                     * effect at the next update event - no glitches */
    pwm_cfg.polarity     = PWM_POLARITY_ACTIVE_HIGH;
    timer_pwm_init(TIM3, &pwm_cfg);
    timer_pwm_enable(TIM3, TIMER_PWM_CHANNEL_1);

    /* Secondary, hardware-timer-driven heartbeat - contrast with the
     * SysTick-driven one in the main loop. */
    timer_set_event_callback(TIM3, TIMER_EVENT_UPDATE, tim3_update_handler);
    timer_update_irq_enable(TIM3);
    nvic_set_priority(TIM3_IRQn, 8U, 0U);
    nvic_enable_irq(TIM3_IRQn);

    timer_start(TIM3);
}

/** @brief ADC1 + DMA2: one channel, TIM2-TRGO-triggered, DMA-captured into a circular buffer. */
static inline void setup_adc_capture(void)
{
    gpio_config_t adc_pin = {0};
    adc_pin.port = GPIOA;
    adc_pin.pins = GPIO_PIN(1);   /* ADC1_IN1 */
    adc_pin.mode = GPIO_MODE_ANALOG;
    adc_pin.pull = GPIO_NOPULL;
    gpio_init(&adc_pin);

    adc_common_init(ADC_PRESCALER_DIV4);

    /* DMA2 Stream0 Channel0 = ADC1 (RM0390 DMA2 request mapping table).
     * Arm the DMA stream BEFORE starting the ADC, so it's already waiting
     * the first time a conversion completes. */
    dma_config_t dma_cfg = {0};
    dma_cfg.controller       = DMA2;
    dma_cfg.stream           = DMA2_Stream0;
    dma_cfg.channel          = DMA_CHANNEL_0;
    dma_cfg.direction        = DMA_DIR_PERIPH_TO_MEM;
    dma_cfg.priority         = DMA_PRIORITY_HIGH;
    dma_cfg.periph_inc       = DMA_PINC_DISABLE;
    dma_cfg.mem_inc          = DMA_MINC_ENABLE;
    dma_cfg.periph_data_size = DMA_DATA_HALFWORD;  /* ADC1->DR: 16-bit for a 12-bit right-aligned result */
    dma_cfg.mem_data_size    = DMA_DATA_HALFWORD;
    dma_cfg.mode             = DMA_MODE_CIRCULAR;
    dma_cfg.fifo_mode        = DMA_DIRECT_MODE;
    dma_cfg.double_buffer    = DMA_DBM_DISABLE;
    dma_cfg.interrupts       = DMA_IT_TC | DMA_IT_HT;

    dma_init(&adc_dma_handle, &dma_cfg);
    dma_config_transfer(&adc_dma_handle,
                         (uint32_t)&ADC1->DR, (uint32_t)adc_buffer,
                         ADC_BUFFER_LEN);
    DMA_RegisterCallback(&adc_dma_handle, DMA_EVENT_HALF,     adc_dma_half_handler,     NULL);
    DMA_RegisterCallback(&adc_dma_handle, DMA_EVENT_COMPLETE, adc_dma_complete_handler, NULL);
    nvic_set_priority(DMA2_Stream0_IRQn, 3U, 0U);
    nvic_enable_irq(DMA2_Stream0_IRQn);
    dma_start(&adc_dma_handle);

    /* continuous = false is deliberate: with an external trigger armed,
     * CONT=1 would make the ADC free-run after the first triggered
     * conversion instead of waiting for each new TIM2 TRGO edge, defeating
     * the point of syncing the sample rate to TIM2. */
    adc_config_t adc_cfg = {0};
    adc_cfg.instance           = ADC1;
    adc_cfg.resolution         = ADC_RES_12BIT;
    adc_cfg.alignment          = ADC_ALIGN_RIGHT;
    adc_cfg.continuous         = false;
    adc_cfg.scan_mode          = false;
    adc_cfg.dma_mode           = true;
    adc_cfg.dma_continuous     = true;    /* DDS: keep servicing DMA requests indefinitely */
    adc_cfg.ext_trigger_edge   = ADC_EXTTRIG_RISING;
    adc_cfg.ext_trigger_source = ADC_TRIGGER_TIM2_TRGO;
    adc_cfg.interrupts         = ADC_IT_OVR;   /* safety net only - DMA does the real data movement */
    adc_cfg.num_channels       = 1U;
    adc_cfg.channels[0]        = ADC_CHANNEL_1;
    adc_cfg.sample_times[0]    = ADC_SAMPLETIME_112CYCLES;

    adc_init(&hadc1, &adc_cfg);
    adc_register_callback(&hadc1, ADC_EVENT_OVR, adc_overrun_handler, NULL);
    nvic_set_priority(ADC_IRQn, 3U, 0U);
    nvic_enable_irq(ADC_IRQn);
    adc_start(&hadc1);
}

/* ===================== Entry point ===================== */

void main(void)
{
    /* NVIC priority grouping + 1ms SysTick tick */
    system_init();

    /* Peripheral clocks - must be enabled before any *_init() below
     * touches that peripheral's registers. */
    (void)rcc_clock_control(RCC_GPIOA, RCC_CLOCK_ENABLE);
    (void)rcc_clock_control(RCC_GPIOB, RCC_CLOCK_ENABLE);
    (void)rcc_clock_control(RCC_GPIOC, RCC_CLOCK_ENABLE);
    (void)rcc_clock_control(RCC_TIM2,  RCC_CLOCK_ENABLE);
    (void)rcc_clock_control(RCC_TIM3,  RCC_CLOCK_ENABLE);
    (void)rcc_clock_control(RCC_DMA2,  RCC_CLOCK_ENABLE);
    (void)rcc_clock_control(RCC_ADC1,  RCC_CLOCK_ENABLE);

    setup_leds();
    setup_button();
    setup_master_timer();   /* TIM2: TRGO only, drives the ADC's sample rate */
    setup_pwm_led();        /* TIM3: PWM breathing LED + 1s hardware-timer heartbeat */
    setup_adc_capture();    /* ADC1 + DMA2: TIM2-synced, DMA-captured analog sampling */

    uint32_t last_blink_ms = 0U;

    while (1) {
        /* SysTick-driven heartbeat - contrast with tim3_update_handler()'s
         * hardware-timer-driven one on PB1. Unsigned-wraparound-safe
         * because systick_get_ms() is free-running over the full 32-bit
         * range (unlike a small hardware auto-reload counter). */
        uint32_t now = systick_get_ms();
        if ((now - last_blink_ms) >= 500U) {
            gpio_toggle(GPIOB, HEARTBEAT_SW_LED);
            last_blink_ms = now;
        }

        /* Everything else in this demo is interrupt-driven (button/EXTI,
         * ADC overrun, DMA half/complete, TIM3 update, SysTick) - sleep
         * between ticks instead of busy-waiting. Sleep mode (not Stop/
         * Standby) leaves all peripheral clocks running, so TIM2/TIM3/ADC/
         * DMA keep working normally; only the CPU core halts until the
         * next interrupt wakes it. */
        pwr_enter_sleep_mode();
    }
}
