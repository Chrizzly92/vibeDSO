// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
inline bool lcd_data=true;
constexpr int TFT_nCS_Port=0,TFT_RS_Port=0;
constexpr int TFT_nCS_Bit=13,TFT_RS_Bit=14;
#define SetToLow(port,mask) do { if((mask)==(1U<<14)) lcd_data=false; } while(0)
#define SetToHigh(port,mask) do { if((mask)==(1U<<14)) lcd_data=true; } while(0)
inline uint16_t lcd_image[240][320]{};
inline unsigned lcd_bytes=0,lcd_pixels=0;
inline bool lcd_bad=false;
inline uint8_t lcd_command=0,lcd_args[4]{};
inline unsigned lcd_arg_count=0;
inline int lcd_x=0,lcd_y=0,lcd_x0=0,lcd_x1=319,lcd_y0=0,lcd_y1=239;
inline unsigned lcd_pixel_bytes=0;
inline uint16_t lcd_pixel=0;
inline void LcdHostWriteByte(uint8_t value) {
 ++lcd_bytes;
 if(!lcd_data) {
  lcd_command=value; lcd_arg_count=0;
  if(value==0x2c) { lcd_x=lcd_x0; lcd_y=lcd_y0; lcd_pixel_bytes=0; }
  return;
 }
 if(lcd_command==0x2a || lcd_command==0x2b) {
  if(lcd_arg_count>=4) { lcd_bad=true; return; }
  lcd_args[lcd_arg_count++]=value;
  if(lcd_arg_count==4) {
   const int first=(lcd_args[0]<<8)|lcd_args[1], last=(lcd_args[2]<<8)|lcd_args[3];
   if(lcd_command==0x2a) { lcd_x0=first; lcd_x1=last; }
   else { lcd_y0=first; lcd_y1=last; }
   if(first>last || first<0 || last>=(lcd_command==0x2a?320:240)) lcd_bad=true;
  }
 } else if(lcd_command==0x2c) {
  lcd_pixel=uint16_t((lcd_pixel<<8)|value);
  if(++lcd_pixel_bytes==2) {
   if(lcd_x<0 || lcd_x>=320 || lcd_y<0 || lcd_y>=240) lcd_bad=true;
   else lcd_image[lcd_y][lcd_x]=lcd_pixel;
   ++lcd_pixels; lcd_pixel_bytes=0;
   if(++lcd_x>lcd_x1) { lcd_x=lcd_x0; if(++lcd_y>lcd_y1) lcd_y=lcd_y0; }
  }
 }
}
inline void write_comm(uint8_t value) { lcd_data=false; LcdHostWriteByte(value); lcd_data=true; }
inline void write_data(uint8_t value) { lcd_data=true; LcdHostWriteByte(value); }
