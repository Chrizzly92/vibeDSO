// Copyright (C)2014 JYE Tech Ltd., All right reserved
// Original code: vendor/JYE-LICENSE.txt.
// Modified for vibeDSO, 2026 Chrizzly92; modifications licensed Apache-2.0.
//////////////////////////////////////////////////////////////////////////////
//
// 	Filename:	Board.h
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
//

#ifndef	Board_h

#define	Board_h

#include	"Common.h"

#include "stm32f10x.h"
//#include "stm32f10x_conf.h"

// TFT control ports

#define	TFT_nRESET_Port			GPIOB	
#define	TFT_nRESET_Bit				11		
#define	TFT_RS_Port					GPIOC
#define	TFT_RS_Bit					14	
#define	TFT_nCS_Port				GPIOC
#define	TFT_nCS_Bit					13	
#define	TFT_nWR_Port				GPIOC
#define	TFT_nWR_Bit				15	
#define	TFT_nRD_Port				GPIOB
#define	TFT_nRD_Bit					10	

#define	TFT_Port					(GPIOB->ODR)

#define	LED_Base					GPIOA
#define	LED_Port					(GPIOA->ODR)
#define	LED_Bit						15

// Pushbuttons
#define	PB_Port						(GPIOB->IDR)
#define	PB_Bits						0xF000


