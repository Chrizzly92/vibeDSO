# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Compact DSO display scenes: decoder, exact bitmap-font rasterizer and Tk viewer."""
from __future__ import annotations
import array, pathlib, struct, time

def rgb(r,g,b): return (r>>3)|((g>>2)<<5)|((b>>3)<<11)
BLACK=0; WHITE=rgb(255,255,255); GRID=rgb(105,105,105)
GREEN=rgb(0,255,0); PINK=rgb(255,105,180); YELLOW=rgb(255,255,0)
NAVY=rgb(0,0,128); AQUA=rgb(0,255,255); ORANGE=rgb(255,165,0); BLUE=rgb(0,191,255); RED=rgb(255,0,0)

def parse_scene(payload):
    if len(payload)<640 or payload[0] not in (1,2,3,4): raise ValueError('Unknown display scene schema')
    focus,edge,generator,measurements,cursors,status,hp_selected=payload[1:8]
    zy,ty,left,right=struct.unpack_from('<hhhh',payload,8)
    cursor_flags,selected=payload[16:18]; x0,x1,y0,y1=struct.unpack_from('<HHHH',payload,18)
    count=payload[26]; ref_color,power_color=struct.unpack_from('<HH',payload,28)
    if edge>3 or generator>3 or measurements>1 or cursors>1 or status>4 or hp_selected>1 or count>9 or selected>1 or cursor_flags>3:
        raise ValueError('Invalid display state')
    if not (0<=left<=right<320 and all(10<=x<=309 for x in (x0,x1)) and all(17<=y<=216 for y in (y0,y1))):
        raise ValueError('Invalid display geometry')
    power=payload[32:39].decode('ascii'); spans=list(zip(payload[40:640:2],payload[41:640:2]))
    if any(not 17<=a<=b<=216 for a,b in spans): raise ValueError('Invalid display spans')
    texts=[]; at=640
    for _ in range(count):
        if at+8>len(payload): raise ValueError('Truncated text')
        x,y,width,color,active,length=struct.unpack_from('<HBBHBB',payload,at); at+=8
        if length>10 or y>224 or not 1<=width<=80 or x+width>320 or active>1 or length*8>width or at+length>len(payload):
            raise ValueError('Invalid text bounds')
        value=payload[at:at+length].decode('ascii'); at+=length
        if any(not 32<=ord(c)<127 for c in value): raise ValueError('Invalid display character')
        texts.append((x,y,width,color,active,value))
    if at!=len(payload): raise ValueError('Unexpected display scene bytes')
    if any(not 32<=ord(c)<127 for c in power): raise ValueError('Invalid supply text')
    return dict(focus=focus,edge=edge,generator=generator,measurements=measurements,cursors=cursors,status=status,hp_selected=hp_selected,
                schema=payload[0],clip_flags=payload[39] if payload[0]>=2 else 0,zero_y=zy,trigger_y=ty,left=left,right=right,cursor_flags=cursor_flags,selected=selected,
                cx=(x0,x1),cy=(y0,y1),reference=ref_color,power_color=power_color,power=power,spans=spans,texts=texts)

class Surface:
    def __init__(self): self.pixels=array.array('H',[0])*(320*240)
    def pixel(self,x,y,color):
        x+=1
        if 0<=x<320 and 0<=y<240: self.pixels[y*320+x]=color
    def rect(self,x,y,w,h,color):
        x+=1; right=min(320,x+w); bottom=min(240,y+h); x=max(0,x); y=max(0,y)
        if right<=x or bottom<=y: return
        row=array.array('H',[color])*(right-x)
        for py in range(y,bottom): self.pixels[py*320+x:py*320+right]=row
    def glyph(self,x,y,char,fg,bg,font):
        self.rect(x,y,8,16,bg); start=(ord(char)-32)*16
        if start<0 or start+16>len(font): return
        for row in range(1,16):
            bits=font[start+row-1]
            for col in range(8):
                if bits&(0x80>>col): self.pixel(x+col,y+row,fg)

