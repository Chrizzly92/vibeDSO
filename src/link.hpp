// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "link_protocol.hpp"
extern "C" {
void LinkInit(void);
void LinkPoll(void);
int DsoReadMirrorView(dso::MirrorView* view);
int DsoReadExportFrame(dso::ExportFrame* frame);
void DsoControl(const char* command,dso::ControlState* state);
void DsoReadControlState(dso::ControlState* state);
void DsoResetView(void);
void DsoSetGenerator(uint32_t hz,int enabled);
void DsoSetCursor(int voltage,unsigned selected,unsigned position);
}
