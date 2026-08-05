  .syntax unified				// unified assembler syntax
  .cpu cortex-m4
  .thumb						// assemble Thumb instructions
  .align 2						/*Align on 2 bytes margin*/

/*Import main and systeminit functions */
.extern SystemInit
.extern main
.extern __libc_init_array

/*Import linker scripts symbols */

/* Import linker script symbols */
.extern 	_stack_top
.extern  	_sidata
.extern  	_sdata
.extern  	_edata
.extern  	_sbss
.extern 	_ebss
.extern     _vectors_start
.extern		_vectors_end
.extern  	_ram_vector_start
.extern  	_ram_vector_end

/* -------------------------------------------------------------
 *  VECTOR TABLE
 * ------------------------------------------------------------- */
.section 	.isr_vector, "a", %progbits
.type	_vectors, %object
.global _vectors
.size _vectors, .-_vectors

_vectors:
	.word _stack_top
	.word Reset_Handler
	
	/* Cortex-M core exceptions */
	.word NMI_Handler
	.word HardFault_Handler
	.word MemManage_Handler
	.word BusFault_Handler
	.word UsageFault_Handler
	.word 0
	.word 0
	.word 0
	.word 0
	.word SVC_Handler
	.word DebugMon_Handler
	.word 0
	.word PendSV_Handler
	.word SysTick_Handler
	
	/* Device-specific IRQs*/
	.word WWDG_Handler
	.word PVD_Handler
	.word TAMP_STAMP_Handler
	.word RTC_WKUP_Handler
	.word FLASH_Handler
	.word RCC_Handler
	.word EXTI0_IRQHandler
	.word EXTI1_IRQHandler
	.word EXTI2_IRQHandler
	.word EXTI3_IRQHandler
	.word EXTI4_IRQHandler
	.word DMA1_Stream0_Handler
	.word DMA1_Stream1_Handler
	.word DMA1_Stream2_Handler
	.word DMA1_Stream3_Handler
	.word DMA1_Stream4_Handler
	.word DMA1_Stream5_Handler
	.word DMA1_Stream6_Handler
	.word ADC_Handler
	.word CAN1_TX_Handler
	.word CAN1_RX0_Handler
	.word CAN1_RX1_Handler
	.word CAN1_SCE_Handler
	.word EXTI9_5_IRQHandler
	.word TIM1_BRK_TIM9_Handler
	.word TIM1_UP_TIM10_Handler
	.word TIM1_TRG_COM_TIM11_Handler
	.word TIM1_CC_Handler
	.word TIM2_Handler
	.word TIM3_Handler
	.word TIM4_Handler
	.word I2C1_EV_Handler
	.word I2C1_ER_Handler
	.word I2C2_EV_Handler
	.word I2C2_ER_Handler
	.word SPI1_Handler
	.word SPI2_Handler
	.word USART1_Handler
	.word USART2_Handler
	.word USART3_Handler
	.word EXTI15_10_IRQHandler
	.word RTC_Alarm_Handler
	.word OTG_FS_WKUP_Handler
	.word TIM8_BRK_TIM12_Handler
	.word TIM8_UP_TIM13_Handler
	.word TIM8_TRG_COM_TIM14_Handler
	.word TIM8_CC_Handler
	.word DMA1_Stream7_Handler
	.word FMC_Handler
	.word SDIO_Handler
	.word TIM5_Handler
	.word SPI3_Handler
	.word UART4_Handler
	.word UART5_Handler
	.word TIM6_DAC_Handler
	.word TIM7_Handler
	.word DMA2_Stream0_Handler
	.word DMA2_Stream1_Handler
	.word DMA2_Stream2_Handler
	.word DMA2_Stream3_Handler
	.word DMA2_Stream4_Handler
	.word 0
	.word 0
	.word CAN2_TX_Handler
	.word CAN2_RX0_Handler
	.word CAN2_RX1_Handler
	.word CAN2_SCE_Handler
	.word OTG_FS_Handler
	.word DMA2_Stream5_Handler
	.word DMA2_Stream6_Handler
	.word DMA2_Stream7_Handler
	.word USART6_Handler
	.word I2C3_EV_Handler
	.word I2C3_ER_Handler
	.word OTG_HS_EP1_OUT_Handler
	.word OTG_HS_EP1_IN_Handler
	.word OTG_HS_WKUP_Handler
	.word OTG_HS_Handler
	.word DCMI_Handler
	.word 0
	.word 0
	.word FPU_Handler
	.word 0
	.word 0
	.word SPI4_Handler
	.word 0
	.word 0
	.word SAI1_Handler
	.word 0
	.word 0
	.word 0
	.word SAI2_Handler
	.word QuadSPI_Handler
	.word HDMI_CEC_Handler
	.word SPDIF_Rx_Handler
	.word FMPI2C1_Handler
	.word FMPI2C1_error_Handler

