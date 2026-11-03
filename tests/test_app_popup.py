# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Regression with a real internally created Tk dropdown; no serial port opened."""
import json,pathlib,sys,tkinter as tk
from tkinter import ttk
ROOT=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from dso138_app import Application
root=tk.Tk();app=Application(root);checks=[]
state=dict(timebase=18,mode=0,edge=1,focus=0,sensitivity=6,coupling=0,flags=2,vpos=0,hpos=362,trigger=60,reclen=1024,generator=1000,zero=23,cursor_a=85,cursor_b=235)
try:
    app.apply_state(state);root.update()
    box=next(w for w in app.control_widgets if isinstance(w,ttk.Combobox))
    root.tk.call('ttk::combobox::Post',str(box));root.update()
    pop=root.tk.call('ttk::combobox::PopdownWindow',str(box));root.tk.call('focus','-force',str(pop)+'.f.l');root.update()
    try:root.focus_get()
    except KeyError:checks.append('Actual Tk popdown produces the formerly fatal focus KeyError')
    else:raise AssertionError('Native popdown focus was not reproduced')
    app.apply_state(dict(state,generator=2000,timebase=24));checks.append('State update survives actual native dropdown focus')
    assert app.gen.get()=='2000' and app.tb.get()=='1 ms';checks.append('Readback updates while active dropdown selection stays unchanged')
    root.tk.call('ttk::combobox::Unpost',str(box));root.tk.call('focus','-force','.');root.update()
    app.apply_state(dict(state,timebase=24));assert app.tb.get()=='10 us';checks.append('Normal readback resumes after dropdown closes')
finally:root.destroy()
(ROOT/'analysis/app-popdown-regression-v0.30.1.json').write_text(json.dumps({'passed':len(checks),'exit_code':0,'checks':checks,'scope':'Actual native Tk dropdown and state readback; no serial connection opened.'},indent=2),encoding='utf-8')
print('\n'.join('PASS '+s for s in checks))
