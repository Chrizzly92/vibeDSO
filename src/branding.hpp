// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "Screen.h"
namespace vibe {
constexpr const char* version="0.37";
constexpr const char* build="037";
// Use the existing font ROM; scaled boot lettering needs no extra bitmap buffer.
inline void large_char(int x,int y,unsigned char ch) {
 const U8* data=ASC8X16.Array+(ch-ASC8X16.IndexOfs)*16;
 for(int row=0;row<16;++row) for(int col=0;col<8;++col)
  if(data[row]&(0x80>>col)) FillRect(x+col*3,y+row*3,3,3,clAqua);
}
inline void splash() {
 FillRect(0,0,320,240,clBlack);
 PutsGenic(108,96,(U8*)"vibe",clWhite,clBlack,&ASC8X16);
 large_char(140,68,'D'); large_char(164,68,'S'); large_char(188,68,'O');
 // Five blank pixels between visible lettering and the one-pixel frame.
 FillRect(103,68,112,1,clAqua); FillRect(103,114,112,1,clAqua);
 FillRect(103,68,1,47,clAqua); FillRect(214,68,1,47,clAqua);
 PutsGenic(116,144,(U8*)__DATE__,clWhite,clBlack,&ASC8X16);
 PutsGenic(124,164,(U8*)"Build 037",clDimgray,clBlack,&ASC8X16);
 PutsGenic(48,216,(U8*)"github.com/Chrizzly92/vibeDSO",clDimgray,clBlack,&ASC8X16);
}
}
