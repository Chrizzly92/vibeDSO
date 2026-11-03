// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#define DSO_SOURCE_ENGINE
#define DSO_LINK_HOST_TEST
#include "../src/link.cpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
uint16_t raw[1024],upper[1024]; dso::ExportFrame available;
int16_t mirror_top[300],mirror_bottom[300]; dso::MirrorView available_view;
extern "C" int DsoReadExportFrame(dso::ExportFrame* f) { *f=available; return available.count!=0; }
extern "C" int DsoReadMirrorView(dso::MirrorView* v) { *v=available_view; return v->top && v->bottom; }
extern "C" void DsoControl(const char*,dso::ControlState* s) { s->timebase=18; }
unsigned checks=0;
void check(bool good,const char* label) { if(!good) { printf("FAIL %s\n",label); exit(1); } ++checks; printf("PASS %s\n",label); }
uint16_t u16(const uint8_t* p) { return p[0]|uint16_t(p[1])<<8; }
uint32_t u32(const uint8_t* p) { return u16(p)|uint32_t(u16(p+2))<<16; }
void command(const char* text) { for(;*text;++text) { const unsigned pos=(64-dma5.CNDTR)&63; rx[pos]=*text; dma5.CNDTR=pos==63?64:63-pos; } LinkPoll(); }
int main() {
 for(unsigned i=0;i<1024;++i) { raw[i]=1000+i; upper[i]=2100+i; }
 available.samples=raw; available.count=1024; available.ticks=56; available.sequence=1; available.sensitivity=6; available.zero=2048; available.gain=1076;
 link_dma.ISR=2; LinkInit(); check(dma5.CNDTR==64 && dma5.CCR==4257 && (uart.CR3&192)==192,"UART RX ring DMA and TX DMA start independently from ADC DMA");
 command("SNAP\n");
 check(tx_busy && dma4.CNDTR==2100 && dma4.CCR==145,"snapshot starts bounded nonblocking low-priority USART DMA");
 const auto* p=exporter.data();
 check(!memcmp(p,"DSO1",4) && p[5]==1 && u16(p+28)==1024 && u16(p+44)==2048,"raw packet carries typed sample count and payload size");
 check(u16(p+48)==1000 && u16(p+48+2046)==2023 && u16(p+32)==2048 && u16(p+36)==1076,"raw ADC samples and calibration metadata are preserved");
 check(u32(p+2096)==dso::crc32(p,2096),"CRC protects metadata and entire sample payload");
 const auto frozen=p[48]; raw[0]=12; LinkPoll(); check(p[48]==frozen && tx_busy,"acquisition changes cannot mutate an in-flight export packet");
 dma4.CNDTR=0; LinkPoll(); available.maximum=upper; available.sequence=2;
 command("SNAP\n"); p=exporter.data();
 check(p[5]==2 && u16(p+28)==512 && (u16(p+30)&16) && u32(p+16)==112 && u16(p+38)==1024,"peak export declares two-bin aggregation and adjusted interval instead of pretending to be raw");
 check(u16(p+48)==12 && u16(p+50)==2101,"peak aggregation preserves min and max of both original bins");
 dma4.CNDTR=0; LinkPoll(); available.maximum=nullptr; available.sequence=4; SystemMilliseconds=1000;
 command("STREAM 5\n"); check(tx_busy && u32(exporter.data()+40)==1,"live export reports omitted publication sequences");
 dma4.CNDTR=0; LinkPoll(); available.sequence=5; SystemMilliseconds=1199; LinkPoll(); check(!tx_busy,"stream export obeys its configured maximum cadence");
 SystemMilliseconds=1200; LinkPoll(); check(tx_busy,"stream resumes asynchronously when cadence is due");
 dma4.CNDTR=0; LinkPoll(); command("STOP\n"); available.sequence=6; SystemMilliseconds=2000; LinkPoll(); check(!tx_busy,"STOP terminates live transfer without touching acquisition");
 check(dso::crc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926,"wire CRC agrees with the standard known vector");

 check((link_dma.ISR&2)!=0,"UART DMA flag handling preserves ADC channel-one status bits");
 dso::FrameExporter malformed; const char* bad="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxSNAP\n";
 for(;*bad;++bad) malformed.receive(*bad);
 check(!malformed.wants(1000),"overlong input cannot execute a command suffix");
 for(const char* p="SNAP\n";*p;++p) malformed.receive(*p);
 check(malformed.wants(1000),"parser recovers at the next complete line");
 FILE* fixture=fopen("analysis/link-fixture.bin","wb"); fwrite(exporter.data(),1,exporter.size(),fixture); fclose(fixture);

 for(unsigned i=0;i<300;++i) { mirror_top[i]=100+i%10; mirror_bottom[i]=120+i%10; }
 available_view.top=mirror_top; available_view.bottom=mirror_bottom; available_view.frame.ticks=56*65536; available_view.frame.sensitivity=6; available_view.frame.sequence=10; available_view.frame.count=1024;
 available_view.text_count=1; auto& caption=available_view.texts[0]; caption.x=8; caption.width=36; caption.color=0xffff; caption.length=1; caption.chars[0]='A';
 memcpy(available_view.power,"=R-8.00",7); available_view.power_color=0x07e0;
 SystemMilliseconds=2000; command("MIRROR 20\n"); const auto* view_packet=exporter.data();
 check(tx_busy && view_packet[5]==3 && u16(view_packet+28)==300 && dma4.CNDTR==701,"mirror sends compact 300-column scene instead of a full sample record");
 check(view_packet[88]==100 && view_packet[89]==120 && u16(view_packet+44)==649,"screen packet carries the actual drawn top/bottom spans and text descriptors");
 const auto frozen_mirror=view_packet[88]; mirror_top[0]=17; LinkPoll();
 check(view_packet[88]==frozen_mirror,"in-flight mirror geometry stays frozen while display state changes");
 dma4.CNDTR=0; LinkPoll(); SystemMilliseconds=2050; LinkPoll();
 check(!tx_busy,"unchanged mirror is not resent at the high-rate cadence");
 available_view.frame.sequence=11; SystemMilliseconds=2100; LinkPoll(); check(tx_busy,"changed display can refresh faster than the former five-frame record limit");
 dma4.CNDTR=0; LinkPoll(); SystemMilliseconds=3100; LinkPoll(); check(tx_busy,"static display emits a one-second heartbeat so HOLD does not time out");
 dma4.CNDTR=0; LinkPoll(); SystemMilliseconds=3150; command("VIEW\n"); check(tx_busy && exporter.data()[5]==3,"VIEW requests a single display scene");
 dma4.CNDTR=0; LinkPoll(); command("SNAP\n"); check(tx_busy && exporter.data()[5]==1 && dma4.CNDTR==2100,"full-record snapshot remains available after mirror mode");
 const auto frozen_control=exporter.data()[5]; command("@42 GET\n");
 check(exporter.data()[5]==frozen_control && dma4.CNDTR==2100,"control reception cannot overwrite an active frame DMA packet");
 dma4.CNDTR=0; LinkPoll();
 check(tx_busy && dma4.CNDTR==84 && exporter.data()[5]==4 && u16(exporter.data()+50)==42,"typed control acknowledgement is transmitted next with matching request ID");
 printf("%u checks passed (UART DMA and framed export).\n",checks);
}