/* -------------------------------------------------------------
 *  RESET HANDLER
 * ------------------------------------------------------------- */
.section .text.Reset_Handler, "ax", %progbits
@ .weak Reset_Handler
.type    Reset_Handler, %function
.global  Reset_Handler

Reset_Handler:
	cpsid i                 /* Disable interrupts globally */

	/* safe INIT: stack, data copy, bss zero, SystemInit(), etc. */
    ldr   sp, =_stack_top

	dsb

	/*Copying data from flash to sram*/
	CopyDataInit:
		ldr r0, =_sidata		// the source address in the flash
		ldr r1, =_sdata			// the start address of destination in the sram
		ldr r2, =_edata			// the end address of the destination
	
	LoopCopyDataInit:
		cmp r1, r2		         /* r2 := _edata - _sdata (bytes) */
		beq  FillZerobss    	/* nothing to copy */

		itt lt				
		ldrlt r3, [r0], #4		// word stored at address held by r0 => _sidata(flash)
		strlt r3, [r1], #4		// store word at address held by r1 => _sdata(sram)
		blt LoopCopyDataInit
		
	/*Zero fiill the bss segment */
	FillZerobss:
		ldr r0, =_sbss			// The start address of bss 
		ldr r1, =_ebss			// The end address of bss
		movs r2, #0
			
	LoopFillZerobss:
		cmp r0, r1		        /* r1 := _ebss - _sbss (bytes) */
		beq bss_done

		it lt
		strlt r2, [r0], #4		//Store 0 at address held by r0
		blt LoopFillZerobss

	bss_done:
		//bkpt #0
	/* ---------------------------------------------------------
	* Relocate vector table to RAM
	* --------------------------------------------------------- */
	ldr r0, =_vectors_start              // source: flash vector table 
	ldr r1, =_ram_vector_start     		// destination: SRAM 
	ldr r2, =_vectors_end

    subs r2, r2, r0               // r2 = size in bytes
	copy_loop:
		cmp r2, #0
		beq set_vtor
		ldr r3, [r0], #4
		str r3, [r1], #4
		subs r2, r2, #4
		b copy_loop

	set_vtor: 
		ldr r0, =0xE000ED08             // SCB->VTOR 
		ldr r1, =_ram_vector_start
		str r1, [r0]
		dsb
		isb

	branches:
		/* Call __libc_init_array(): C++ constructors */
		bl      __libc_init_array

		//bkpt #0

		/* Call SystemInit()  */
		bl      SystemInit

		//bkpt #0

		/* enable interrupts only after clocks/peripherals are stable */
    	cpsie i                 /* enable interrupts globally */

		/* Call main() */
		bl      main
	
	/* If main returns — loop forever */
	infinite_loop:
		b       infinite_loop
		
	.size Reset_Handler, . - Reset_Handler
		
/* ============================================================= */
/*  Default handler — all weak handlers alias to this            */
/* ============================================================= */

.section .text.Default_Handler, "ax", %progbits
.global  Default_Handler
.type    Default_Handler, %function
	
Default_Handler:
    //bkpt #0
	b .
	.size Default_Handler, . - Default_Handler

/* ============================================================= */
/*  Weak aliases for all exceptions / interrupts                 */
/* ============================================================= */
.macro  weak_handler name
	.weak   \name
	.thumb_set \name, Default_Handler
