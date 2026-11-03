// Copyright (C)2014 JYE Tech Ltd., All right reserved
// Original code: vendor/JYE-LICENSE.txt.
// Modified for vibeDSO, 2026 Chrizzly92; modifications licensed Apache-2.0.
//////////////////////////////////////////////////////////////////////////////
//
// 	Filename:	Board.c
//	Version:		
//	Data:		
//
//	Author:		Liu, Zemin
//	Company:	JYE Tech Ltd.
//	Web:		www.jyetech.com
//
//-----------------------------------------------------------------------------
//
// 	Target: 		STM32F103C8
// 	Tool chain: 	CodeSourcery G++
//
//-----------------------------------------------------------------------------
//	Required files:
//
//-----------------------------------------------------------------------------
//	Notes:
//
//
//-----------------------------------------------------------------------------
//	Revision History:
//
///////////////////////////////////////////////////////////////////////////////
//
//-----------------------------------------------------------------------------
// 	Includes
//-----------------------------------------------------------------------------

#include "stm32f10x.h"

#include "Common.h"
#include "Board.h"
#include	"libdso138.h"
#include "lcd_bus.hpp"
#include "test_signal.hpp"
#include "clock_hardware.hpp"

// ===========================================================
//	File Scope Variables
// ===========================================================
//
	
volatile U16 GTimer;
volatile U8 GTimeout;

volatile U16 TimerKeyScan;

//U8	GeneralBuf[50];

// ===========================================================
//	Function Definitions
// ===========================================================

//-----------------------------------------------------------------------------
// Clock_Init
//-----------------------------------------------------------------------------
//
void	Clock_Init(void)
{
 if(!board::configure_clock(*RCC,*FLASH,[]{})) {
  // Do not run measurements or the inverter from an unknown fallback clock.
  while(1) {}
 }
 SystemCoreClockUpdate();

 RCC->APB2RSTR = (0 << AFIORST) 	/*!< Alternate Function I/O reset */
			|(0 << IOPARST)     	/*!< I/O port A reset */
			|(0 << IOPBRST)     	/*!< I/O port B reset */
			|(0 << IOPCRST)     	/*!< I/O port C reset */
			|(0 << IOPDRST)     	/*!< I/O port D reset */
			|(0 << IOPERST)     	/*!< I/O port E reset */
			|(0 << IOPFRST)     	/*!< I/O port F reset */
			|(0 << IOPGRST)     	/*!< I/O port G reset */
			|(0 << ADC1RST)     	/*!< ADC 1 interface reset */
			|(0 << ADC2RST)     	/*!< ADC 2 interface reset */
			|(0 << TIM1RST)     	/*!< TIM1 Timer reset */
			|(0 << SPI1RST)     	/*!< SPI 1 reset */
			|(0 << TIM8RST)     	/*!< TIM8 Timer reset */
			|(0 << USART1RST)     	/*!< USART1 reset */
			|(0 << ADC3RST);     	/*!< ADC3 interface reset */

 RCC->APB1RSTR = (0 << TIM2RST) /*!< Timer 2 reset */
			|(0 << TIM3RST)     	/*!< Timer 3 reset */
			|(0 << TIM4RST)     	/*!< Timer 4 reset */
			|(0 << TIM5RST)     	/*!< Timer 5 reset */
			|(0 << TIM6RST)     	/*!< Timer 6 reset */
			|(0 << TIM7RST)     	/*!< Timer 7 reset */
			|(0 << WWDGRST)     	/*!< Window Watchdog reset */
			|(0 << SPI2RST)     	/*!< SPI 2 reset */
			|(0 << SPI3RST)     	/*!< SPI 3 reset */
			|(0 << USART2RST)     	/*!< USART 2 reset */
			|(0 << USART3RST)     	/*!< RUSART 3 reset */
			|(0 << UART4RST )     	/*!< UART 4 reset */
			|(0 << UART5RST)     	/*!< UART 5 reset */
			|(0 << I2C1RST)     	/*!< I2C 1 reset */
			|(0 << I2C2RST)     	/*!< I2C 2 reset */
			|(0 << USBRST)     	/*!< USB Device reset */
			|(0 << CAN1RST)     	/*!< CAN1 reset */
			|(0 << BKPRST)     	/*!< Backup interface reset */
			|(0 << PWRRST)     	/*!< Power interface reset */
			|(0 << DACRST);     	/*!< DAC interface reset */

 
 RCC->AHBENR = (0 << SDIOEN)
				|(0 << FSMCEN)
				|(0 << CRCEN)
				|(1 << FLITFEN)
				|(1 << SRAMEN)
				|(0 << DMA2EN)
				|(1 << DMA1EN);

 RCC->APB1ENR = (0 << DACEN)
				|(0 << PWREN)
				|(0 << BKPEN)
				|(0 << CANEN)
				|(0 << USBEN)
				|(0 << I2C2EN)
				|(0 << I2C1EN)
				|(0 << UART5EN)
				|(0 << UART4EN)
				|(0 << USART3EN)
				|(0 << USART2EN)
				|(0 << SPI3EN)
				|(0 << SPI2EN)
				|(0 << WWDGEN)
				|(0 << TIM7EN)
				|(0 << TIM6EN)
				|(0 << TIM5EN)
				|(1 << TIM4EN)
				|(1 << TIM3EN)
				|(1 << TIM2EN);

 RCC->APB2ENR = (0 << ADC3EN)
				|(1 << USART1EN)
				|(0 << TIM8EN)
				|(0 << SPI1EN)
				|(1 << TIM1EN)
				|(1 << ADC2EN)
				|(1 << ADC1EN)
				|(0 << IOPGEN)
				|(0 << IOPFEN)
				|(0 << IOPEEN)
				|(1 << IOPDEN)
				|(1 << IOPCEN)
				|(1 << IOPBEN)
				|(1 << IOPAEN)
				|(1 << AFIOEN);

 RCC->BDCR = 0x00000000;
 RCC->CSR = 0x00000000;


}

