# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Calibration fitting tests use temporary files and never alter firmware constants."""
import importlib.util, pathlib, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('calibrate', ROOT/'tools/calibrate.py')
calibrate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(calibrate)

class CalibrationTests(unittest.TestCase):
    def fixture(self, rows):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        path = pathlib.Path(directory.name)/'reference.csv'
        path.write_text('range,reference_mV,zero_adc,signal_adc\n'+rows, encoding='utf-8')
        return path

    def test_known_gain(self):
        report = calibrate.fit(self.fixture('1V,5000,2000,2472\n'))
        self.assertEqual(report['1V']['gain_q10'], 1077)

    def test_negative_reference(self):
        report = calibrate.fit(self.fixture('1V,-5000,2500,2028\n'))
        self.assertEqual(report['1V']['gain_q10'], 1077)

    def test_missing_ranges_not_invented(self):
        report = calibrate.fit(self.fixture('1V,5000,2000,2472\n2V,4000,,\n'))
        self.assertEqual(list(report), ['1V'])

    def test_bad_points(self):
        for row in ['1V,5000,2000,4095','1V,5000,2000,2001','1V,5000,2000,1900','1V,nan,2000,2472','bad,5000,2000,2472']:
            with self.subTest(row=row), self.assertRaises(ValueError):
                calibrate.fit(self.fixture(row+'\n'))

    def test_disagreeing_points(self):
        with self.assertRaises(ValueError):
            calibrate.fit(self.fixture('1V,5000,2000,2472\n1V,5000,2000,2300\n'))

    def test_apply_only_measured_range(self):
        path = self.fixture('1V,5000,2000,2472\n')
        header = path.parent/'gain.hpp'
        header.write_text('constexpr unsigned vertical_gain_q10[] = {1024,1024,1024,1024,1024,1024,1024,1024,1024};')
        original = header.read_text()
        calibrate.apply(calibrate.fit(path), header)
        self.assertIn('{1024,1024,1077,1024,1024,1024,1024,1024,1024}', header.read_text())
        self.assertIn('1024,1024,1024', original)

    def test_user_dataset_quality_and_coverage(self):
        import json
        gains = json.loads((ROOT/'reference/calibration-display-gains-v0.13.json').read_text())
        report = calibrate.audit(ROOT/'reference/dso138-kalibrierung_021126-korrigiert.csv', gains)
        self.assertEqual({k:v['gain_q10'] for k,v in report['measured'].items()},
                         {'5V':1065,'2V':1070,'1V':1076,'0.5V':1009,'0.2V':1011})
        self.assertEqual(len(report['accepted']), 7)
        self.assertEqual(len(report['rejected']), 3)
        self.assertEqual(report['missing'], ['0.1V','50mV','20mV','10mV'])

    def test_optional_display_column_is_only_crosscheck(self):
        path = self.fixture('1V,5000,2000,2472\n')
        plain = calibrate.audit(path, dict.fromkeys(calibrate.RANGES, 1078))
        self.assertEqual(plain['measured']['1V']['gain_q10'], 1077)
        path.write_text('range,reference_mV,zero_adc,signal_adc,Rdisp_mV\n1V,5000,2000,2472,100\n')
        bad = calibrate.audit(path, dict.fromkeys(calibrate.RANGES, 1078))
        self.assertEqual(bad['measured'], {})
        self.assertIn('Rdisp_mV', bad['rejected'][0]['reason'])

    def test_upper_analog_headroom_guard(self):
        report = calibrate.audit(self.fixture('0.1V,2000,2075,3870\n'), dict.fromkeys(calibrate.RANGES,1078))
        self.assertEqual(report['measured'], {})
        self.assertIn('headroom', report['rejected'][0]['reason'])

    def test_suspect_range_does_not_block_others(self):
        report = calibrate.audit(self.fixture('1V,5000,2000,2472\n0.1V,2000,2075,3870\n'), dict.fromkeys(calibrate.RANGES,1078))
        self.assertEqual(list(report['measured']), ['1V'])

    def test_inconsistent_range_excluded_as_group(self):
        report = calibrate.audit(self.fixture('1V,5000,2000,2472\n1V,5000,2000,2300\n2V,2000,2075,2170\n'), dict.fromkeys(calibrate.RANGES,1078))
        self.assertEqual(list(report['measured']), ['2V'])
        self.assertEqual(len(report['rejected']),2)

    def test_recorded_zero_offset_distinguishes_display_bias_from_gain(self):
        path = self.fixture('1V,5000,2000,2472\n')
        path.write_text('range,reference_mV,zero_adc,signal_adc,Rdisp_mV,Zoffset_adc\n1V,5000,2000,2472,4793,-28\n')
        report = calibrate.audit(path, dict.fromkeys(calibrate.RANGES,1078))
        self.assertEqual(report['measured']['1V']['gain_q10'],1077)
        self.assertEqual(report['accepted'][0]['display_zero_adc'],2020)

if __name__ == '__main__':
    unittest.main()