_FONT=None
_BASE=None

def render_scene(scene):
    global _FONT,_BASE
    if _FONT is None: _FONT=pathlib.Path(__file__).with_name('dso138_font.bin').read_bytes()
    if _BASE is None:
        base=Surface()
        for x in range(10,310):
            for y in range(17,217):
                if (x-10)%25==0 or (y-17)%25==0 or x==309 or y==216: base.pixel(x,y,GRID)
        _BASE=base.pixels
    s=scene; out=Surface();out.pixels=_BASE[:]
    out.rect(160,17,1,200,s['reference'])
    for i,(top,bottom) in enumerate(s['spans']): out.rect(10+i,top,1,bottom-top+1,YELLOW)
    # Margin selection boxes precede debug levels, as in the device renderer.
    for right,raw,focus in ((False,s['zero_y'],5),(True,s['trigger_y'],3)):
        cy=max(17,min(216,raw)); width=9 if right else 10; x0=310 if right else 0
        if s['focus']==focus:
            for x in range(width):
                for y in range(max(17,cy-4),min(216,cy+4)+1):
                    color=AQUA if y in (cy-4,cy+4) or x in (0,width-1) else NAVY
                    out.pixel(x0+x,y,color)
    zy,ty=s['zero_y'],s['trigger_y']
    if 17<=zy<=216: out.rect(0,zy,320,1,GREEN)
    if 17<=ty<=216:
        if ty==zy: out.rect(160,ty,160,1,PINK)
        else: out.rect(0,ty,320,1,PINK)
    if s['cursor_flags']&1:
        for i in range(2):
            c=ORANGE if i==s['selected'] else BLUE
            if s['cursor_flags']&2: out.rect(10,s['cy'][i],300,1,c)
            else: out.rect(s['cx'][i],17,1,200,c)
    for right,raw,focus in ((False,zy,5),(True,ty,3)):
        cy=max(17,min(216,raw)); width=9 if right else 10; x0=310 if right else 0
        color=AQUA if s['focus']==focus else PINK if right else GREEN
        for local in range(width):
            radius=(local//2 if local<=6 else -1) if right else ((9-local)//2 if local>=3 else -1)
            if radius>=0:
                for y in range(max(17,cy-radius),min(216,cy+radius)+1): out.pixel(x0+local,y,color)
    for side in range(2):
        if s.get('clip_flags',0)&(1<<side):
            color=RED if s['clip_flags']&(4<<side) else ORANGE
            center,height=(301,7) if s.get('schema',2)>=3 else (305,3)
            for row in range(height): out.rect(center-row,216-row if side else 17+row,1+row*2,1,color)
    bar=AQUA if s['hp_selected'] else GRID; arrow=WHITE if s['hp_selected'] else GRID
    out.rect(s['left'],219,s['right']-s['left']+1,1,bar)
    out.rect(s['left'],218,1,3,bar);out.rect(s['right'],218,1,3,bar)
    out.rect(160,217,1,1,arrow);out.rect(159,218,3,1,arrow);out.rect(158,219,5,1,arrow);out.rect(160,220,1,2,arrow)
    for x,y,width,color,active,value in s['texts']:
        bg=NAVY if active else BLACK;out.rect(x,y,width,16,bg);start=x+(width-len(value)*8)//2
        for i,c in enumerate(value): out.glyph(start+i*8,y,c,color,bg,_FONT)
        if active:
            out.rect(x,y,width,1,AQUA);out.rect(x,y+15,width,1,AQUA);out.rect(x,y,1,16,AQUA);out.rect(x+width-1,y,1,16,AQUA)
    # Timebase symbol.
    out.rect(72,7,8,1,GREEN);out.rect(72,6,1,3,GREEN);out.rect(79,6,1,3,GREEN)
    for x in (73,78): out.pixel(x,5,GREEN);out.pixel(x,9,GREEN)
    edge_selected=bool(s['edge']&2)
    for x in range(10):
        for y in range(16):
            before,after=(11,4) if s['edge']&1 else (4,11)
            stroke=(2<=x<5 and y==before) or (x==5 and 4<=y<=11) or (5<=x<=7 and y==after)
            border=edge_selected and (x in (0,9) or y in (0,15))
            out.pixel(162+x,y,AQUA if border else PINK if stroke else NAVY if edge_selected else BLACK)
    enabled=bool(s['generator']&1);selected=bool(s['generator']&2)
    for x in range(16):
        for y in range(16):
            stroke=(y==11 and (2<=x<=5 or 10<=x<=13)) or (y==4 and 5<=x<=10) or (x in (5,10) and 4<=y<=11)
            border=selected and (x in (0,15) or y in (0,15))
            out.pixel(174+x,y,AQUA if border else (GREEN if enabled else GRID) if stroke else NAVY if selected else BLACK)
    for x,c,active in ((192,'M',s['measurements']),(208,'C',s['cursors'])):
        bg=NAVY if active else BLACK;out.rect(x,0,16,16,bg);out.glyph(x+4,0,c,WHITE,bg,_FONT)
        if active:
            out.rect(x,0,16,1,AQUA);out.rect(x,15,16,1,AQUA);out.rect(x,0,1,16,AQUA);out.rect(x+15,0,1,16,AQUA)
    state=s['status'];label=('AUTO',"T'D",'HOLD','OVR','WAIT')[state]
    bg=RED if state in (1,3) else GREEN if state==0 else ORANGE if state==2 else NAVY
    out.rect(224,0,32,16,bg)
    for i,c in enumerate(label): out.glyph(224+(32-len(label)*8)//2+i*8,0,c,BLACK if state==0 else WHITE,bg,_FONT)
    if s.get('schema',2)>=4:
        active=s['focus']==9;fault=s['power_color']==RED;bg=NAVY if active else BLACK
        out.rect(256,0,56,16,bg);label='PWR!' if fault else 'D'
        for i,c in enumerate(label):out.glyph(256+(56-len(label)*8)//2+i*8,0,c,RED if fault else WHITE,bg,_FONT)
        if active:
            out.rect(256,0,56,1,AQUA);out.rect(256,15,56,1,AQUA);out.rect(256,0,1,16,AQUA);out.rect(311,0,1,16,AQUA)
    else:
        for i,c in enumerate(s['power']): out.glyph(256+i*8,0,c,s['power_color'],BLACK,_FONT)
    return out.pixels

_RGB=None

def ppm(pixels):
    global _RGB
    if _RGB is None:
        _RGB=tuple(bytes(((c&31)*255//31,((c>>5)&63)*255//63,((c>>11)&31)*255//31)) for c in range(65536))
    return b'P6\n320 240\n255\n'+b''.join(_RGB[c] for c in pixels)

class MirrorWindow:
    def __init__(self,scale=2):
        import tkinter as tk
        self.tk=tk;self.root=tk.Tk();self.root.title('DSO138 mirror');self.closed=False;self.scale=scale
        self.canvas=tk.Canvas(self.root,width=320*scale,height=240*scale,highlightthickness=0,bg='black');self.canvas.pack()
        self.item=self.canvas.create_image(0,0,anchor='nw');self.image=None
        self.root.protocol('WM_DELETE_WINDOW',self.close);self.frames=0;self.last=time.monotonic()
    def close(self): self.closed=True;self.root.destroy()
    def update(self):
        if not self.closed: self.root.update_idletasks();self.root.update()
    def show(self,frame):
        if self.closed:return
        image=self.tk.PhotoImage(data=ppm(render_scene(frame.scene)),format='PPM')
        self.image=image.zoom(self.scale,self.scale);self.canvas.itemconfigure(self.item,image=self.image)
        self.frames+=1;now=time.monotonic()
        if now-self.last>=1:
            self.root.title(f'DSO138 mirror | {self.frames/(now-self.last):.1f} received frames/s | revision {frame.sequence}')
            self.frames=0;self.last=now
        self.update()
