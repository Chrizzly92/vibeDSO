# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""DSO138 snapshot, record export and compact display mirror receiver.
Live port: pip install pyserial. Mirror GUI uses Tk; raw-record plot optionally matplotlib.
"""
from __future__ import annotations
import argparse,csv,dataclasses,pathlib,struct,sys,time,zlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from dso138_mirror import parse_scene,MirrorWindow
SENSITIVITY_V=(5,2,1,.5,.2,.1,.05,.02,.01)
@dataclasses.dataclass
class ControlReply:
    request:int; result:int; state:dict
@dataclasses.dataclass
class Frame:
    kind:int; sequence:int; published_ms:int; ticks:int; core_hz:int; count:int
    flags:int; zero:int; sensitivity:int; coupling:int; gain:int; source_count:int; skipped:int
    samples:list[tuple[int,int]]
    @property
    def interval(self): return self.ticks/self.core_hz
    def volts(self,code): return (code-self.zero)*SENSITIVITY_V[self.sensitivity-4]*275*self.gain/(1092*1024*25)
    def rows(self):
        for i,(low,high) in enumerate(self.samples):
            yield (self.sequence,self.published_ms,self.kind,self.flags,self.skipped,i,i*self.interval,low,high,self.volts(low),self.volts(high),
                   self.ticks,self.core_hz,self.source_count,self.zero,self.sensitivity,self.coupling,self.gain)
@dataclasses.dataclass
class MirrorFrame(Frame):
    scene:dict
    @property
    def interval(self): return self.ticks/(self.core_hz*65536)
    def rows(self):
        scale=SENSITIVITY_V[self.sensitivity-4]/25;zero=self.scene['zero_y']
        for i,(top,bottom) in enumerate(self.samples):
            yield (self.sequence,self.published_ms,self.kind,self.flags,self.skipped,i,i*self.interval,top,bottom,(zero-bottom)*scale,(zero-top)*scale,
                   self.ticks,self.core_hz,self.source_count,self.zero,self.sensitivity,self.coupling,self.gain)
class Decoder:
    def __init__(self): self.buffer=bytearray();self.bad_crc=0;self.discarded=0
    def feed(self,data):
        self.buffer.extend(data);frames=[]
        while True:
            at=self.buffer.find(b'DSO1')
            if at<0:
                if len(self.buffer)>3:self.discarded+=len(self.buffer)-3;del self.buffer[:-3]
                break
            if at:self.discarded+=at;del self.buffer[:at]
            if len(self.buffer)<48:break
            p=self.buffer;kind=p[5];count=struct.unpack_from('<H',p,28)[0];payload=struct.unpack_from('<H',p,44)[0]
            shape=(kind in (1,2) and 2<=count<=1024 and payload==count*(2 if kind==1 else 4)) or (kind==3 and count==300 and 640<=payload<=802) or (kind==4 and count==0 and payload==32)
            if p[4]!=1 or struct.unpack_from('<H',p,6)[0]!=48 or not shape or payload>2048:
                self.discarded+=1;del p[:1];continue
            total=52+payload
            if len(p)<total:break
            if zlib.crc32(p[:48+payload])!=struct.unpack_from('<I',p,48+payload)[0]:
                self.bad_crc+=1;del p[:1];continue
            if kind==4:
                schema,result,request,tb,mode,edge,focus,sen,cpl,flags,vp,hp,tl,reclen,gen,zero,ca,cb,_=struct.unpack_from('<BBHBBBBBBHhHhHIhHHH',p,48)
                if schema==1 and request and result<=4 and 1<=tb<=24 and mode<=2 and edge<=1 and focus<=9 and 4<=sen<=12 and cpl<=2 and 512<=reclen<=1024 and 0<=hp<=reclen-300 and -255<=vp<=255 and 1<=gen<=100000:
                    frames.append(ControlReply(request,result,dict(timebase=tb,mode=mode,edge=edge,focus=focus,sensitivity=sen,coupling=cpl,flags=flags,vpos=vp,hpos=hp,trigger=tl,reclen=reclen,generator=gen,zero=zero,cursor_a=ca,cursor_b=cb)))
                else:self.discarded+=total
                del p[:total];continue
            sequence,ms=struct.unpack_from('<II',p,8);ticks=struct.unpack_from('<Q',p,16)[0];core=struct.unpack_from('<I',p,24)[0]
            flags,zero=struct.unpack_from('<HH',p,30);sensitivity,coupling=p[34:36];gain,source_count=struct.unpack_from('<HH',p,36);skipped=struct.unpack_from('<I',p,40)[0]
            if not ticks or not core or not 4<=sensitivity<=12 or zero>4095 or not gain:
                self.discarded+=total;del p[:total];continue
            args=(kind,sequence,ms,ticks,core,count,flags,zero,sensitivity,coupling,gain,source_count,skipped)
            if kind==3:
                try:scene=parse_scene(p[48:48+payload])
                except (ValueError,UnicodeDecodeError,struct.error):self.discarded+=total;del p[:total];continue
                frames.append(MirrorFrame(*args,scene['spans'],scene))
            else:
                codes=struct.unpack_from('<'+'H'*(payload//2),p,48)
                samples=[(v,v) for v in codes] if kind==1 else list(zip(codes[::2],codes[1::2]))
                if any(low>4095 or high>4095 or low>high for low,high in samples):
                    self.discarded+=total;del p[:total];continue
                frames.append(Frame(*args,samples))
            del p[:total]
        return frames
CSV_HEADER=('sequence','published_ms','kind','flags','skipped_publications','index','t_record_s','adc_min','adc_max','voltage_min_v','voltage_max_v','sample_ticks','core_hz','source_count','zero_adc','sensitivity_code','coupling_code','gain_q10')
MIRROR_CSV_HEADER=('ui_revision','scene_ms','kind','flags','skipped_revisions','column','t_view_s','pixel_y_top','pixel_y_bottom','visible_voltage_min_v','visible_voltage_max_v','pixel_ticks_q16','core_hz','record_count','zero_adc','sensitivity_code','coupling_code','gain_q10')
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    src=ap.add_mutually_exclusive_group(required=True);src.add_argument('--port');src.add_argument('--decode-file',type=pathlib.Path)
    ap.add_argument('--baud',type=int,default=230400);ap.add_argument('--stream',action='store_true');ap.add_argument('--mirror',action='store_true',help='live display mirror with Tk viewer')
    ap.add_argument('--full-record',action='store_true',help='use legacy full-record STREAM instead of screen scenes')
    ap.add_argument('--rate',type=int,help='mirror 1..20; full-record 1..5');ap.add_argument('--scale',type=int,choices=(1,2,3,4),default=2)
    ap.add_argument('--frames',type=int,default=0);ap.add_argument('--output',type=pathlib.Path);ap.add_argument('--plot',action='store_true');ap.add_argument('--timeout',type=float,default=10)
    args=ap.parse_args();mirror=args.mirror or (args.stream and not args.full_record);live=args.stream or args.mirror
    rate=args.rate if args.rate is not None else 20 if mirror else 4
    if not 1<=rate<=(20 if mirror else 5):ap.error('Mirror rate must be 1..20; full-record rate 1..5.')
    if args.mirror and args.full_record:ap.error('--mirror and --full-record are mutually exclusive.')
    if args.output is None and not mirror and not (args.decode_file and args.plot):args.output=pathlib.Path('dso138-capture.csv')
    decoder=Decoder();connection=None;output=None;writer=None;csv_mirror=None;received=0;failed=False;viewer=None;raw_plot=None;last_log=0
    want_plot=args.plot or args.mirror
    def consume(frames):
        nonlocal received,writer,csv_mirror,viewer,raw_plot,last_log
        frames=[f for f in frames if not isinstance(f,ControlReply)]
        for f in frames:
            is_mirror=f.kind==3
            if output:
                if writer is None:
                    csv_mirror=is_mirror;writer=csv.writer(output);writer.writerow(MIRROR_CSV_HEADER if is_mirror else CSV_HEADER)
                if csv_mirror!=is_mirror:raise ValueError('Do not mix raw and display-scene CSV formats in one file.')
                writer.writerows(f.rows())
            received+=1
        if output:output.flush()
        if not frames:return
        f=frames[-1];now=time.monotonic()
        if now-last_log>=1 or not live:
            print(f'{received} frames; latest {f.sequence}: {f.count} {"screen columns" if f.kind==3 else "ADC samples" if f.kind==1 else "min/max bins"}; CRC errors={decoder.bad_crc}',file=sys.stderr);last_log=now
        if want_plot:
            if f.kind==3:
                if viewer is None:viewer=MirrorWindow(args.scale)
                viewer.show(f)
            else:
                if raw_plot is None:
                    import matplotlib.pyplot as plt
                    plt.ion();fig,ax=plt.subplots();raw_plot=(plt,fig,ax)
                plt,fig,ax=raw_plot;ax.clear();x=[i*f.interval for i in range(f.count)];low=[f.volts(p[0]) for p in f.samples];high=[f.volts(p[1]) for p in f.samples]
                if f.kind==2:ax.fill_between(x,low,high,color='gold')
                else:ax.plot(x,low,color='gold')
                ax.set(xlabel='Time within record (s)',ylabel='Voltage (V)',title=f'DSO138 record {f.sequence}');ax.grid();plt.pause(.001)
    try:
        if args.output:output=args.output.open('w',newline='',encoding='utf-8')
        if args.decode_file:
            with args.decode_file.open('rb') as source:
                while chunk:=source.read(4096):consume(decoder.feed(chunk))
            if viewer:
                while not viewer.closed:viewer.update();time.sleep(.01)
        else:
            try:import serial
            except ImportError:ap.error('Install pyserial: python -m pip install pyserial')
            connection=serial.Serial(args.port,args.baud,timeout=.01,write_timeout=1);connection.reset_input_buffer()
            command=f'{"MIRROR" if mirror else "STREAM"} {rate}\n' if live else 'VIEW\n' if mirror else 'SNAP\n'
            connection.write(command.encode());limit=args.frames or (0 if live else 1);last_valid=time.monotonic()
            while not limit or received<limit:
                frames=decoder.feed(connection.read(min(8192,max(1,connection.in_waiting))))
                if frames:last_valid=time.monotonic();consume(frames)
                if viewer:
                    viewer.update()
                    if viewer.closed:break
                if raw_plot:raw_plot[0].pause(.001)
                if time.monotonic()-last_valid>args.timeout:raise TimeoutError('No valid capture arrived; check wiring/baud, acquisition mode and firmware version.')
    except KeyboardInterrupt:pass
    except (TimeoutError,OSError,ValueError,ImportError) as error:failed=True;print(str(error),file=sys.stderr)
    finally:
        if connection:
            try:connection.write(b'STOP\n')
            except OSError:pass
            finally:connection.close()
        if output:output.close()
        if viewer and not viewer.closed:viewer.close()
    print(f'{received} frames; {decoder.bad_crc} rejected CRCs; output {args.output or "not recorded"}',file=sys.stderr)
    return 0 if received and not failed else 1
if __name__=='__main__':raise SystemExit(main())
