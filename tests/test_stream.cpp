// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "../src/stream_measurements.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
unsigned checks=0;
void check(bool good,const char* label) { if(!good) { printf("FAIL %s\n",label); exit(1); } ++checks; printf("PASS %s\n",label); }
int main() {
 dso::StreamMeter meter; meter.reset();
 for(unsigned i=0;i<400000;++i) meter.push(i%100<25?2300:1900);
 auto m=dso::measure_stream(meter.view(),board::core_hz/100000,6,2048);
 check(m.timing_valid && m.frequency_uhz==1000000000ULL && m.duty_permille==250,"streaming meter resolves 1 kHz and 25 percent duty across blocks");
 check(m.mean_adc_q8==2000*256 && m.minimum==1900 && m.maximum==2300,"stream statistics retain real-sample mean and extrema");
 const auto rms=dso::codes_q8_to_uv(uint32_t(sqrt((252.0*252/4+148.0*148*3/4))*256),6);
 check(m.rms_uv>=rms && m.rms_uv-rms<100,"streaming RMS includes correct unequal-duty weighting about calibrated zero");
 meter.reset(1200000);
 for(unsigned i=0;i<4000000;++i) meter.push(i%1000000<250000?2300:1900);
 m=dso::measure_stream(meter.view(),board::core_hz/100000,6,2048);
 check(m.timing_valid && m.frequency_uhz==100000 && m.period_ns==10000000000ULL && m.high_ns==2500000000ULL,"stream meter resolves 0.1 Hz using multi-second edge history without raw-record storage");
 check(meter.view().stats.count>=1200000 && meter.view().stats.count<2400000,"long statistics use completed and current bounded windows");
 for(unsigned i=0;i<3100000;++i) meter.push(1900);
 m=dso::measure_stream(meter.view(),board::core_hz/100000,6,2048);
 check(!m.timing_valid,"stopped slow signal expires instead of retaining stale frequency forever");
 meter.reset(); for(unsigned i=0;i<200000;++i) meter.push(2048);
 m=dso::measure_stream(meter.view(),board::core_hz/100000,6,2048);
 check(m.valid && !m.timing_valid && m.pp_uv==0 && m.rms_uv==0,"DC input reports valid zero statistics without invented timing");
 meter.push(4095); m=dso::measure_stream(meter.view(),board::core_hz/100000,6,2048);
 check(m.clipped && !m.timing_valid,"raw clipping invalidates streaming timing");
 meter.reset(); check(!dso::measure_stream(meter.view(),board::core_hz/100000,6,2048).valid,"range reset removes prior long-window data");
 for(unsigned i=0;i<300000;++i) meter.push(i%1000<500?2200:1800);
 check(meter.midpoint()==2000,"stream signal midpoint follows extrema independently of zero and duty");
 printf("%u checks passed (streaming statistics and slow signal timing).\n",checks);
}
