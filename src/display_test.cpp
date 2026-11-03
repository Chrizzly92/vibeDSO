// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
// Static panel test: does not start the capture engine or write EEPROM.
extern "C" void DisplayTest(void) {
 ClrScreen();
 FillRect(0,0,320,1,clWhite); FillRect(0,239,320,1,clWhite);
 FillRect(0,0,1,240,clWhite); FillRect(319,0,1,240,clWhite);
 FillRect(10,17,300,1,clGreen); FillRect(10,216,300,1,clGreen);
 FillRect(10,17,1,200,clGreen); FillRect(309,17,1,200,clGreen);
 const U16 colors[]={clRed,clGreen,clBlue,clYellow,clHotpink,clAqua,clGray1,clWhite};
 for(int i=0;i<8;++i) FillRect(i*40,64,40,32,colors[i]);
 PutsGenic(12,24,(U8*)"LEFT x=12",clWhite,clBlack,&ASC8X16);
 PutsGenic(204,24,(U8*)"RIGHT x=204",clWhite,clBlack,&ASC8X16);
 PutsGenic(12,112,(U8*)"DSO138 LCD TEST v0.8",clWhite,clBlack,&ASC8X16);
 PutsGenic(12,144,(U8*)"8 bars, outer white frame",clWhite,clBlack,&ASC8X16);
 PutsGenic(12,176,(U8*)"Green: x10..309 y17..216",clWhite,clBlack,&ASC8X16);
 PutsGenic(12,220,(U8*)"BOTTOM LEFT",clWhite,clBlack,&ASC8X16);
 PutsGenic(204,220,(U8*)"BOTTOM RIGHT",clWhite,clBlack,&ASC8X16);
 while(1) { __asm volatile("nop"); }
}