//-----------------------------------------------------------------------------
// Misc_Init
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PORT_Init
//-----------------------------------------------------------------------------
//
// This routine configures the crossbar and GPIO ports.
//
void Port_Init(void)
{
 GPIOA->CRL = ((GPIO_CNF_AnalogIn | GPIO_Mode_In) << (0*4))	// ADC1_IN0
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (1*4))
 			|((GPIO_CNF_Floating|GPIO_Mode_In) << (2*4))	
 			|((GPIO_CNF_Floating |GPIO_Mode_In) << (3*4))		
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (4*4))
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (5*4))
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (6*4))		
 			|((GPIO_CNF_AF_PP | GPIO_Mode_Out50M) << (7*4));		//Test signal


 GPIOA->CRH = ((GPIO_CNF_Floating| GPIO_Mode_In) << (8 - 8)*4)		// TrigIn
 			|((GPIO_CNF_AF_PP |GPIO_Mode_Out50M) << (9 - 8)*4)	// TX1
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (10 - 8)*4)		// RX1
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (11 - 8)*4)
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (12 - 8)*4)
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (13 - 8)*4)
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (14 - 8)*4)
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (15 - 8)*4);	// LED

 GPIOA->ODR = 0xFFFF;

 GPIOB->CRL = ((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (0*4))		// TFT port - D0
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (1*4))		// TFT port - D1
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (2*4))		// TFT port - D2
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (3*4))		// TFT port - D3
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (4*4))		// TFT port - D4
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (5*4))		// TFT port - D5
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (6*4))		// TFT port - D6	
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (7*4));		// TFT port - D7		

 GPIOB->CRH = ((GPIO_CNF_AF_PP| GPIO_Mode_Out50M) << ((8 - 8)*4))		// Output, Trigger level
 			|((GPIO_CNF_AF_PP| GPIO_Mode_Out50M) << ((9 - 8)*4))		// Output, Gen
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << ((10 - 8)*4))		// Output, TFT_nRD
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << ((11 - 8)*4))		// Output, TFT_nReset
 			|((GPIO_CNF_IPU | GPIO_Mode_In) << ((12 - 8)*4))		// SW4	
 			|((GPIO_CNF_IPU | GPIO_Mode_In) << ((13 - 8)*4))		// SW3
 			|((GPIO_CNF_IPU | GPIO_Mode_In) << ((14 - 8)*4))		// SW2
 			|((GPIO_CNF_IPU | GPIO_Mode_In) << ((15 - 8)*4));		// SW1

 GPIOB->ODR = 0xFFFF;

 GPIOC->CRH = ((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (13 - 8)*4)		// TFT_nCS
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (14 - 8)*4)		// TFT_RS
 			|((GPIO_CNF_GP_PP | GPIO_Mode_Out50M) << (15 - 8)*4);		// TFT_nWR

 GPIOC->ODR = 0xFFFF;
 
 GPIOD->CRL = ((GPIO_CNF_Floating | GPIO_Mode_In) << (0*4))		
 			|((GPIO_CNF_Floating | GPIO_Mode_In) << (1*4));		

 // Remap to make PB3 & PB4 available
 AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG;
 AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_1;
 
}

void	USART1_Init(void)
{
 // USART1 runs on APB2; board clock initialization sets APB2 equal to HCLK.
#ifdef DSO_SOURCE_ENGINE
 constexpr uint32_t baud=230400;
#else
 constexpr uint32_t baud=38400;
#endif
 USART1->CR1=0;
 USART1->CR2=0;
 USART1->CR3=0;
 USART1->BRR=(SystemCoreClock+baud/2)/baud;
 USART1->CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE;
}

void UartPutc(U8 ch, USART_TypeDef* USARTx)
{
 while(!(USARTx->SR & USART_SR_TXE)) {}
 USARTx->DR=ch;
}

void	uputs(U8 *s, USART_TypeDef* USARTx)
{
 while(*s != 0) {
 	UartPutc(*s, USARTx);
	s++;
 	}
}

void TIM3_Init(void)
{
 board::init_test_signal(*TIM3);
}

