# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
import dataclasses,json,pathlib,queue,struct,sys,threading,time,types,unittest,zlib
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from dso138_capture import Decoder,ControlReply
from dso138_control import ControlQueue,SerialWorker

class ControlTests(unittest.TestCase):
    def fixture(self,request=65535):
        data=bytearray((ROOT/'analysis/control-fixture.bin').read_bytes());struct.pack_into('<I',data,8,request);struct.pack_into('<H',data,50,request);struct.pack_into('<I',data,80,zlib.crc32(data[:80]));return bytes(data)
    def test_actual_cpp_reply(self):
        reply=Decoder().feed(self.fixture())[0];self.assertIsInstance(reply,ControlReply);self.assertEqual(reply.request,65535);self.assertEqual(reply.state['zero'],23);self.assertEqual(reply.state['generator'],1000)
    def test_interleaved_scenes_replies(self):
        data=(ROOT/'analysis/mirror-9.bin').read_bytes()+self.fixture()+ (ROOT/'analysis/mirror-10.bin').read_bytes();d=Decoder();frames=[]
        for at in range(0,len(data),7):frames+=d.feed(data[at:at+7])
        self.assertEqual(len(frames),3);self.assertIsInstance(frames[1],ControlReply);self.assertEqual(frames[0].scene['clip_flags'],15);self.assertEqual(frames[2].scene['clip_flags'],0)
    def test_reply_crc_recovery(self):
        bad=bytearray(self.fixture());bad[55]^=1;d=Decoder();self.assertEqual(len(d.feed(bad+self.fixture())),1);self.assertEqual(d.bad_crc,1)
    def test_queue_one_in_flight(self):
        q=ControlQueue();q.enqueue('GET');q.enqueue('GEN 2000');self.assertEqual(q.next(0),b'@1 GET\n');self.assertIsNone(q.next(1))
        reply=Decoder().feed(self.fixture(2))[0];self.assertIsNone(q.accept(reply));self.assertIsNotNone(q.pending)
        reply.request=1;self.assertEqual(q.accept(reply),'GET');self.assertEqual(q.next(1),b'@2 GEN 2000\n')
    def test_timeout_without_replay(self):
        q=ControlQueue();q.enqueue('RESET SETTINGS');q.next(0);self.assertIsNone(q.expired(3.99));self.assertEqual(q.expired(4),'RESET SETTINGS');self.assertIsNone(q.next(5))
    def test_queue_limits(self):
        q=ControlQueue()
        for _ in range(32):q.enqueue('GET')
        for body in ('GET','GET\nRESET SETTINGS','a'*31,''):
            with self.assertRaises(ValueError):q.enqueue(body)
    def test_request_wrap(self):
        q=ControlQueue();q.counter=65535;q.enqueue('GET');self.assertEqual(q.next(0),b'@1 GET\n')
    def test_serial_worker(self):
        owner=self
        class FakeSerial:
            def __init__(self,*args,**kw):self.data=bytearray();self.closed=False;self.writes=[];self.lock=threading.Lock()
            def reset_input_buffer(self):pass
            @property
            def in_waiting(self):return len(self.data)
            def write(self,data):
                self.writes.append(data)
                with self.lock:
                    if data.startswith(b'MIRROR'):self.data.extend((ROOT/'analysis/mirror-3.bin').read_bytes())
                    elif data.startswith(b'@'):self.data.extend(owner.fixture(int(data[1:].split(b' ')[0])))
                return len(data)
            def read(self,count):
                time.sleep(.001)
                with self.lock:result=bytes(self.data[:count]);del self.data[:count];return result
            def close(self):self.closed=True
        channel=FakeSerial()
        with patch.dict(sys.modules,{'serial':types.SimpleNamespace(Serial=lambda *a,**k:channel)}):
            worker=SerialWorker('COM-TEST',230400);worker.start();events=[];deadline=time.monotonic()+2
            try:
                while time.monotonic()<deadline:
                    try:events.append(worker.events.get(timeout=.02))
                    except queue.Empty:pass
                    if any(k=='reply' for k,v in events) and worker.latest is not None:break
                self.assertTrue(any(k=='reply' and v[0]=='GET' for k,v in events));self.assertEqual(worker.take_frame().kind,3)
                worker.send('GEN 2000');deadline=time.monotonic()+2
                while time.monotonic()<deadline and not any(b'GEN 2000' in data for data in channel.writes):time.sleep(.005)
                self.assertTrue(any(b'GEN 2000' in data for data in channel.writes))
            finally:worker.stop();worker.join(1)
        self.assertFalse(worker.is_alive());self.assertTrue(channel.closed);self.assertEqual(channel.writes[-1],b'STOP\n')
    def test_port_failure(self):
        def fail(*a,**k):raise OSError('port unavailable')
        with patch.dict(sys.modules,{'serial':types.SimpleNamespace(Serial=fail)}):
            worker=SerialWorker('MISSING',230400);worker.start();worker.join(1)
        events=[]
        while not worker.events.empty():events.append(worker.events.get())
        self.assertTrue(any(k=='error' and 'unavailable' in v for k,v in events));self.assertEqual(events[-1][0],'disconnected')

if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(ControlTests))
    (ROOT/'analysis/control-python-tests.json').write_text(json.dumps({'passed':result.testsRun-len(result.errors)-len(result.failures),'exit_code':0 if result.wasSuccessful() else 1,'scope':'Actual C++ replies, interleaved display/control decoding, bounded queue and actual threaded serial worker against a fake channel; no physical COM validation.'},indent=2),encoding='utf-8')
    sys.exit(not result.wasSuccessful())
