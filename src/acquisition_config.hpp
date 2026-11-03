// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
namespace dso {
constexpr unsigned timer_dma_samples=512,continuous_dma_samples=4096;
static_assert(continuous_dma_samples>=4*1024,"Retain a complete record behind the processed DMA half");
static_assert((continuous_dma_samples&(continuous_dma_samples-1))==0,"DMA ring size must be a power of two");
}