void	TIM4_Init(void)
{
 // Disable counter first
 TIM4->CR1 = (0 << CEN)    	    	//!<Counter enable //
		| (0 << UDIS)           		//!<Update disable //
		| (0 << URS)          		//!<Update request source //
		| (0 << OPM)          		//!<One pulse mode //
		| (0 << DIR)          		//!<Direction. 0: Up, 1: Down
		| (0 << CMS)          		//!<CMS[1:0] bits (Center-aligned mode selection) //
		| (1 << ARPE)          		//!<Auto-reload preload enable //
		| (0 << CKD);          		//!<CKD[1:0] bits (clock division for filtering) 0 = 1/1, 1 = 1/2, 2 = 1/4

 TIM4->CR2 = (0 << CCPC)           	//!<Capture/Compare Preloaded Control //
		| (0 << CCUS)           		//!<Capture/Compare Control Update Selection //
		| (0 << CCDS)           		//!<Capture/Compare DMA Selection //
		| (0 << MMS)           		//!<MMS[2:0] bits (Master Mode Selection) //
		| (0 << TI1S)           		//!<TI1 Selection //
		| (0 << OIS1)           		//!<Output Idle state 1 (OC1 output) //
		| (0 << OIS1N)           		//!<Output Idle state 1 (OC1N output) //
		| (0 << OIS2)           		//!<Output Idle state 2 (OC2 output) //
		| (0 << OIS2N)           		//!<Output Idle state 2 (OC2N output) //
		| (0 << OIS3)           		//!<Output Idle state 3 (OC3 output) //
		| (0 << OIS3N)           		//!<Output Idle state 3 (OC3N output) //
		| (0 << OIS4);           		//!<Output Idle state 4 (OC4 output) //

 TIM4->SMCR = (0 << SMS)           	//!<SMS[2:0] bits (Slave mode selection) //
		| (0 << TS)           		//!<TS[2:0] bits (Trigger selection) //
		| (0 << MSM)           		//!<Master/slave mode //
		| (0 << ETF)           		//!<ETF[3:0] bits (External trigger filter) //
		| (0 << ETPS)           		//!<ETPS[1:0] bits (External trigger prescaler) //
		| (0 << ECE)           		//!<External clock enable //
		| (0 << ETP);           		//!<External trigger polarity //

 
 TIM4->SR = 0x0000;
 TIM4->EGR = 0x0000;
 
//----------------------------------------------------------------------------
// TIMx capture/compare usage (x = 2 ~ 5, n = 1 ~ 4)
//
//	CCnS[1:0] :
//		00: CCn channel is configured as output.
//		01: CCn channel is configured as input, ICn is mapped on TI1.
//		10: CCn channel is configured as input, ICn is mapped on TI2.
//		11: CCn channel is configured as input, ICn is mapped on TRC. This mode is working only
//			if an internal trigger input is selected through TS bit (TIMx_SMCR register)
//		Note: 	CCnS bits are writable only when the channel is OFF (CCnE = 0 in TIMx_CCER).
//				Output compare mode
//
//	OCnM[2:0] :
//		000: Frozen
//		001: Set channel n to active level on match. 
//		010: Set channel n to inactive level on match. 
//		011: Toggle - OCnREF toggles when TIMx_CNT=TIMx_CCRn.
//		100: Force inactive level - OCnREF is forced low.
//		101: Force active level - OCnREF is forced high.
//		110: PWM mode 1 - In upcounting, channel n is active as long as TIMx_CNT<TIMx_CCRn
//			else inactive. In downcounting, channel 1 is inactive (OCnREF=0) as long as
//			TIMx_CNT>TIMx_CCRn else active (OCnREF=1).
//		111: PWM mode 2 - In upcounting, channel n is inactive as long as
//			TIMx_CNT<TIMx_CCRn else active. In downcounting, channel n is active as long as
//			TIMx_CNT>TIMx_CCRn else inactive.
//		Note: 1: These bits can not be modified as long as LOCK level 3 has been programmed
//				(LOCK bits in TIMx_BDTR register) and CC1S=00 (the channel is configured in output).
//			  2: In PWM mode 1 or 2, the OCREF level changes only when the result of the
//				comparison changes or when the output compare mode switches from "frozen" mode
//				to "PWM" mode.
//		
//	ICnPSC[1:0] :
//			This bit-field defines the ratio of the prescaler acting on CCn input (ICn).
//			The prescaler is reset as soon as CC1E= 0 (TIMx_CCER register).
//		00: no prescaler, capture is done each time an edge is detected on the capture input.
//		01: capture is done once every 2 events.
//		10: capture is done once every 4 events.
//		11: capture is done once every 8 events.
//		
//	ICnF[3:0] :
//			This bit-field defines the frequency used to sample TIn input and the length of the digital
//			filter applied to TIn. The digital filter is made of an event counter in which N events are
//			needed to validate a transition on the output:
//		0000: No filter, sampling is done at fDTS.
//		0001: fSAMPLING=fCK_INT, N=2.
//		0010: fSAMPLING=fCK_INT, N=4.
//		0011: fSAMPLING=fCK_INT, N=8.
//		0100: fSAMPLING=fDTS/2, N=6.
//		0101: fSAMPLING=fDTS/2, N=8.
//		0110: fSAMPLING=fDTS/4, N=6.
//		0111: fSAMPLING=fDTS/4, N=8.
//		1000: fSAMPLING=fDTS/8, N=6.
//		1001: fSAMPLING=fDTS/8, N=8.
//		1010: fSAMPLING=fDTS/16, N=5.
//		1011: fSAMPLING=fDTS/16, N=6.
//		1100: fSAMPLING=fDTS/16, N=8.
//		1101: fSAMPLING=fDTS/32, N=5.
//		1110: fSAMPLING=fDTS/32, N=6.
//		1111: fSAMPLING=fDTS/32, N=8.
//				Note: 	In current silicon revision, fDTS is replaced in the formula by CK_INT 
//						when ICnF[3:0]= 1, 2 or 3.
//		
// Output compare mode
// CH3 for VGEN generation
// -- Set OC3 to output mode (CC3S[1:0] = 00)
// -- Set output to PWM mode 1 (OC3M[2:0] = 110)
// -- Set OC4 to output mode (CC4S[1:0] = 00)
// -- Set output to PWM mode 1 (OC4M[2:0] = 110)

 TIM4->CCMR1 = (0 << CC1S)          	//!<CC1S[1:0] bits (Capture/Compare 1 Selection) 
		| (0 << OC1FE)           	 	//!<Output Compare 1 Fast enable 
		| (0 << OC1PE)           		//!<Output Compare 1 Preload enable 
		| (0 << OC1M)           	//!<OC1M[2:0] bits (Output Compare 1 Mode) 
		| (0 << OC1CE)           		//!<Output Compare 1Clear Enable 
		| (0 << CC2S)           		//!<CC2S[1:0] bits (Capture/Compare 2 Selection) 
		| (0 << OC2FE)           		//!<Output Compare 2 Fast enable 
		| (0 << OC2PE)           		//!<Output Compare 2 Preload enable 
		| (0 << OC2M)           		//!<OC2M[2:0] bits (Output Compare 2 Mode) 
		| (0 << OC2CE);           	//!<Output Compare 2 Clear Enable 

// Input capture mode
/*
 TIM4->CCMR1 = (0 << CC1S)          	//!<CC1S[1:0] bits (Capture/Compare 1 Selection) 
		| (0 << IC1PSC)           	//!<IC1PSC[1:0] bits (Input Capture 1 Prescaler) 
		| (0 << IC1F)           	 	//!<IC1F[3:0] bits (Input Capture 1 Filter) 
		| (0 << CC2S)           		//!<CC2S[1:0] bits (Capture/Compare 2 Selection) 
		| (0 << IC2PSC)           	//!<IC2PSC[1:0] bits (Input Capture 2 Prescaler) 
		| (0 << IC2F);           	 	//!<IC2F[3:0] bits (Input Capture 2 Filter) 
*/

// Output compare mode
 TIM4->CCMR2 = (0 << CC3S)          	//!<CC3S[1:0] bits (Capture/Compare 3 Selection) 
		| (0 << OC3FE)           	 	//!<Output Compare 3 Fast enable 
		| (0 << OC3PE)           		//!<Output Compare 3 Preload enable 
		| (0x06 << OC3M)           	//!<OC3M[2:0] bits (Output Compare 3 Mode) 
		| (0 << OC3CE)           		//!<Output Compare 3Clear Enable 
		| (0 << CC4S)           		//!<CC4S[1:0] bits (Capture/Compare 4 Selection) 
		| (0 << OC4FE)           		//!<Output Compare 4 Fast enable 
		| (0 << OC4PE)           		//!<Output Compare 4 Preload enable 
		| (0x06 << OC4M)           		//!<OC4M[2:0] bits (Output Compare 4 Mode) 
		| (0 << OC4CE);           	//!<Output Compare 4 Clear Enable 

// Input capture mode
/*
 TIM4->CCMR2 = (0 << CC3S)          	//!<CC3S[1:0] bits (Capture/Compare 3 Selection) 
		| (0 << IC3PSC)           	//!<IC3PSC[1:0] bits (Input Capture 3 Prescaler) 
		| (0 << IC3F)           	 	//!<IC3F[3:0] bits (Input Capture 3 Filter) 
		| (0 << CC4S)           		//!<CC4S[1:0] bits (Capture/Compare 4 Selection) 
		| (0 << IC4PSC)           	//!<IC4PSC[1:0] bits (Input Capture 4 Prescaler) 
		| (0 << IC4F);           	 	//!<IC4F[3:0] bits (Input Capture 4 Filter) 
*/

 TIM4->CCER = (0 << CC1E)           	//!<Capture/Compare 1 output enable //
		| (0 << CC1P)           		//!<Capture/Compare 1 output Polarity //
		| (0 << CC1NE)           		//!<Capture/Compare 1 Complementary output enable //
		| (0 << CC1NP)           		//!<Capture/Compare 1 Complementary output Polarity //
		| (0 << CC2E)           		//!<Capture/Compare 2 output enable //
		| (0 << CC2P)           		//!<Capture/Compare 2 output Polarity //
		| (0 << CC2NE)           		//!<Capture/Compare 2 Complementary output enable //
		| (0 << CC2NP)           		//!<Capture/Compare 2 Complementary output Polarity //
		| (1 << CC3E)           		//!<Capture/Compare 3 output enable //
		| (0 << CC3P)           		//!<Capture/Compare 3 output Polarity //
		| (0 << CC3NE)           		//!<Capture/Compare 3 Complementary output enable //
		| (0 << CC3NP)           		//!<Capture/Compare 3 Complementary output Polarity //
		| (1 << CC4E)           		//!<Capture/Compare 4 output enable //
		| (0 << CC4P);           		 //!<Capture/Compare 4 output Polarity //

 
 TIM4->CNT = 0x0000;
 
 TIM4->PSC = 1 - 1;		// Make 1M (36M/36) as basic clock
 
 TIM4->ARR = board::timer4_period - 1;	//  1KHz
 
 TIM4->CCR1 = 0x0000;	// This value must be set to smaller than ARR. Otherwise there is no
 						//	compare match and no output generated at pin.
 TIM4->CCR2 = 0x0000;
 TIM4->CCR3 = board::timer4_compare(0x800 - 1);
 TIM4->CCR4 = 0; // inverter soft-started by PowerInit/PowerTick after ADC2 calibration
 TIM4->DCR = 0x0000;
 TIM4->DMAR = 0x0000;

 TIM4->DIER = (0 << UIE)           	//!<Update interrupt enable //
		| (0 << CC1IE)           		//!<Capture/Compare 1 interrupt enable //
		| (0 << CC2IE)           		//!<Capture/Compare 2 interrupt enable //
		| (0 << CC3IE)           		//!<Capture/Compare 3 interrupt enable //
		| (0 << CC4IE)           		//!<Capture/Compare 4 interrupt enable //
		| (0 << COMIE)           		//!<COM interrupt enable //
		| (0 << TIE)           		//!<Trigger interrupt enable //
		| (0 << BIE)           		//!<Break interrupt enable //
		| (0 << UDE)           		//!<Update DMA request enable //
		| (0 << CC1DE)           		//!<Capture/Compare 1 DMA request enable //
		| (0 << CC2DE)           		//!<Capture/Compare 2 DMA request enable //
		| (0 << CC3DE)           		//!<Capture/Compare 3 DMA request enable //
		| (0 << CC4DE)           		//!<Capture/Compare 4 DMA request enable //
		| (0 << COMDE)           	//!<COM DMA request enable //
		| (0 << TDE);           		//!<Trigger DMA request enable //

 TIM4->EGR = TIM_EGR_UG;
 TIM4->SR = 0;
 TIM4->CR1 = (1 << CEN)    	    	//!<Counter enable //
		| (0 << UDIS)           		//!<Update disable //
		| (0 << URS)          		//!<Update request source //
		| (0 << OPM)          		//!<One pulse mode //
		| (0 << DIR)          		//!<Direction //
		| (0 << CMS)          		//!<CMS[1:0] bits (Center-aligned mode selection) //
		| (1 << ARPE)          		//!<Auto-reload preload enable //
		| (0 << CKD);          		//!<CKD[1:0] bits (clock division) //

}


