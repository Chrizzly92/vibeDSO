# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""vibeDSO Desktop: standalone Windows frontend for the vibeDSO firmware."""
from __future__ import annotations
import argparse, json, pathlib, sys, time, tkinter as tk
from tkinter import ttk, messagebox, filedialog
from dso138_capture import Decoder, ControlReply, MIRROR_CSV_HEADER
from dso138_mirror import render_scene, ppm
from dso138_control import SerialWorker

TIMEBASES=('500 s','200 s','100 s','50 s','20 s','10 s','5 s','2 s','1 s','500 ms','200 ms','100 ms','50 ms','20 ms','10 ms','5 ms','2 ms','1 ms','500 us','200 us','100 us','50 us','20 us','10 us')
FOCUS=('Timebase','Trigger mode','Edge','Trigger level','Horizontal position','Zero position','Generator','Measurements','Cursor','Debug')
ERRORS={1:'Unknown command',2:'Value out of range',3:'Device busy',4:'Could not save settings'}
APP_VERSION='0.37'

class Application:
    def __init__(self,root):
        self.root=root;self.worker=None;self.retiring=None;self.frame=None;self.state=None;self.remote_ready=False
        self.image=None;self.frames=0;self.fps_start=time.monotonic();self.last_redraw=0;self.control_widgets=[];self.combo_variables=set();self.closing=False
        root.title('vibeDSO Desktop · v'+APP_VERSION);root.minsize(1080,680);root.configure(bg='#111827')
        style=ttk.Style(root);style.theme_use('clam')
        style.configure('.',font=('Segoe UI',10),background='#172235',foreground='#e5edf6')
        style.configure('TFrame',background='#111827');style.configure('Card.TFrame',background='#172235')
        style.configure('TLabel',background='#172235');style.configure('Muted.TLabel',foreground='#9dafc4')
        style.configure('TButton',padding=(10,7),background='#263a52');style.map('TButton',background=[('active','#355673')])
        style.configure('TEntry',fieldbackground='#0d1625',foreground='#e5edf6',insertcolor='#e5edf6')
        style.configure('TCombobox',fieldbackground='#0d1625',foreground='#e5edf6',arrowsize=14)
        style.map('TCombobox',fieldbackground=[('readonly','#0d1625')],foreground=[('readonly','#e5edf6')])
        style.configure('TNotebook',background='#172235',borderwidth=0)
        style.configure('TNotebook.Tab',background='#263a52',foreground='#dce8f5',padding=(9,7))
        style.map('TNotebook.Tab',background=[('selected','#355673')],foreground=[('selected','#ffffff')])
        outer=ttk.Frame(root,padding=16);outer.pack(fill='both',expand=True)
        top=ttk.Frame(outer,style='Card.TFrame',padding=12);top.pack(fill='x',pady=(0,12))
        ttk.Label(top,text='vibeDSO',font=('Segoe UI',17,'bold')).pack(side='left',padx=(0,16))
        self.port=tk.StringVar();self.baud=tk.StringVar(value='230400');self.rate=tk.StringVar(value='20');self.scale=tk.StringVar(value='2')
        ttk.Label(top,text='COM port').pack(side='left',padx=(0,6))
        self.port_box=ttk.Combobox(top,textvariable=self.port,width=25,state='readonly');self.port_box.pack(side='left')
        self.refresh=ttk.Button(top,text='Refresh',command=self.scan_ports);self.refresh.pack(side='left',padx=6)
        ttk.Label(top,text='Baud').pack(side='left',padx=(10,6))
        self.baud_box=ttk.Combobox(top,textvariable=self.baud,values=('38400','115200','230400','460800','921600'),width=9,state='readonly');self.baud_box.pack(side='left')
        self.connect_button=ttk.Button(top,text='Connect',command=self.toggle_connection);self.connect_button.pack(side='left',padx=(12,0))
        self.connection_label=tk.StringVar(value='Disconnected');ttk.Label(top,textvariable=self.connection_label,style='Muted.TLabel').pack(side='right',padx=8)
        body=ttk.Frame(outer);body.pack(fill='both',expand=True)
        left=ttk.Frame(body,style='Card.TFrame',padding=12);left.pack(side='left',fill='both',expand=True,padx=(0,12))
        tools=ttk.Frame(left,style='Card.TFrame');tools.pack(fill='x',pady=(0,10))
        ttk.Label(tools,text='LIVE DISPLAY',font=('Segoe UI',10,'bold')).pack(side='left')
        ttk.Label(tools,text='Max. FPS').pack(side='left',padx=(20,6))
        rate_box=ttk.Combobox(tools,textvariable=self.rate,values=('5','10','15','20'),state='readonly',width=4);rate_box.pack(side='left');rate_box.bind('<<ComboboxSelected>>',self.change_rate)
        ttk.Label(tools,text='Zoom').pack(side='left',padx=(14,6))
        scale_box=ttk.Combobox(tools,textvariable=self.scale,values=('1','2','3'),state='readonly',width=3);scale_box.pack(side='left');scale_box.bind('<<ComboboxSelected>>',lambda _:self.redraw())
        ttk.Button(tools,text='Save image',command=self.save_image).pack(side='right')
        self.raw_image=tk.PhotoImage(width=320,height=240);self.image=tk.PhotoImage(width=640,height=480)
        self.canvas=tk.Label(left,image=self.image,compound='center',text='Select a COM port and connect',fg='#8ea1b8',font=('Segoe UI',14),width=640,height=480,bg='#000000',highlightthickness=1,highlightbackground='#34475d');self.canvas.pack(expand=True)
        self.metrics=tk.StringVar(value='230400 baud · 8N1 · Display mirroring and remote control')
        ttk.Label(left,textvariable=self.metrics,style='Muted.TLabel',wraplength=640).pack(anchor='w',pady=(12,0))
        self.clip_text=tk.StringVar(value='Orange: trace outside display · Red: ADC clipping')
        ttk.Label(left,textvariable=self.clip_text,style='Muted.TLabel').pack(anchor='w',pady=(4,0))
        right_host=ttk.Frame(body,style='Card.TFrame',padding=14);right_host.pack(side='right',fill='y')
        ttk.Label(right_host,text='REMOTE CONTROL',font=('Segoe UI',11,'bold')).pack(anchor='w',pady=(0,12))
        tabs=ttk.Notebook(right_host);tabs.pack(fill='both',expand=True)
        right=ttk.Frame(tabs,style='Card.TFrame',padding=8);extra=ttk.Frame(tabs,style='Card.TFrame',padding=8);settings=ttk.Frame(tabs,style='Card.TFrame',padding=8)
        tabs.add(right,text='Acquisition');tabs.add(extra,text='Cursors / buttons');tabs.add(settings,text='Settings')
        self.tb=tk.StringVar(value=TIMEBASES[17]);self.mode=tk.StringVar(value='AUTO');self.edge=tk.StringVar(value='Rising')
        self.combo(right,1,'Timebase',self.tb,TIMEBASES,lambda:self.send(f'TB {TIMEBASES.index(self.tb.get())+1}'))
        self.combo(right,2,'Trigger',self.mode,('AUTO','NORMAL','SINGLE'),lambda:self.send(f'MODE {("AUTO","NORMAL","SINGLE").index(self.mode.get())}'))
        self.combo(right,3,'Edge',self.edge,('Falling','Rising'),lambda:self.send(f'EDGE {int(self.edge.get()=="Rising")}'))
        self.tl=self.entry(right,4,'Trigger level / ADC','0',lambda:self.numeric('TL',self.tl,-4095,4095))
        self.vp=self.entry(right,5,'Vertical position / px','0',lambda:self.numeric('VP',self.vp,-255,255))
        self.hp=self.entry(right,6,'H position [-1..1]','0.00',self.apply_hp)
        self.gen=self.entry(right,7,'Generator / Hz','1000',lambda:self.numeric('GEN',self.gen,1,100000))
        actions=ttk.Frame(right,style='Card.TFrame');actions.grid(row=8,column=0,columnspan=3,sticky='ew',pady=(10,8))
        for text,body_ in [('RUN','HOLD 0'),('HOLD','HOLD 1'),('Rearm','ARM')]:self.button(actions,text,lambda b=body_:self.send(b)).pack(side='left',padx=(0,5))
        genrow=ttk.Frame(right,style='Card.TFrame');genrow.grid(row=9,column=0,columnspan=3,sticky='ew')
        self.button(genrow,'Generator on',lambda:self.send('GENON 1')).pack(side='left',padx=(0,5));self.button(genrow,'Off',lambda:self.send('GENON 0')).pack(side='left')
        self.selected=tk.StringVar(value=FOCUS[0]);self.combo(extra,0,'Selection',self.selected,FOCUS,lambda:self.send(f'FOCUS {FOCUS.index(self.selected.get())}'))
        keys=ttk.Frame(extra,style='Card.TFrame');keys.grid(row=1,column=0,columnspan=3,sticky='ew',pady=8)
        for text,key in [('SEL',0),('+',1),('−',2),('OK',3),('Long OK',4)]:self.button(keys,text,lambda k=key:self.send(f'KEY {k}')).pack(side='left',padx=(0,3))
        self.cmode=tk.StringVar(value='Time');self.combo(extra,2,'Cursor mode',self.cmode,('Time','Voltage'),lambda:self.send(f'CMODE {int(self.cmode.get()=="Voltage")}'))
        self.ca=self.entry(extra,3,'Cursor A / px','85',lambda:self.apply_cursor('CA',self.ca))
        self.cb=self.entry(extra,4,'Cursor B / px','235',lambda:self.apply_cursor('CB',self.cb))
        ttk.Label(extra,text='Select Cursors to show them on the display.\nOK selects A/B; long OK switches between time and voltage.',wraplength=310,style='Muted.TLabel').grid(row=5,column=0,columnspan=3,sticky='w',pady=12)
        self.device_info=tk.StringVar(value='Set the voltage range and AC/DC/GND coupling on the device.')
        ttk.Label(right_host,textvariable=self.device_info,style='Muted.TLabel',wraplength=315).pack(anchor='w',pady=(12,0))
        ttk.Label(settings,text='Save the timebase, trigger, positions and record length.\nGenerator and cursor settings apply to the current session.\n\nFactory reset preserves zero calibration, range gains and physical switch settings.',wraplength=310,style='Muted.TLabel').grid(row=0,column=0,sticky='w',pady=(4,16))
        self.button(settings,'Save acquisition settings',lambda:self.send('SAVE')).grid(row=1,column=0,sticky='ew',pady=3)
        self.button(settings,'Factory reset…',self.reset_settings).grid(row=2,column=0,sticky='ew',pady=3)
        self.status=tk.StringVar(value='Ready. Match the firmware baud rate; the default is 230400.')
        ttk.Label(outer,textvariable=self.status,background='#111827',wraplength=1100).pack(fill='x',pady=(12,0))
        ttk.Button(outer,text='Licenses and attribution',command=self.show_licenses).pack(anchor='e',pady=(4,0))
        self.set_controls(False);self.scan_ports();root.protocol('WM_DELETE_WINDOW',self.close);root.after(20,self.poll)
    def show_licenses(self):
        base=pathlib.Path(__file__).resolve().parent
        bundled=base/'notices'
        paths=([bundled/'NOTICE',bundled/'LICENSE']+sorted((bundled/'pc-licenses').glob('*.txt'))) if bundled.is_dir() else ([base.parent/'NOTICE',base.parent/'LICENSE']+sorted((base.parent/'docs/pc-licenses').glob('*.txt')))
        window=tk.Toplevel(self.root);window.title('vibeDSO · Licenses and attribution');window.geometry('820x580')
        text=tk.Text(window,wrap='word',padx=12,pady=12)
        scroll=ttk.Scrollbar(window,command=text.yview);scroll.pack(side='right',fill='y')
        text.configure(yscrollcommand=scroll.set);text.pack(fill='both',expand=True)
        for path in paths:
            if path.is_file():text.insert('end',path.name+'\n'+'='*60+'\n'+path.read_text(encoding='utf-8',errors='replace')+'\n\n')
        text.configure(state='disabled')
    def button(self,parent,text,command):
        widget=ttk.Button(parent,text=text,command=command);self.control_widgets.append(widget);return widget
    def combo(self,parent,row,label,var,values,command):
        self.combo_variables.add(str(var))
        ttk.Label(parent,text=label).grid(row=row,column=0,sticky='w',pady=4,padx=(0,8))
        widget=ttk.Combobox(parent,textvariable=var,values=values,state='readonly',width=18);widget.grid(row=row,column=1,columnspan=2,sticky='ew',pady=4)
        widget.bind('<<ComboboxSelected>>',lambda _:command());self.control_widgets.append(widget)
    def entry(self,parent,row,label,value,command):
        ttk.Label(parent,text=label).grid(row=row,column=0,sticky='w',pady=4,padx=(0,8))
        var=tk.StringVar(value=value);entry=ttk.Entry(parent,textvariable=var,width=12);entry.grid(row=row,column=1,sticky='ew',pady=4)
        entry.bind('<Return>',lambda _:command());self.control_widgets.append(entry)
        self.button(parent,'Apply',command).grid(row=row,column=2,padx=(5,0),pady=4)
        return var
    def set_controls(self,enabled):
        self.remote_ready=enabled
        for widget in self.control_widgets:widget.configure(state=('readonly' if isinstance(widget,ttk.Combobox) else 'normal') if enabled else 'disabled')
    def scan_ports(self):
        try:
            from serial.tools import list_ports
            self.ports={f'{p.device} · {p.description}':p.device for p in list_ports.comports()}
            self.port_box['values']=tuple(self.ports)
            if self.port.get() not in self.ports:self.port.set(next(iter(self.ports),''))
        except (OSError,ImportError) as error:self.status.set(str(error))
    def toggle_connection(self):
        if self.worker:self.disconnect();return
        if self.retiring and self.retiring.is_alive():self.status.set('The previous connection is still closing.');return
        port=self.ports.get(self.port.get())
        if not port:self.status.set('Please select a COM port.');return
        self.set_controls(False);self.state=None
        self.worker=SerialWorker(port,int(self.baud.get()),int(self.rate.get()));self.worker.start()
        self.connect_button.configure(text='Disconnect');self.port_box.configure(state='disabled');self.baud_box.configure(state='disabled');self.refresh.configure(state='disabled')
        self.connection_label.set('Connecting…');self.status.set('Waiting for display data and confirmed device settings…')
    def disconnect(self):
        if self.worker:self.worker.stop();self.retiring=self.worker;self.worker=None
        self.set_controls(False);self.connect_button.configure(text='Connect');self.port_box.configure(state='readonly');self.baud_box.configure(state='readonly');self.refresh.configure(state='normal');self.connection_label.set('Disconnected')
    def send(self,body):
        if not self.worker or not self.remote_ready:return
        try:self.worker.send(body);self.status.set(f'Sending: {body}')
        except ValueError as error:self.status.set(str(error))
    def numeric(self,name,var,lo,hi):
        try:
            value=int(var.get())
            if not lo<=value<=hi:raise ValueError()
        except ValueError:self.status.set(f'Allowed range: {lo} to {hi}.');return
        self.send(f'{name} {value}')
    def apply_hp(self):
        try:
            value=float(self.hp.get().replace(',','.'))
            if not -1<=value<=1:raise ValueError()
        except ValueError:self.status.set('Horizontal position must be between -1.00 and +1.00.');return
        self.send(f'HP {round(value*100)}')
    def apply_cursor(self,name,var):
        voltage=bool(self.state and self.state['flags']&8)
        self.numeric(name,var,17 if voltage else 10,216 if voltage else 309)
    def change_rate(self,_=None):
        if self.worker:
            try:self.worker.send(f'MIRROR {self.rate.get()}')
            except ValueError as error:self.status.set(str(error))
    def reset_settings(self):
        if messagebox.askyesno('Factory reset','Reset acquisition and display settings?\n\nCalibration is preserved. The generator restarts at 1 kHz.',parent=self.root):self.send('RESET SETTINGS')
    def apply_state(self,state):
        self.state=state;self.set_controls(True)
        popup_focus=False
        try:focus=self.root.focus_get()
        except (KeyError,tk.TclError):
            # Tk creates the dropdown's listbox internally; it has no Python Widget.
            focus=None;popup_focus=True
        def set_value(var,value):
            if popup_focus and str(var) in self.combo_variables:return
            if not isinstance(focus,ttk.Entry) or str(focus.cget('textvariable'))!=str(var):var.set(value)
        set_value(self.tb,TIMEBASES[state['timebase']-1]);set_value(self.mode,('AUTO','NORMAL','SINGLE')[state['mode']]);set_value(self.edge,('Falling','Rising')[state['edge']])
        set_value(self.tl,str(state['trigger']));set_value(self.vp,str(state['vpos']));set_value(self.hp,f'{2*state["hpos"]/(state["reclen"]-300)-1:.2f}');set_value(self.gen,str(state['generator']))
        set_value(self.selected,FOCUS[state['focus']]);set_value(self.cmode,'Voltage' if state['flags']&8 else 'Time');set_value(self.ca,str(state['cursor_a']));set_value(self.cb,str(state['cursor_b']))
        volts=('5','2','1','0.5','0.2','0.1','0.05','0.02','0.01')[state['sensitivity']-4]
        self.device_info.set(f'{volts} V/div · {("DC","AC","GND")[state["coupling"]]} · {"HOLD" if state["flags"]&1 else "RUN"}\nGenerator {"on" if state["flags"]&2 else "off"} · Zero calibration={state["zero"]}')
    def show_frame(self,frame):
        self.frame=frame;self.frames+=1;self.redraw()
        elapsed=time.monotonic()-self.fps_start
        if elapsed>=1:
            self.metrics.set(f'{self.frames/elapsed:.1f} received FPS · Revision {frame.sequence} · Static display: 1 frame/s');self.frames=0;self.fps_start=time.monotonic()
        flags=frame.scene.get('clip_flags',0)
        self.clip_text.set('ADC clipping: measurements are invalid' if flags&12 else 'Signal outside the visible grid' if flags&3 else 'Signal within the visible grid')
    def redraw(self):
        if not self.frame:return
        self.last_redraw=time.monotonic()
        scale=int(self.scale.get())
        self.raw_image.configure(data=ppm(render_scene(self.frame.scene)),format='PPM')
        self.image.configure(width=320*scale,height=240*scale)
        self.image.tk.call(self.image.name,'copy',self.raw_image.name,'-zoom',scale,scale)
        self.canvas.configure(text='',width=320*scale,height=240*scale)
    def save_image(self):
        if not self.frame:self.status.set('No display frame has been received yet.');return
        path=filedialog.asksaveasfilename(parent=self.root,defaultextension='.png',filetypes=[('PNG image','*.png')],initialfile='vibeDSO.png')
        if path:
            try:tk.PhotoImage(data=ppm(render_scene(self.frame.scene)),format='PPM').write(path,format='png');self.status.set(f'Image saved: {path}')
            except (OSError,tk.TclError) as error:self.status.set(str(error))
    def poll(self):
        if self.closing:return
        # Schedule first: one unexpected callback failure must not kill the loop.
        self.root.after(20,self.poll)
        worker=self.worker
        if worker:
            for _ in range(64):
                try:kind,value=worker.events.get_nowait()
                except __import__('queue').Empty:break
                if kind=='connected':self.connection_label.set(value)
                elif kind=='error':self.status.set(value)
                elif kind=='reply':
                    first=not self.remote_ready;body,reply=value;self.apply_state(reply.state)
                    if reply.result:self.status.set(f'{body}: {ERRORS.get(reply.result,"Error")}')
                    elif body!='GET':self.status.set(f'Confirmed: {body}')
                    elif first:self.status.set('Connected. Remote control is ready; changes are acknowledged.')
                    elif not self.frame:self.status.set('Remote control is ready; waiting for a display frame.')
                elif kind=='disconnected':self.disconnect();break
            frame=worker.take_frame()
            if frame:self.show_frame(frame)
    def close(self):
        self.closing=True;self.disconnect()
        deadline=time.monotonic()+1
        def finish():
            if self.retiring and self.retiring.is_alive() and time.monotonic()<deadline:self.root.after(20,finish)
            else:self.root.destroy()
        finish()

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--demo',type=pathlib.Path,nargs='?',const=pathlib.Path(__file__).parent/'demo/mirror-3.bin');parser.add_argument('--self-test',type=pathlib.Path);args=parser.parse_args()
    if args.self_test and args.demo is None:args.demo=pathlib.Path(__file__).parent/'demo/mirror-3.bin'
    root=tk.Tk();app=Application(root)
    if args.demo:
        frames=Decoder().feed(args.demo.read_bytes());app.show_frame(next(f for f in frames if getattr(f,'kind',0)==3));app.status.set('Demo: recorded test scene; no hardware connection.')
    if args.self_test:
        root.withdraw();root.update();checks=[]
        def check(ok,label):
            if not ok:raise AssertionError(label)
            checks.append(label)
        check(app.baud.get()=='230400','default baud');check(not app.remote_ready,'controls disabled before handshake')
        check(app.image is not None,'bundled font and Tk rasterizer load')
        check(app.frame.count==300,'scene decoder renders 300 columns')
        for _ in range(10):app.redraw();root.update_idletasks()
        check(app.image.get(60,100)==(255,255,0),'persistent scaled bitmap keeps waveform through repeated redraws')
        state=dict(timebase=18,mode=0,edge=1,focus=6,sensitivity=6,coupling=0,flags=2,vpos=-20,hpos=362,trigger=123,reclen=1024,generator=1234,zero=23,cursor_a=85,cursor_b=235)
        app.apply_state(state);root.update();check(app.remote_ready and app.gen.get()=='1234','confirmed state populates controls')
        original_focus=root.focus_get
        def internal_focus():raise KeyError('popdown')
        root.focus_get=internal_focus
        changed=dict(state,generator=4321,timebase=24);app.apply_state(changed)
        check(app.gen.get()=='4321' and app.tb.get()==TIMEBASES[17],'internal dropdown focus cannot stop readback or overwrite active dropdown')
        root.focus_get=original_focus;app.apply_state(state)
        class FakeWorker:
            sent=[]
            def send(self,body):self.sent.append(body)
            def stop(self):pass
            def is_alive(self):return False
        fake=FakeWorker();app.worker=fake;app.gen.set('2000');app.numeric('GEN',app.gen,1,100000);check(fake.sent==['GEN 2000'],'generator command wired')
        app.gen.set('0');app.numeric('GEN',app.gen,1,100000);check(len(fake.sent)==1,'invalid generator rejected')
        app.hp.set('-0.50');app.apply_hp();check(fake.sent[-1]=='HP -50','normalized HP command')
        def select(var,value):
            var.set(value)
            widget=next(w for w in app.control_widgets if isinstance(w,ttk.Combobox) and str(w.cget('textvariable'))==str(var))
            widget.event_generate('<<ComboboxSelected>>')
        fake.sent.clear();select(app.edge,'Falling');select(app.edge,'Rising')
        check(fake.sent==['EDGE 0','EDGE 1'],'English edge choices preserve wire command mapping')
        fake.sent.clear();select(app.cmode,'Voltage');select(app.cmode,'Time')
        check(fake.sent==['CMODE 1','CMODE 0'],'English cursor choices preserve wire command mapping')
        fake.sent.clear();select(app.selected,'Debug')
        check(fake.sent==['FOCUS 9'],'English Debug selection preserves focus mapping')
        original_after=root.after;scheduled=[];root.after=lambda ms,callback:scheduled.append((ms,callback))
        fake.events=__import__('queue').Queue()
        def broken_frame():raise ValueError('injected display error')
        fake.take_frame=broken_frame
        try:app.poll()
        except ValueError:pass
        finally:root.after=original_after
        check(len(scheduled)==1 and scheduled[0][1]==app.poll,'next GUI poll survives an unexpected event-processing error')
        app.show_licenses()
        license_window=next(w for w in root.winfo_children() if isinstance(w,tk.Toplevel))
        license_text=next(w for w in license_window.winfo_children() if isinstance(w,tk.Text)).get('1.0','end')
        check('https://github.com/Chrizzly92/vibeDSO' in license_text,'original project attribution accessible in application')
        check('Version 2.0, January 2004' in license_text,'full Apache license accessible in application')
        check(all(name in license_text for name in ('Python-3.12.txt','Tcl-Tk.txt','pyserial-3.5.txt','PyInstaller.txt','JYE-font.txt')),'bundled desktop dependency notices accessible in application')
        license_window.destroy()
        path=args.self_test.with_suffix('.png');tk.PhotoImage(data=ppm(render_scene(app.frame.scene)),format='PPM').write(str(path),format='png');check(path.exists(),'PNG export without extra dependencies')
        args.self_test.write_text(json.dumps({'passed':len(checks),'exit_code':0,'checks':checks,'scope':'Actual Tk application and frozen executable smoke test using native C++ scene; no physical COM transfer.'},indent=2),encoding='utf-8');app.close()
    root.mainloop()
if __name__=='__main__':main()
