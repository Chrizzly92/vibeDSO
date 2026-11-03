// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#define DSO_HOST_TEST
#include "../src/engine.cpp"
#include <stdio.h>
#include <stdlib.h>
void check(bool ok,const char* label) { if(!ok) { printf("FAIL %s\n",label); exit(1); } printf("PASS %s\n",label); }
void save(unsigned index) {
 SystemMilliseconds+=300; dirty|=Disp_Trace|Disp_Param; DsoDisplay();
 char power[7]; U16 color; PowerReadDisplay(power,&color);
 // Power text is telemetry only in schema4; the engine draws the Debug icon.
 dso::MirrorView view{}; check(DsoReadMirrorView(&view)!=0,"rendered scene is readable");
 dso::FrameExporter tx; for(const char* p="VIEW\n";*p;++p) tx.receive(*p);
 check(tx.prepare_mirror(view,SystemMilliseconds) && tx.size()<=854,"rendered scene fits bounded compact packet");
 char name[80]; sprintf(name,"analysis/mirror-%u.bin",index); FILE* f=fopen(name,"wb"); fwrite(tx.data(),1,tx.size(),f); fclose(f);
 sprintf(name,"analysis/mirror-%u.lcd",index); f=fopen(name,"wb");
 for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x) { const U16 c=x?framebuffer[y][x-1]:0; fwrite(&c,2,1,f); } fclose(f);
}
int main() {
 static U8 font[4096]; FILE* f=fopen("tools/dso138_font.bin","rb"); check(f && fread(font,1,sizeof font,f)==sizeof font,"original LCD font resource loads"); fclose(f);
 ASC8X16={font,8,16,8,16,32}; NoScopePanel=0; status=DSO_Hold; vsen=VS_1V; timebase=TB_02ms; sample_ticks=dso::timing(timebase).ticks;
 for(unsigned i=0;i<1024;++i) display_samples[i]=i%125<62?2300:2048;
 measured=dso::measure(display_samples,1024,sample_ticks,vsen,2048);
 SetFocus(FC_Timebase); save(0);
 SetFocus(FC_HPos); SetHPos(400); save(1);
 SetFocus(FC_Measurements); save(2);
 SetFocus(FC_Cursors); save(3);
 ToggleCursorMode(); SelectCursor(); save(4);
 SetFocus(FC_TrigEdge); SetTrigEdge(TE_Falling); save(5);
 SetFocus(FC_VPos); SetVPos(130); save(6);
 SetFocus(FC_Generator); ToggleGenerator(); save(7);
 SetVPos(0); vsen=VS_01V;
 for(unsigned i=0;i<1024;++i) display_samples[i]=(i%125<62)?3000:1000;
 save(8);check(old_clip_flags==3,"viewport overflow marked in both directions");
 for(unsigned i=0;i<1024;++i) display_samples[i]=(i%125<62)?4095:0;
 save(9);check(old_clip_flags==15,"ADC saturation marked separately from viewport overflow");
 check(framebuffer[23][295]==clRed && framebuffer[210][307]==clRed,"larger warning arrows occupy the full thirteen by seven pixel area");
 for(unsigned i=0;i<1024;++i) display_samples[i]=2048;
 save(10);check(old_clip_flags==0,"overload markers clear on a valid centered signal");
 check(framebuffer[23][295]==clBlack && framebuffer[210][307]==clBlack,"outer pixels of enlarged arrows restore cleanly");
 SetFocus(FC_Debug); save(11);
 check(text_cache[6].length>0 && debug_icon_cache==1,"debug menu shows telemetry and selected header");
 SetFocus(FC_HPos); save(12);
 check(text_cache[7].x==120 && text_cache[8].x==240,"HP value centered between left move and right center help");
 SetFocus(FC_Debug); ToggleMeasurementPanel(); save(14);
 SetFocus(FC_Timebase); save(15);
 check(text_cache[6].chars[0]=='P',"leaving ADC Debug page restores normal measurements");
 ToggleMeasurementPanel();
 host_power_color=clRed; SetFocus(FC_Timebase); save(13);
 check(debug_icon_cache==2,"power fault remains visible after hiding normal rail telemetry");
}