void SysTick_Init(void)
{
 board::init_systick(*SysTick);
}

void	ADC2_Init(void)
{
// NOTE: Remember to program ADC clock in RCC->CFGR

  ADC2->SR = (0 << AWD)                         /*!<Analog watchdog flag */
		| (0 << EOC)                              /*!<End of conversion */
		| (0 << JEOC)                            /*!<Injected channel end of conversion */
		| (0 << JSTRT)                             /*!<Injected channel Start flag */
		| (0 << STRT);                             /*!<Regular channel Start flag */
  	
  ADC2->CR1 = (0 << AWDCH)            /*!<AWDCH[4:0] bits (Analog watchdog channel select bits) */
			| (0 << EOCIE)           /*!<Interrupt enable for EOC */
			| (0 << AWDIE)              /*!<AAnalog Watchdog interrupt enable */
			| (0 << JEOCIE)          /*!<Interrupt enable for injected channels */
			| (0 << SCAN )           /*!<Scan mode */
			| (0 << AWDSGL)             /*!<Enable the watchdog on a single channel in scan mode */
			| (0 << JAUTO)            /*!<Automatic injected group conversion */
			| (0 << DISCEN)            /*!<Discontinuous mode on regular channels */
			| (0 << JDISCEN)             /*!<Discontinuous mode on injected channels */
			| (0 << DISCNUM )           /*!<DISCNUM[2:0] bits (Discontinuous mode channel count) */
			| (0 << DUALMOD)             /*!<DUALMOD[3:0] bits (Dual mode selection) */
			| (0 << JAWDEN )            /*!<Analog watchdog enable on injected channels */
			| (0 << AWDEN);		/*!<Analog watchdog enable on regular channels */

  ADC2->CR2 = (0 << ADON)        	//           /*!<A/D Converter ON / OFF */
			| (0 << CONT)        	//          /*!<Continuous Conversion */
			| (0 << CAL)     		//           /*!<A/D Calibration */
			| (0 << RSTCAL)       	//            /*!<Reset Calibration */
			| (0 << DMA)     		//            /*!<Direct Memory access mode */
								//				0: DMA mode disabled
								//				1: DMA mode enabled
			| (0 << ALIGN)   		//            /*!<Data Alignment */
			| (0 << JEXTSEL)		//           /*!<JEXTSEL[2:0] bits (External event select for injected group) */
			| (0 << JEXTTRIG)		//           /*!<External Trigger Conversion mode for injected channels */
			| (0 << EXTSEL)		//            /*!<EXTSEL[2:0] bits (External Event Select for regular group) */
								//			For ADC2 and ADC2, the assigned triggers are:
								//				000: Timer 1 CC1 event
								//				001: Timer 1 CC2 event
								//				010: Timer 1 CC3 event
								//				011: Timer 2 CC2 event
								//				100: Timer 3 TRGO event
								//				101: Timer 4 CC4 event
								//				110: EXTI line11/TIM8_TRGO event (TIM8_TRGO is available only in high-density devices)
								//				111: SWSTART
			| (0 << EXTTRIG)		//              /*!<External Trigger Conversion mode for regular channels */
			| (0 << JSWSTART)	//            /*!<Start Conversion of injected channels */
			| (0 << SWSTART)		//              /*!<Start Conversion of regular channels */
			| (0 << TSVREFE);		//              /*!<Temperature Sensor and VREFINT Enable */

 // Sample time selection
 // SMPx[2:0]:
 //		000: 1.5 cycles
 //		001: 7.5 cycles
 //		010: 13.5 cycles
 //		011: 28.5 cycles
 //		100: 41.5 cycles
 //		101: 55.5 cycles
 //		110: 71.5 cycles
 //		111: 239.5 cycles
 ADC2->SMPR1 = (0 << SMP10) 	//           /*!<SMP10[2:0] bits (Channel 10 Sample time selection) */
			| (0 << SMP11) 	//            /*!<SMP11[2:0] bits (Channel 11 Sample time selection) */
			| (0 << SMP12) 	//              /*!<SMP12[2:0] bits (Channel 12 Sample time selection) */
			| (0 << SMP13)	//                 /*!<SMP13[2:0] bits (Channel 13 Sample time selection) */
			| (0 << SMP14)	//               /*!<SMP14[2:0] bits (Channel 14 Sample time selection) */
			| (0 << SMP15)	//            /*!<SMP15[2:0] bits (Channel 15 Sample time selection) */
			| (0 << SMP16) 	//             /*!<SMP16[2:0] bits (Channel 16 Sample time selection) */
			| (0 << SMP17); 	//               /*!<SMP17[2:0] bits (Channel 17 Sample time selection) */

 ADC2->SMPR2 = (0 << SMP0 )  	//        /*!<SMP0[2:0] bits (Channel 0 Sample time selection) */
			| (0 << SMP1)   	//            /*!<SMP1[2:0] bits (Channel 1 Sample time selection) */
			| (0 << SMP2)   	//              /*!<SMP2[2:0] bits (Channel 2 Sample time selection) */
			| (0 << SMP3) 	//             /*!<SMP3[2:0] bits (Channel 3 Sample time selection) */
			| (0 << SMP4 )  	//              /*!<SMP4[2:0] bits (Channel 4 Sample time selection) */
			| (0 << SMP5)   	//            /*!<SMP5[2:0] bits (Channel 5 Sample time selection) */
			| (0 << SMP6)   	//           /*!<SMP6[2:0] bits (Channel 6 Sample time selection) */
			| (0 << SMP7)  	//           /*!<SMP7[2:0] bits (Channel 7 Sample time selection) */
			| (0 << SMP8)  	//          /*!<SMP8[2:0] bits (Channel 8 Sample time selection) */
			| (0 << SMP9);  	//            /*!<SMP9[2:0] bits (Channel 9 Sample time selection) */
  
  ADC2->JOFR1 = 0x0000;
  ADC2->JOFR2 = 0x0000;
  ADC2->JOFR3 = 0x0000;
  ADC2->JOFR4 = 0x0000;
  
  ADC2->HTR = 0x0FFF;
  ADC2->LTR = 0x0000;

 //	L[3:0]: Regular channel sequence length, i.e. number of channels in the sequence.
 //		These bits are written by software to define the total number of conversions in the regular
 //		channel conversion sequence.
 //			0000: 1 conversion
 //			0001: 2 conversions
 //			.....
 //			1111: 16 conversions 
 //	SQn[4:0]: The order of conversion in regular sequence
 //		These bits are written by software with the channel number (0..17) assigned as the n-th conversion in the
 //		sequence to be converted.
 //
 ADC2->SQR1 = (0 << SQ13 )   //            /*!<SQ13[4:0] bits (13th conversion in regular sequence) */
			| (0 << SQ14)     	//              /*!<SQ14[4:0] bits (14th conversion in regular sequence) */
			| (0 << SQ15)    	//                /*!<SQ15[4:0] bits (15th conversion in regular sequence) */
			| (0 << SQ16)    	//              /*!<SQ16[4:0] bits (16th conversion in regular sequence) */
			| (0 << L );     	//             /*!<L[3:0] bits (Regular channel sequence length) */
  
  ADC2->SQR2 = (0 << SQ7)      //               /*!<SQ7[4:0] bits (7th conversion in regular sequence) */
			| (0 << SQ8)       	//              /*!<SQ8[4:0] bits (8th conversion in regular sequence) */
			| (0 << SQ9)     	//                /*!<SQ9[4:0] bits (9th conversion in regular sequence) */
			| (0 << SQ10)     	//               /*!<SQ10[4:0] bits (10th conversion in regular sequence) */
			| (0 << SQ11)      	//               /*!<SQ11[4:0] bits (11th conversion in regular sequence) */
			| (0 << SQ12);     	//               /*!<SQ12[4:0] bits (12th conversion in regular sequence) */
  
  ADC2->SQR3 = (0 << SQ1)     	//             /*!<SQ1[4:0] bits (1st conversion in regular sequence) */
			| (0 << SQ2)        	//            /*!<SQ2[4:0] bits (2nd conversion in regular sequence) */
			| (0 << SQ3)      	//              /*!<SQ3[4:0] bits (3rd conversion in regular sequence) */
			| (0 << SQ4)      	//             /*!<SQ4[4:0] bits (4th conversion in regular sequence) */
			| (0 << SQ5)      	//              /*!<SQ5[4:0] bits (5th conversion in regular sequence) */
			| (0 << SQ6);       	//             /*!<SQ6[4:0] bits (6th conversion in regular sequence) */
			
 //	JL[1:0]: Injected sequence length
 //		These bits are written by software to define the total number of conversions in the injected
 //		channel conversion sequence.
 //			00: 1 conversion
 //			01: 2 conversions
 //			10: 3 conversions
 //			11: 4 conversions
 //	JSQ4[4:0]: 4th conversion in injected sequence
 //		These bits are written by software with the channel number (0..17) assigned as the 4th in
 //		the sequence to be converted.
 //		Note: Unlike a regular conversion sequence, if JL[1:0] length is less than four, the channels
 //				are converted in a sequence starting from (4-JL). Example: ADC_JSQR[21:0] = 10
 //				00011 00011 00111 00010 means that a scan conversion will convert the following
 //				channel sequence: 7, 3, 3. (not 2, 7, 3) 
 //
  ADC2->JSQR = (0 << JSQ1)     //            /*!<JSQ1[4:0] bits (1st conversion in injected sequence) */  
			| (0 << JSQ2)       	//             /*!<JSQ2[4:0] bits (2nd conversion in injected sequence) */
			| (0 << JSQ3)       	//             /*!<JSQ3[4:0] bits (3rd conversion in injected sequence) */
			| (0 << JSQ4)       	//              /*!<JSQ4[4:0] bits (4th conversion in injected sequence) */
			| (0 << JL);        	//            /*!<JL[1:0] bits (Injected Sequence length) */

  // These registers are read-only
//  ADC2->JDR1;
//  ADC2->JDR2;
//  ADC2->JDR3;
//  ADC2->JDR4;
//  ADC2->DR;

 // RM0008: power up, reset calibration, then wait for CAL to clear.
 ADC2->CR2 |= ADC_CR2_ADON;
 Delay(1000);
 ADC2->CR2 |= ADC_CR2_RSTCAL;
 while(ADC2->CR2 & ADC_CR2_RSTCAL) {}
 ADC2->CR2 |= ADC_CR2_CAL;
 while(ADC2->CR2 & ADC_CR2_CAL) {}
}