// Board register field positions and GPIO configuration nibbles.
#define SDIOEN 10
#define FSMCEN 8
#define CRCEN 6
#define FLITFEN 4
#define SRAMEN 2
#define DMA2EN 1
#define DMA1EN 0
#define DACEN 29
#define PWREN 28
#define BKPEN 27
#define CANEN 25
#define USBEN 23
#define I2C2EN 22
#define I2C1EN 21
#define UART5EN 20
#define UART4EN 19
#define USART3EN 18
#define USART2EN 17
#define SPI3EN 15
#define SPI2EN 14
#define WWDGEN 11
#define TIM7EN 5
#define TIM6EN 4
#define TIM5EN 3
#define TIM4EN 2
#define TIM3EN 1
#define TIM2EN 0
#define ADC3EN 15
#define USART1EN 14
#define TIM8EN 13
#define SPI1EN 12
#define TIM1EN 11
#define ADC2EN 10
#define ADC1EN 9
#define IOPGEN 8
#define IOPFEN 7
#define IOPEEN 6
#define IOPDEN 5
#define IOPCEN 4
#define IOPBEN 3
#define IOPAEN 2
#define AFIOEN 0
#define PLLMULL 18
#define AFIORST 0
#define IOPARST 2
#define IOPBRST 3
#define IOPCRST 4
#define IOPDRST 5
#define IOPERST 6
#define IOPFRST 7
#define IOPGRST 8
#define ADC1RST 9
#define ADC2RST 10
#define TIM1RST 11
#define SPI1RST 12
#define TIM8RST 13
#define USART1RST 14
#define ADC3RST 15
#define TIM2RST 0
#define TIM3RST 1
#define TIM4RST 2
#define TIM5RST 3
#define TIM6RST 4
#define TIM7RST 5
#define WWDGRST 11
#define SPI2RST 14
#define SPI3RST 15
#define USART2RST 17
#define USART3RST 18
#define UART4RST 19
#define UART5RST 20
#define I2C1RST 21
#define I2C2RST 22
#define USBRST 23
#define CAN1RST 25
#define BKPRST 27
#define PWRRST 28
#define DACRST 29
#define CEN 0
#define UDIS 1
#define URS 2
#define OPM 3
#define DIR 4
#define CMS 5
#define ARPE 7
#define CKD 8
#define CCPC 0
#define CCUS 2
#define CCDS 3
#define MMS 4
#define TI1S 7
#define OIS1 8
#define OIS1N 9
#define OIS2 10
#define OIS2N 11
#define OIS3 12
#define OIS3N 13
#define OIS4 14
#define SMS 0
#define TS 4
#define MSM 7
#define ETF 8
#define ETPS 12
#define ECE 14
#define ETP 15
#define UIE 0
#define CC1IE 1
#define CC2IE 2
#define CC3IE 3
#define CC4IE 4
#define COMIE 5
#define TIE 6
#define BIE 7
#define UDE 8
#define CC1DE 9
#define CC2DE 10
#define CC3DE 11
#define CC4DE 12
#define COMDE 13
#define TDE 14
#define UG 0
#define CC1S 0
#define OC1FE 2
#define OC1PE 3
#define OC1M 4
#define OC1CE 7
#define CC2S 8
#define OC2FE 10
#define OC2PE 11
#define OC2M 12
#define OC2CE 15
#define IC1PSC 2
#define IC1F 4
#define IC2PSC 10
#define IC2F 12
#define CC3S 0
#define OC3FE 2
#define OC3PE 3
#define OC3M 4
#define OC3CE 7
#define CC4S 8
#define OC4FE 10
#define OC4PE 11
#define OC4M 12
#define OC4CE 15
#define IC3PSC 2
#define IC3F 4
#define IC4PSC 10
#define IC4F 12
#define CC1E 0
#define CC1P 1
#define CC1NE 2
#define CC1NP 3
#define CC2E 4
#define CC2P 5
#define CC2NE 6
#define CC2NP 7
#define CC3E 8
#define CC3P 9
#define CC3NE 10
#define CC3NP 11
#define CC4E 12
#define CC4P 13
#define LOCK 8
#define GPIO_Mode_In 0x00
#define GPIO_Mode_Out50M 0x03
#define GPIO_CNF_GP_PP 0x00
#define GPIO_CNF_AF_PP 0x08
#define GPIO_CNF_AnalogIn 0x00
#define GPIO_CNF_Floating 0x04
#define GPIO_CNF_IPU 0x08
#define AWD 0
#define EOC 1
#define JEOC 2
#define JSTRT 3
#define STRT 4
#define AWDCH 0
#define EOCIE 5
#define AWDIE 6
#define JEOCIE 7
#define SCAN 8
#define AWDSGL 9
#define JAUTO 10
#define DISCEN 11
#define JDISCEN 12
#define DISCNUM 13
#define DUALMOD 16
#define JAWDEN 22
#define AWDEN 23
#define ADON 0
#define CONT 1
#define CAL 2
#define RSTCAL 3
#define DMA 8
#define ALIGN 11
#define JEXTSEL 12
#define JEXTTRIG 15
#define EXTSEL 17
#define EXTTRIG 20
#define JSWSTART 21
#define SWSTART 22
#define TSVREFE 23
#define SMP10 0
#define SMP11 3
#define SMP12 6
#define SMP13 9
#define SMP14 12
#define SMP15 15
#define SMP16 18
#define SMP17 21
#define SMP0 0
#define SMP1 3
#define SMP2 6
#define SMP3 9
#define SMP4 12
#define SMP5 15
#define SMP6 18
#define SMP7 21
#define SMP8 24
#define SMP9 27
#define SQ13 0
#define SQ14 5
#define SQ15 10
#define SQ16 15
#define L 20
#define SQ7 0
#define SQ8 5
#define SQ9 10
#define SQ10 15
#define SQ11 20
#define SQ12 25
#define SQ1 0
#define SQ2 5
#define SQ3 10
#define SQ4 15
#define SQ5 20
#define SQ6 25
#define JSQ1 0
#define JSQ2 5
#define JSQ3 10
#define JSQ4 15
#define JL 20

extern volatile U16 GTimer;
extern volatile U8 GTimeout;

extern volatile U16 TimerKeyScan;
extern	U8		GeneralBuf[];

// ====================================================
//	Macros
//

// ====================================================
//	Function Prototype Declarations
//
void	Clock_Init(void);
void Port_Init(void);
void	USART1_Init(void);
void	UartPutc(U8 ch, USART_TypeDef* USARTx);
void	uputs(U8 *s, USART_TypeDef* USARTx);
void	ADC2_Init(void);
U16	ADC_Poll(ADC_TypeDef * adc, U8 chn);
void	TIM3_Init(void);
void	TIM4_Init(void);
void	SysTick_Init(void);
void	TFT_Init_Ili9341(void);
void	write_comm(U8 commport);
void write_data(U8 data);
void assert_failed(U8 * file, U32 line);
void NVIC_Configuration(void);
void	OutputTLvl(void);

#endif	// Board_h


