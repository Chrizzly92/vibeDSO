# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
import array, json, pathlib, struct, sys, unittest, zlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from dso138_capture import Decoder, MirrorFrame
from dso138_mirror import render_scene,RED

class MirrorTests(unittest.TestCase):
    def test_exact_rasters(self):
        for index in range(16):
            with self.subTest(scene=index):
                frames=Decoder().feed((ROOT/f'analysis/mirror-{index}.bin').read_bytes())
                self.assertEqual(len(frames),1)
                self.assertIsInstance(frames[0],MirrorFrame)
                expected=array.array('H'); expected.frombytes((ROOT/f'analysis/mirror-{index}.lcd').read_bytes())
                self.assertEqual(render_scene(frames[0].scene),expected)
    def test_fragmentation(self):
        data=(ROOT/'analysis/mirror-0.bin').read_bytes(); d=Decoder(); frames=[]
        for byte in data: frames+=d.feed(bytes((byte,)))
        self.assertEqual(len(frames),1)
    def test_crc_recovery(self):
        data=(ROOT/'analysis/mirror-0.bin').read_bytes(); bad=bytearray(data);bad[90]^=1
        d=Decoder();self.assertEqual(len(d.feed(bad+data)),1);self.assertEqual(d.bad_crc,1)
    def test_geometry_rejected(self):
        bad=bytearray((ROOT/'analysis/mirror-0.bin').read_bytes());bad[88]=0
        struct.pack_into('<I',bad,len(bad)-4,zlib.crc32(bad[:-4]))
        self.assertEqual(Decoder().feed(bad),[])
    def test_time_axis(self):
        f=Decoder().feed((ROOT/'analysis/mirror-0.bin').read_bytes())[0]
        self.assertAlmostEqual(f.interval,0.0002/25)
        self.assertEqual(len(list(f.rows())),300)
    def test_marker_size_and_legacy_schema(self):
        data=bytearray((ROOT/'analysis/mirror-9.bin').read_bytes())
        new=render_scene(Decoder().feed(data)[0].scene)
        self.assertEqual(sum(c==RED for c in new),98)
        data[48]=2;struct.pack_into('<I',data,len(data)-4,zlib.crc32(data[:-4]))
        old=render_scene(Decoder().feed(data)[0].scene)
        self.assertEqual(sum(c==RED for c in old),18)

if __name__=='__main__':
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(MirrorTests)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    (ROOT/'analysis/mirror-python-tests.json').write_text(json.dumps({'passed':result.testsRun-len(result.failures)-len(result.errors),'exit_code':0 if result.wasSuccessful() else 1,'scope':'Eleven actual C++ incremental display rasters compared pixel-for-pixel with PC renderer; fragmentation, CRC recovery, geometry and time axis. No physical UART or GUI timing validation.'},indent=2),encoding='utf-8')
    sys.exit(not result.wasSuccessful())
