# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Bounded request/ack queue and serial worker; no GUI work on the serial thread."""
from __future__ import annotations
import collections, queue, threading, time
from dso138_capture import Decoder, ControlReply

class ControlQueue:
    def __init__(self):
        self.waiting=collections.deque();self.pending=None;self.counter=0
    def enqueue(self,body):
        if not body or len(body)>30 or any(ord(c)<32 or ord(c)>126 for c in body):raise ValueError('Invalid command')
        if len(self.waiting)>=32:raise ValueError('Command queue is full')
        self.waiting.append(body)
    def next(self,now):
        if self.pending or not self.waiting:return None
        self.counter=self.counter%65535+1;body=self.waiting.popleft()
        self.pending=(self.counter,body,now+4)
        return f'@{self.counter} {body}\n'.encode('ascii')
    def accept(self,reply):
        if self.pending and reply.request==self.pending[0]:
            body=self.pending[1];self.pending=None;return body
        return None
    def expired(self,now):
        if self.pending and now>=self.pending[2]:
            body=self.pending[1];self.pending=None;return body
        return None

class SerialWorker(threading.Thread):
    def __init__(self,port,baud,rate=20):
        super().__init__(daemon=True);self.port=port;self.baud=baud;self.rate=rate
        self.commands=queue.Queue(maxsize=32);self.events=queue.Queue(maxsize=64)
        self.latest=None;self.lock=threading.Lock();self.stop_event=threading.Event()
    def emit(self,kind,value):
        try:self.events.put_nowait((kind,value))
        except queue.Full:pass
    def send(self,body):
        try:self.commands.put_nowait(body)
        except queue.Full:raise ValueError('Command queue is full')
    def take_frame(self):
        with self.lock:frame=self.latest;self.latest=None
        return frame
    def stop(self):self.stop_event.set()
    def run(self):
        connection=None
        try:
            import serial
            connection=serial.Serial(self.port,self.baud,timeout=.01,write_timeout=.3)
            connection.reset_input_buffer();connection.write(f'MIRROR {self.rate}\n'.encode())
            decoder=Decoder();controls=ControlQueue();controls.enqueue('GET');next_sync=time.monotonic()+1
            self.emit('connected',self.port)
            while not self.stop_event.is_set():
                now=time.monotonic()
                for _ in range(8):
                    try:body=self.commands.get_nowait()
                    except queue.Empty:break
                    if body.startswith('MIRROR '):connection.write((body+'\n').encode('ascii'))
                    else:controls.enqueue(body)
                expired=controls.expired(now)
                if expired:self.emit('error',f'No acknowledgement for {expired}. Firmware v0.30 or later is required; check the port and connection.')
                if now>=next_sync and not controls.pending and not controls.waiting:
                    controls.enqueue('GET');next_sync=now+1
                packet=controls.next(now)
                if packet:connection.write(packet)
                frames=decoder.feed(connection.read(min(8192,max(1,connection.in_waiting))))
                for frame in frames:
                    if isinstance(frame,ControlReply):
                        body=controls.accept(frame)
                        if body is not None:self.emit('reply',(body,frame))
                    elif frame.kind==3:
                        with self.lock:self.latest=frame
                if frames:self.emit('stats',decoder.bad_crc)
        except (OSError,ValueError,ImportError) as error:self.emit('error',str(error))
        finally:
            if connection:
                try:connection.write(b'STOP\n')
                except OSError:pass
                connection.close()
            self.emit('disconnected',None)