.endm

	/*Core Exceptions*/
	weak_handler NMI_Handler
    weak_handler HardFault_Handler
    weak_handler MemManage_Handler
    weak_handler BusFault_Handler
    weak_handler UsageFault_Handler
    weak_handler SVC_Handler
    weak_handler DebugMon_Handler
    weak_handler PendSV_Handler
    weak_handler SysTick_Handler
	
	/* Peripheral interrupts*/
	weak_handler WWDG_Handler
	weak_handler PVD_Handler
	weak_handler TAMP_STAMP_Handler
	weak_handler RTC_WKUP_Handler
	weak_handler FLASH_Handler
	weak_handler RCC_Handler
	weak_handler EXTI0_IRQHandler
	weak_handler EXTI1_IRQHandler
	weak_handler EXTI2_IRQHandler
	weak_handler EXTI3_IRQHandler
	weak_handler EXTI4_IRQHandler
	weak_handler DMA1_Stream0_Handler
	weak_handler DMA1_Stream1_Handler
	weak_handler DMA1_Stream2_Handler
	weak_handler DMA1_Stream3_Handler
	weak_handler DMA1_Stream4_Handler
	weak_handler DMA1_Stream5_Handler
	weak_handler DMA1_Stream6_Handler
	weak_handler ADC_Handler
	weak_handler CAN1_TX_Handler
	weak_handler CAN1_RX0_Handler
	weak_handler CAN1_RX1_Handler
	weak_handler CAN1_SCE_Handler
	weak_handler EXTI9_5_IRQHandler
	weak_handler TIM1_BRK_TIM9_Handler
	weak_handler TIM1_UP_TIM10_Handler
	weak_handler TIM1_TRG_COM_TIM11_Handler
	weak_handler TIM1_CC_Handler
	weak_handler TIM2_Handler
	weak_handler TIM3_Handler
	weak_handler TIM4_Handler
	weak_handler I2C1_EV_Handler
	weak_handler I2C1_ER_Handler
	weak_handler I2C2_EV_Handler
	weak_handler I2C2_ER_Handler
	weak_handler SPI1_Handler
	weak_handler SPI2_Handler
	weak_handler USART1_Handler
	weak_handler USART2_Handler
	weak_handler USART3_Handler
	weak_handler EXTI15_10_IRQHandler
	weak_handler RTC_Alarm_Handler
	weak_handler OTG_FS_WKUP_Handler
	weak_handler TIM8_BRK_TIM12_Handler
	weak_handler TIM8_UP_TIM13_Handler
	weak_handler TIM8_TRG_COM_TIM14_Handler
	weak_handler TIM8_CC_Handler
	weak_handler DMA1_Stream7_Handler
	weak_handler FMC_Handler
	weak_handler SDIO_Handler
	weak_handler TIM5_Handler
	weak_handler SPI3_Handler
	weak_handler UART4_Handler
	weak_handler UART5_Handler
	weak_handler TIM6_DAC_Handler
	weak_handler TIM7_Handler
	weak_handler DMA2_Stream0_Handler
	weak_handler DMA2_Stream1_Handler
	weak_handler DMA2_Stream2_Handler
	weak_handler DMA2_Stream3_Handler
	weak_handler DMA2_Stream4_Handler
	weak_handler CAN2_TX_Handler
	weak_handler CAN2_RX0_Handler
	weak_handler CAN2_RX1_Handler
	weak_handler CAN2_SCE_Handler
	weak_handler OTG_FS_Handler
	weak_handler DMA2_Stream5_Handler
	weak_handler DMA2_Stream6_Handler
	weak_handler DMA2_Stream7_Handler
	weak_handler USART6_Handler
	weak_handler I2C3_EV_Handler
	weak_handler I2C3_ER_Handler
	weak_handler OTG_HS_EP1_OUT_Handler
	weak_handler OTG_HS_EP1_IN_Handler
	weak_handler OTG_HS_WKUP_Handler
	weak_handler OTG_HS_Handler
	weak_handler DCMI_Handler
	weak_handler FPU_Handler
	weak_handler SPI4_Handler
	weak_handler SAI1_Handler
	weak_handler SAI2_Handler
	weak_handler QuadSPI_Handler
	weak_handler HDMI_CEC_Handler
	weak_handler SPDIF_Rx_Handler
	weak_handler FMPI2C1_Handler
	weak_handler FMPI2C1_error_Handler

