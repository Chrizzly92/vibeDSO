# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
import importlib.util,pathlib,sys,tempfile,struct,unittest,zlib,csv,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('dso_capture',ROOT/'tools/dso138_capture.py'); module=importlib.util.module_from_spec(spec); sys.modules[spec.name]=module; spec.loader.exec_module(module)
def peak_packet():
    p=bytearray(48);p[:4]=b'DSO1';p[4:6]=bytes((1,2));struct.pack_into('<HIIQIHHHBBHHIHH',p,6,48,7,1234,112,56000000,2,20,15,6,0,1024,4,1,8,0)
    p+=struct.pack('<HHHH',10,20,30,40);p+=struct.pack('<I',zlib.crc32(p));return bytes(p)
class CaptureTests(unittest.TestCase):
    def setUp(self): self.fixture=(ROOT/'analysis/link-fixture.bin').read_bytes()
    def test_cpp_fixture(self):
        frames=module.Decoder().feed(self.fixture); self.assertEqual(len(frames),1); f=frames[0]
        self.assertEqual((f.count,f.ticks,f.core_hz),(1024,56,56000000)); self.assertEqual(f.samples[-1],(2023,2023)); self.assertEqual(len(list(f.rows())),1024); self.assertEqual(tuple(f.rows())[0][-4:],(f.zero,f.sensitivity,f.coupling,f.gain))
    def test_fragmented_noise(self):
        d=module.Decoder();frames=[]
        for byte in b'UART startup\r\n'+self.fixture: frames+=d.feed(bytes((byte,)))
        self.assertEqual(len(frames),1);self.assertGreater(d.discarded,0)
    def test_crc_recovery(self):
        bad=bytearray(self.fixture);bad[97]^=1;d=module.Decoder();frames=d.feed(bad+self.fixture)
        self.assertEqual((len(frames),d.bad_crc),(1,1))
    def test_invalid_header(self):
        bad=bytearray(self.fixture);struct.pack_into('<H',bad,44,65535);self.assertEqual(len(module.Decoder().feed(bad+self.fixture)),1)
    def test_peak(self):
        f=module.Decoder().feed(peak_packet())[0];self.assertEqual(f.samples,[(10,20),(30,40)]);self.assertEqual(f.source_count,4);self.assertLess(f.volts(10),0);self.assertGreater(f.volts(20),0)
    def test_csv_record_time(self):
        f=module.Decoder().feed(self.fixture)[0];rows=list(f.rows());self.assertEqual(rows[0][6],0);self.assertAlmostEqual(rows[-1][6],.001023)
    def test_bad_calibration(self):
        bad=bytearray(self.fixture);struct.pack_into('<H',bad,32,5000);struct.pack_into('<I',bad,len(bad)-4,zlib.crc32(bad[:-4]));self.assertEqual(module.Decoder().feed(bad),[])
    def test_bounded_garbage(self):
        d=module.Decoder();d.feed(b'x'*100000);self.assertLessEqual(len(d.buffer),3)
    def test_cli_offline(self):
        with tempfile.TemporaryDirectory() as temp:
            out=pathlib.Path(temp)/'capture.csv';r=subprocess.run([sys.executable,str(ROOT/'tools/dso138_capture.py'),'--decode-file',str(ROOT/'analysis/link-fixture.bin'),'--output',str(out)],capture_output=True,text=True)
            self.assertEqual(r.returncode,0,r.stderr)
            with out.open() as saved: rows=list(csv.reader(saved))
            self.assertEqual(len(rows),1025);self.assertEqual(tuple(rows[0]),module.CSV_HEADER)
if __name__=='__main__': unittest.main()
