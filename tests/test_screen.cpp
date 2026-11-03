// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#define DSO_LCD_HOST_TEST
#include "../src/Screen.cpp"
#include "../src/branding.hpp"
int main() {
 int checks=0;
 const auto check=[&](bool condition,const char* name) {
  if(!condition) { printf("FAIL %s\n",name); exit(1); }
  ++checks; printf("PASS %s\n",name);
 };
 FillRect(0,0,320,240,0x1234);
 check(lcd_pixels==76560,"shifted full logical screen clips exactly one physical column");
 check(lcd_image[0][0]==0 && lcd_image[0][1]==0x1234 && lcd_image[239][319]==0x1234,"translation leaves the physical left margin and fills through physical x319");
 FillRect(310,230,20,20,0xabcd);
 check(lcd_pixels==76650 && lcd_image[239][319]==0xabcd,"rectangles clip at right and bottom edges");
 const auto before=lcd_bytes;
 FillRect(0,0,0,240,1); FillRect(0,0,-1,10,1); FillRect(320,0,5,5,1);
 check(lcd_bytes==before,"empty or offscreen rectangles issue no commands");
 const U16 colors[]={0x001f,0x07e0,0xf800,0xffff};
 WriteColumn(318,0,colors,4);
 check(lcd_image[0][319]==0x001f && lcd_image[3][319]==0xffff,"column stream preserves RGB565 order at x319");
 PutcGenic(0,0,'A',0xffff,0,&ASC8X16);
 PutcGenic(311,224,'A',0xffff,0,&ASC8X16);
 bool same=true,has_ink=false;
 for(int y=0;y<16;++y) for(int x=0;x<8;++x) {
  if(lcd_image[y][x+1]!=lcd_image[224+y][312+x]) same=false;
  if(lcd_image[y][x+1]) has_ink=true;
 }
 check(same && has_ink,"real font glyph complete at both screen edges");
 check(!lcd_bad,"all actual Screen.cpp ILI9341 windows remain valid");

 bool shifted=true,blank_bottom=true;
 for(unsigned row=0;row<16;++row) for(unsigned col=0;col<8;++col) {
  const U16 expected=row && (Font_ASC8X16[('A'-32)*16+row-1]&(0x80>>col))?0xffff:0;
  shifted &= lcd_image[row][col+1]==expected;
 }
 for(unsigned ch=32;ch<127;++ch) blank_bottom &= Font_ASC8X16[(ch-32)*16+15]==0;
 check(shifted && blank_bottom,"ASCII glyphs move one pixel down within their original window without losing nonblank bottom pixels");

 const auto clipped_before=lcd_pixels; WriteColumn(319,0,colors,4);
 check(lcd_pixels==clipped_before,"translated rightmost offscreen column cannot wrap into the next row");
 PutcGenic(312,224,'A',0xffff,0,&ASC8X16);
 check(!lcd_bad,"partially clipped right-edge glyph streams only its physical visible width");
 ClrScreen(); bool cleared=true;
 for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) cleared &= lcd_image[y][x]==0;
 check(cleared,"clear screen includes the otherwise unused physical left column");

 FILE* font_file=fopen("analysis/lcd-font.bin","wb"); fwrite(Font_ASC8X16,1,sizeof(Font_ASC8X16),font_file); fclose(font_file);
 vibe::splash();
 check(!lcd_bad,"vibeDSO splash and repository footer fit the actual LCD windows");
 FILE* splash=fopen("analysis/vibeDSO-splash.lcd","wb"); fwrite(lcd_image,2,320*240,splash); fclose(splash);
 printf("%d screen protocol checks passed.\n",checks);
}
