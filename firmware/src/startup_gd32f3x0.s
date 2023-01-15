/*
 * Copyright (c) 2020 Spacecraft-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

.section .text.start
.syntax unified
.align 4
.global _start
_start:
				.word	 __stack_top__						// Top of Stack
				.word	 Reset_Handler						// Reset Handler
				.word	 NMI_Handler						// NMI Handler
				.word	 HardFault_Handler					// Hard Fault Handler
				.word	 MemManage_Handler					// MPU Fault Handler
				.word	 BusFault_Handler					// Bus Fault Handler
				.word	 UsageFault_Handler					// Usage Fault Handler
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 SVC_Handler						// SVCall Handler
				.word	 DebugMon_Handler					// Debug Monitor Handler
				.word	 0									// Reserved
				.word	 PendSV_Handler						// PendSV Handler
				.word	 SysTick_Handler_cnt				// SysTick Handler

				// external interrupts handler
				.word	 WWDGT_IRQHandler					// 16:Window Watchdog Timer
				.word	 LVD_IRQHandler						// 17:LVD through EXTI Line detect
				.word	 RTC_IRQHandler						// 18:RTC through EXTI Line
				.word	 FMC_IRQHandler						// 19:FMC
				.word	 RCU_CTC_IRQHandler					// 20:RCU and CTC
				.word	 EXTI0_1_IRQHandler					// 21:EXTI Line 0 and EXTI Line 1
				.word	 EXTI2_3_IRQHandler					// 22:EXTI Line 2 and EXTI Line 3
				.word	 EXTI4_15_IRQHandler				// 23:EXTI Line 4 to EXTI Line 15
				.word	 TSI_IRQHandler						// 24:TSI
				.word	 DMA_Channel0_IRQHandler			// 25:DMA Channel 0 
				.word	 DMA_Channel1_2_IRQHandler			// 26:DMA Channel 1 and DMA Channel 2
				.word	 DMA_Channel3_4_IRQHandler			// 27:DMA Channel 3 and DMA Channel 4
				.word	 ADC_CMP_IRQHandler					// 28:ADC and Comparator 0-1
				.word	 TIMER0_BRK_UP_TRG_COM_IRQHandler	// 29:TIMER0 Break,Update,Trigger and Commutation
				.word	 TIMER0_Channel_IRQHandler			// 30:TIMER0 Channel Capture Compare
				.word	 TIMER1_IRQHandler					// 31:TIMER1
				.word	 TIMER2_IRQHandler					// 32:TIMER2
				.word	 TIMER5_DAC_IRQHandler				// 33:TIMER5 and DAC
				.word	 0									// Reserved
				.word	 TIMER13_IRQHandler					// 35:TIMER13
				.word	 TIMER14_IRQHandler					// 36:TIMER14
				.word	 TIMER15_IRQHandler					// 37:TIMER15
				.word	 TIMER16_IRQHandler					// 38:TIMER16
				.word	 I2C0_EV_IRQHandler					// 39:I2C0 Event
				.word	 I2C1_EV_IRQHandler					// 40:I2C1 Event
				.word	 SPI0_IRQHandler					// 41:SPI0
				.word	 SPI1_IRQHandler					// 42:SPI1
				.word	 USART0_IRQHandler					// 43:USART0
				.word	 USART1_IRQHandler					// 44:USART1
				.word	 0									// Reserved
				.word	 CEC_IRQHandler						// 46:CEC
				.word	 0									// Reserved
				.word	 I2C0_ER_IRQHandler					// 48:I2C0 Error
				.word	 0									// Reserved
				.word	 I2C1_ER_IRQHandler					// 50:I2C1 Error
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 USBFS_WKUP_IRQHandler				// 58:USBFS Wakeup
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 DMA_Channel5_6_IRQHandler			// 64:DMA Channel5 and Channel6 
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 0									// Reserved
				.word	 USBFS_IRQHandler					// 83:USBFS

.global firmware_version
firmware_version:
				.word	 0x000702

// reset Handler
.global Reset_Handler
.thumb_func
Reset_Handler:
				PUSH { R0 }
				CMP R0, #0
				BNE jump_system_init
				BLX	SystemInit
jump_system_init:
				LDR R0, =__data_start__
				LDR R1, =__data_flash_start__
				LDR R2, =__data_end__
				SUBS R2, R2, R0
				BLX memcpy
				LDR R0, =__bss_start__
				MOVS R1, #0
				LDR R2, =__bss_end__
				SUBS R2, R2, R0
				BLX memset
				POP { R0 }
				CMP R0, #0
				BEQ jump_firmware_main
				BLX debug_main
jump_firmware_main:
				BLX firmware_main
				B .

// dummy Exception Handlers
.thumb_func
.weak NMI_Handler
NMI_Handler:
.thumb_func
.weak HardFault_Handler
HardFault_Handler:
.thumb_func
.weak MemManage_Handler
MemManage_Handler:
.thumb_func
.weak BusFault_Handler
BusFault_Handler:
.thumb_func
.weak UsageFault_Handler
UsageFault_Handler:
.thumb_func
.weak SVC_Handler
SVC_Handler:
.thumb_func
.weak DebugMon_Handler
DebugMon_Handler:
.thumb_func
.weak PendSV_Handler
PendSV_Handler:
.thumb_func
.weak SysTick_Handler_cnt
SysTick_Handler_cnt:

// external interrupts handler
.thumb_func
.weak WWDGT_IRQHandler
WWDGT_IRQHandler:
.thumb_func
.weak LVD_IRQHandler
LVD_IRQHandler:
.thumb_func
.weak RTC_IRQHandler
RTC_IRQHandler:
.thumb_func
.weak FMC_IRQHandler
FMC_IRQHandler:
.thumb_func
.weak RCU_CTC_IRQHandler
RCU_CTC_IRQHandler:
.thumb_func
.weak EXTI0_1_IRQHandler
EXTI0_1_IRQHandler:
.thumb_func
.weak EXTI2_3_IRQHandler
EXTI2_3_IRQHandler:
.thumb_func
.weak EXTI4_15_IRQHandler
EXTI4_15_IRQHandler:
.thumb_func
.weak TSI_IRQHandler
TSI_IRQHandler:
.thumb_func
.weak DMA_Channel0_IRQHandler
DMA_Channel0_IRQHandler:
.thumb_func
.weak DMA_Channel1_2_IRQHandler
DMA_Channel1_2_IRQHandler:
.thumb_func
.weak DMA_Channel3_4_IRQHandler
DMA_Channel3_4_IRQHandler:
.thumb_func
.weak ADC_CMP_IRQHandler
ADC_CMP_IRQHandler:
.thumb_func
.weak TIMER0_BRK_UP_TRG_COM_IRQHandler
TIMER0_BRK_UP_TRG_COM_IRQHandler:
.thumb_func
.weak TIMER0_Channel_IRQHandler
TIMER0_Channel_IRQHandler:
.thumb_func
.weak TIMER1_IRQHandler
TIMER1_IRQHandler:
.thumb_func
.weak TIMER2_IRQHandler
TIMER2_IRQHandler:
.thumb_func
.weak TIMER5_DAC_IRQHandler
TIMER5_DAC_IRQHandler:
.thumb_func
.weak TIMER13_IRQHandler
TIMER13_IRQHandler:
.thumb_func
.weak TIMER14_IRQHandler
TIMER14_IRQHandler:
.thumb_func
.weak TIMER15_IRQHandler
TIMER15_IRQHandler:
.thumb_func
.weak TIMER16_IRQHandler
TIMER16_IRQHandler:
.thumb_func
.weak I2C0_EV_IRQHandler
I2C0_EV_IRQHandler:
.thumb_func
.weak I2C1_EV_IRQHandler
I2C1_EV_IRQHandler:
.thumb_func
.weak SPI0_IRQHandler
SPI0_IRQHandler:
.thumb_func
.weak SPI1_IRQHandler
SPI1_IRQHandler:
.thumb_func
.weak USART0_IRQHandler
USART0_IRQHandler:
.thumb_func
.weak USART1_IRQHandler
USART1_IRQHandler:
.thumb_func
.weak CEC_IRQHandler
CEC_IRQHandler:
.thumb_func
.weak I2C0_ER_IRQHandler
I2C0_ER_IRQHandler:
.thumb_func
.weak I2C1_ER_IRQHandler
I2C1_ER_IRQHandler:
.thumb_func
.weak DMA_Channel5_6_IRQHandler
DMA_Channel5_6_IRQHandler:
				B	   .

.thumb_func
USBFS_WKUP_IRQHandler:
				LDR R0, =0x80000E8
				LDR R0, [R0]
				BX  R0
					
.thumb_func
USBFS_IRQHandler:
				LDR R0, =0x800014C
				LDR R0, [R0]
				BX  R0
.thumb_func
.global jump_bootloader_sdio_handler
jump_bootloader_sdio_handler:
				LDR R0, =0x08000000
				LDR SP, [R0]
				LDR R0, =0x08000150
				LDR R0, [R0]
				BX  R0
