// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
constexpr uint32_t ADC_SR_EOC = 2;
constexpr uint32_t TIM_EGR_UG = 1;
// STM32F103 register bit values, matching the pinned official device header.
constexpr uint32_t ADC_CR1_EOCIE = 0x00000020;
constexpr uint32_t ADC_CR2_ADON = 0x00000001;
constexpr uint32_t ADC_CR2_CAL = 0x00000004;
constexpr uint32_t ADC_CR2_DMA = 0x00000100;
constexpr uint32_t ADC_CR2_EXTTRIG = 0x00100000;
constexpr uint32_t ADC_CR2_RSTCAL = 0x00000008;
constexpr uint32_t DMA_CCR_CIRC = 0x0020;
constexpr uint32_t DMA_CCR_EN = 0x0001;
constexpr uint32_t DMA_CCR_HTIE = 0x0004;
constexpr uint32_t DMA_CCR_MINC = 0x0080;
constexpr uint32_t DMA_CCR_MSIZE_0 = 0x0400;
constexpr uint32_t DMA_CCR_PL_1 = 0x2000;
constexpr uint32_t DMA_CCR_PSIZE_0 = 0x0100;
constexpr uint32_t DMA_CCR_TCIE = 0x0002;
constexpr uint32_t DMA_CCR_TEIE = 0x0008;
constexpr uint32_t DMA_ISR_HTIF1 = 0x00000004;
constexpr uint32_t DMA_ISR_TCIF1 = 0x00000002;
constexpr uint32_t DMA_ISR_TEIF1 = 0x00000008;
constexpr uint32_t RCC_AHBENR_DMA1EN = 0x0001;
constexpr uint32_t RCC_APB1ENR_TIM2EN = 0x00000001;
constexpr uint32_t RCC_APB2ENR_ADC1EN = 0x00000200;
constexpr uint32_t TIM_CCER_CC2E = 0x0010;
constexpr uint32_t TIM_CR1_CEN = 0x0001;

constexpr uint32_t ADC_CR2_CONT=0x00000002,ADC_CR2_SWSTART=0x00400000;