U16	ADC_Poll(ADC_TypeDef * adc, U8 chn)
{
 // Assuming that the ADC refered has been properly initialized with channel and sample time selected.
  adc->SQR3 = (chn << SQ1);     	//             /*!<SQ1[4:0] bits (1st conversion in regular sequence) */
 
 // Start conversion
 adc->CR2 |= (1 << ADON); 
 while(!BitTest(adc->SR, (1 << EOC))) {
 	// Wait for end of conversion
 	}
 return (adc->DR);
}

void	TFT_Init_Ili9341(void)
{
 U8  tmp;

 // Reset TFT controller (Ili9341)
 SetToHigh(TFT_nRESET_Port, (1 << TFT_nRESET_Bit));
 Delay(5000);	// About 1.1ms
 SetToLow(TFT_nRESET_Port, (1 << TFT_nRESET_Bit));
 Delay(65000);	// About 15ms
 SetToHigh(TFT_nRESET_Port, (1 << TFT_nRESET_Bit));
 tmp = 10;
 while(tmp) {
 	Delay(65535);
	tmp--;
 	}
 
	write_comm(0xcf); 
	write_data(0x00);
	write_data(0xC1);
	write_data(0x30);

	write_comm(0xed); 
	write_data(0x67);
	write_data(0x03);
	write_data(0x12);
	write_data(0x81);

	write_comm(0xcb); 
	write_data(0x39);
	write_data(0x2c);
	write_data(0x00);
	write_data(0x34);
	write_data(0x02);

	write_comm(0xea); 
	write_data(0x00);
	write_data(0x00);

	write_comm(0xe8); 
	write_data(0x85);
	write_data(0x0a);
	write_data(0x78);

	write_comm(0xF7); 
	write_data(0x20);

	write_comm(0xC0); //Power control
	write_data(0x26); //VRH[5:0]

	write_comm(0xC1); //Power control
	write_data(0x01); //SAP[2:0];BT[3:0]

	write_comm(0xC5); //VCM control
	write_data(0x2b);
	write_data(0x2F);

	write_comm(0xc7); 
	write_data(0xc7);

	write_comm(0x3A); 
	write_data(0x55);

	write_comm(0x36); // Memory Access Control
//	write_data(0x08);
	write_data(0x20);
	
	write_comm(0xB1); // Frame Rate Control
	write_data(0x00);
	write_data(0x18);
	
	write_comm(0xB6); // Display Function Control
	write_data(0x0a);
	write_data(0xE2);
	
	write_comm(0xF2); // 3Gamma Function Disable
	write_data(0x00);
	write_comm(0x26); //Gamma curve selected
	write_data(0x01);
	write_comm(0xE0); //Set Gamma
	write_data(0x0f);
	write_data(0x1d);
	write_data(0x1a);
	write_data(0x09);
	write_data(0x0f);
	write_data(0x09);
	write_data(0x46);
	write_data(0x88);
	write_data(0x39);
	write_data(0x05);
	write_data(0x0f);
	write_data(0x03);
	write_data(0x07);
	write_data(0x05);
	write_data(0x00);

	write_comm(0XE1); //Set Gamma
	write_data(0x00);
	write_data(0x22);
	write_data(0x25);
	write_data(0x06);
	write_data(0x10);
	write_data(0x06);
	write_data(0x39);
	write_data(0x22);
	write_data(0x4a);
	write_data(0x0a);
	write_data(0x10);
	write_data(0x0c);
	write_data(0x38);
	write_data(0x3a);
	write_data(0x0F);

	write_comm(0x11); //Exit Sleep
//	delay(120);
	 tmp = 100;
	 while(tmp) {
	 	Delay(50000);
		tmp--;
	 	}
	write_comm(0x29); //display on	
 // Explicit native scroll origin and landscape address range. Do not inherit
 // a previous firmware's controller window/scroll state after a warm reset.
 write_comm(0x33);
 write_data(0); write_data(0); write_data(1); write_data(0x40); write_data(0); write_data(0);
 write_comm(0x37); write_data(0); write_data(0);
 write_comm(0x2a); write_data(0); write_data(0); write_data(1); write_data(0x3f);
 write_comm(0x2b); write_data(0); write_data(0); write_data(0); write_data(0xef);
//	write_comm(0x2C);	

 Delay(50000);
 Delay(50000);
 
}


