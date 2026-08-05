/**
 * @file hardfault_handler.c
 * @brief Minimal HardFault handler: recovers the pre-fault register file
 *        from whichever stack was active, snapshots the fault-cause
 *        registers into debugger-inspectable locals, then blinks an LED
 *        forever so a fault is visible even with no debugger attached.
 * @note  STM32H755 port: on the M7 core specifically, HFSR/CFSR can also
 *        indicate a bus fault caused by stale/uncached DMA data (D-Cache
 *        coherency issue) rather than a "real" addressing bug - worth
 *        checking cache-maintenance calls around DMA buffers first if
 *        faults start appearing only after porting DMA-using code to H7.
 *        See PORTING_NOTES_H755.md.
 */
#include "device.h"

#define __NAKED   __attribute__((naked))
#define __USED    __attribute__((used))
#define __NOINLINE __attribute__((noinline))

/**
 * @brief Naked trampoline: figures out whether the CPU was using the main
 *        stack (MSP) or process stack (PSP) at the moment of the fault -
 *        bit 2 of the exception LR (EXC_RETURN) encodes which - and passes
 *        that stack's base as the argument to hardfault_c_handler(), which
 *        is exactly where the CPU auto-stacked r0-r3/r12/lr/pc/xPSR when
 *        it entered the fault.
 * @note  Must stay naked + call (not branch into a normal C function
 *        directly): a non-naked prologue would push its own registers
 *        first and throw off the stack-pointer arithmetic the caller
 *        relies on.
 */
__NAKED void HardFault_Handler(void)
{
    __asm volatile (
        "tst lr, #4            \n"
        "ite eq                \n"
        "mrseq r0, msp         \n"
        "mrsne r0, psp         \n"
        "b hardfault_c_handler \n"
    );
}

/**
 * @brief C-level fault report. Every local here is 'volatile' and the
 *        function itself is __USED + __NOINLINE specifically so a
 *        debugger can still find and read r0/r1/.../cfsr/hfsr/bfar/mmfar
 *        after the fault, even under -Os - without 'volatile' the
 *        optimizer would happily conclude these locals are never actually
 *        used for anything (they're never branched on) and delete them
 *        entirely.
 * @note  cfsr (Configurable Fault Status Register) is the first thing to
 *        decode when debugging a real fault - its sub-fields (MMFSR/BFSR/
 *        UFSR) narrow down memory-management vs. bus vs. usage fault, and
 *        bfar/mmfar hold the faulting address when CFSR says it's valid.
 */
__USED __NOINLINE void hardfault_c_handler(uint32_t *stack)
{
    volatile uint32_t r0    = stack[0];
    volatile uint32_t r1    = stack[1];
    volatile uint32_t r2    = stack[2];
    volatile uint32_t r3    = stack[3];
    volatile uint32_t r12   = stack[4];
    volatile uint32_t lr    = stack[5];
    volatile uint32_t pc    = stack[6];
    volatile uint32_t psr   = stack[7];

    volatile uint32_t cfsr  = SCB->CFSR;
    volatile uint32_t hfsr  = SCB->HFSR;
    volatile uint32_t bfar  = SCB->BFAR;
    volatile uint32_t mmfar = SCB->MMFAR;

    /* -Wunused-variable fires on these regardless of 'volatile' (that
     * qualifier only stops the load from being optimized away - it doesn't
     * count as a "use" for this warning). The (void) casts are a no-op at
     * runtime; the values themselves are still live in memory for a
     * debugger to read. */
    (void)r0; (void)r1; (void)r2; (void)r3; (void)r12; (void)lr; (void)pc; (void)psr;
    (void)cfsr; (void)hfsr; (void)bfar; (void)mmfar;

    __disable_irq();

    // Enable GPIOB clock
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    // Configure PB5 as push-pull output, no pull
    GPIOB->MODER &= ~(0x3U << (5 * 2));      // clear mode bits
    GPIOB->MODER |=  (0x1U << (5 * 2));      // set to output
    GPIOB->OTYPER &= ~(1U << 5);             // push-pull
    GPIOB->OSPEEDR |= (0x3U << (5 * 2));     // high speed (optional)
    GPIOB->PUPDR   &= ~(0x3U << (5 * 2));    // no pull

    // Blink forever
    while (1) {
        GPIOB->BSRR = (1U << 5);             // set PB5
        for (volatile int i = 0; i < 1000000; i++);
        GPIOB->BSRR = (1U << (5 + 16));      // reset PB5
        for (volatile int i = 0; i < 1000000; i++);
    }
}