void	write_comm(U8 commport)
{
 // Set TFT_nCS low
 SetToLow(TFT_nCS_Port, (1 << TFT_nCS_Bit));
 // Set up to access Index Register (RS == 0)
 SetToLow(TFT_RS_Port, (1 << TFT_RS_Bit));
// Delay(2);

 LcdWriteByte(commport);

 // Set up to access Data Register (RS == 1)
 SetToHigh(TFT_RS_Port, (1 << TFT_RS_Bit));
// Delay(2);

 // Set TFT_nCS high
 SetToHigh(TFT_nCS_Port, (1 << TFT_nCS_Bit));
 
}

void write_data(U8 data)
{
 // Set TFT_nCS low
 SetToLow(TFT_nCS_Port, (1 << TFT_nCS_Bit));

 // Set up to access Data Register (RS == 1)
 SetToHigh(TFT_RS_Port, (1 << TFT_RS_Bit));

 LcdWriteByte(data);

 // Set TFT_nCS high
 SetToHigh(TFT_nCS_Port, (1 << TFT_nCS_Bit));
 
}


void assert_failed(U8 * file, U32 line)
//void assert_failed((U8 *) file, U32 line)
{
 (void)file; (void)line;
 __disable_irq();
 while(1) {}
}


/**
  * @brief  Configures the nested vectored interrupt controller.
  * @param  None
  * @retval None
  */
void NVIC_Configuration(void)
{
 SCB->VTOR=FLASH_BASE;
 __DSB();
 __ISB();
 NVIC_SetPriority(TIM1_CC_IRQn,0);
 NVIC_EnableIRQ(TIM1_CC_IRQn);
 NVIC_SetPriority(ADC1_2_IRQn,0);
 NVIC_EnableIRQ(ADC1_2_IRQn);
 NVIC_SetPriority(DMA1_Channel1_IRQn,0);
 NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}


void	OutputTLvl(void)
{
 int level=int(GetTrigLvl())+int(GetVPosOfs())+0x800;
 if(level<0) level=0;
 if(level>4095) level=4095;
 TIM4->CCR3=board::timer4_compare(uint32_t(level));
}


