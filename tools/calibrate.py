# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Fit per-range gain from externally measured DC references; never guesses missing data."""
import argparse, csv, json, math, pathlib, re

ROOT = pathlib.Path(__file__).resolve().parents[1]
RANGES = dict(zip(['5V','2V','1V','0.5V','0.2V','0.1V','50mV','20mV','10mV'],
                  [5000,2000,1000,500,200,100,50,20,10]))  # mV/div

def fit(path):
    with pathlib.Path(path).open(encoding='utf-8-sig', newline='') as stream:
        return fit_rows(list(csv.DictReader(stream)))

def fit_rows(rows):
    points = {name: [] for name in RANGES}
    for row in rows:
        if not row.get('signal_adc', '').strip():
            continue
        name = row['range'].strip()
        if name not in RANGES:
            raise ValueError('Unknown range: ' + name)
        reference, zero, signal = (float(row[key]) for key in ('reference_mV','zero_adc','signal_adc'))
        if not all(math.isfinite(v) for v in (reference, zero, signal)):
            raise ValueError('Non-finite calibration data')
        delta = signal-zero
        if not (4 < zero < 4091 and 4 < signal < 4091):
            raise ValueError(name + ': ADC close to a rail; reduce reference voltage')
        if abs(delta) < 32 or reference*delta <= 0:
            raise ValueError(name + ': insufficient or oppositely signed reference swing')
        nominal_mv = delta*RANGES[name]*275/(1092*25)
        points[name].append((nominal_mv, reference))
    report = {}
    for name, values in points.items():
        if not values:
            continue
        gain = sum(x*y for x,y in values)/sum(x*x for x,y in values)
        if not math.isfinite(gain):
            raise ValueError(name + ': reference magnitude is not usable')
        q10 = int(gain*1024+0.5)
        if not 512 <= q10 <= 2048:
            raise ValueError(name + ': gain outside 0.5..2; check range, reference and hardware profile')
        residual = max(abs(x*q10/1024-y)/abs(y) for x,y in values)
        if residual > 0.03:
            raise ValueError(name + ': reference points disagree by more than 3%; check zero and linearity')
        report[name] = dict(points=len(values), gain_q10=q10, gain=q10/1024,
                            max_relative_residual=residual)
    if not report:
        raise ValueError('No completed reference measurements; fill signal_adc and zero_adc first')
    return report

def read_gains(header):
    text = pathlib.Path(header).read_text(encoding='utf-8')
    match = re.search(r'vertical_gain_q10\[\]\s*=\s*\{([^}]+)\}', text)
    if not match:
        raise ValueError('Calibration table not found')
    values = [int(v.strip()) for v in match[1].split(',')]
    if len(values) != len(RANGES):
        raise ValueError('Unexpected calibration table length')
    return dict(zip(RANGES, values))

def audit(path, gains):
    # A conservative fitting boundary, NOT a measured exact analog clip level.
    # Current references flatten near ADC 3870; fit comfortably below that region.
    accepted, rejected = [], []
    with pathlib.Path(path).open(encoding='utf-8-sig', newline='') as stream:
        for line, row in enumerate(csv.DictReader(stream), 2):
            item = dict(line=line, input=row)
            try:
                if not row.get('signal_adc', '').strip():
                    raise ValueError('Missing signal ADC measurement')
                name = row['range'].strip()
                fit_rows([row])  # Common input, sign, swing and gain checks.
                z, a = float(row['zero_adc']), float(row['signal_adc'])
                if max(z, a) >= 3600:
                    raise ValueError('Insufficient upper analog headroom for fitting (ADC >= 3600); repeat at lower voltage')
                slope = RANGES[name]*275/(1092*25)*gains[name]/1024
                predicted = abs(a-z)*slope
                item['predicted_old_dc_mV'] = predicted
                item['predicted_old_ground_mV'] = predicted
                if row.get('Rdisp_mV', '').strip():
                    display_zero = z
                    if row.get('Zoffset_adc', '').strip():
                        offset = float(row['Zoffset_adc'])
                        if not math.isfinite(offset) or not -2048<=offset<=2047:
                            raise ValueError('Invalid Zoffset_adc')
                        display_zero = 2048+offset
                    item['display_zero_adc'] = display_zero
                    item['display_zero_source'] = 'Zoffset_adc' if row.get('Zoffset_adc','').strip() else 'ground mean assumed; stored Z not provided'
                    predicted = abs(a-display_zero)*slope
                    item['predicted_old_dc_mV'] = predicted
                    shown = float(row['Rdisp_mV'])
                    if not math.isfinite(shown) or shown < 0:
                        raise ValueError('Invalid Rdisp_mV')
                    # DC RMS consistency check; allow 3 ADC codes of zero drift
                    # plus 5 mV of displayed-voltage rounding. Rdisp is NOT truth.
                    tolerance = 3*slope+5
                    item.update(display_error_mV=shown-predicted, display_tolerance_mV=tolerance)
                    if abs(shown-predicted) > tolerance:
                        raise ValueError('Rdisp_mV does not agree with ADC/ground reading; repeat synchronously and record Z')
                accepted.append(item)
            except (ValueError, KeyError, OverflowError) as error:
                item['reason'] = str(error)
                rejected.append(item)
    measured = {}
    for name in RANGES:
        group = [item for item in accepted if item['input']['range'].strip() == name]
        if not group:
            continue
        try:
            measured.update(fit_rows([item['input'] for item in group]))
        except ValueError as error:
            for item in group:
                accepted.remove(item)
                item['reason'] = str(error)
                rejected.append(item)
    for item in accepted:
        row = item['input']; name = row['range'].strip()
        item['predicted_fitted_dc_mV'] = item['predicted_old_ground_mV']*measured[name]['gain_q10']/gains[name]
    return dict(measured=measured, accepted=accepted, rejected=rejected,
                baseline_gain_q10=gains, missing=[name for name in RANGES if name not in measured])

def apply(report, header):
    header = pathlib.Path(header)
    text = header.read_text(encoding='utf-8')
    pattern = r'(vertical_gain_q10\[\]\s*=\s*\{)([^}]+)(\})'
    match = re.search(pattern, text)
    if not match:
        raise ValueError('Calibration table not found')
    values = [int(v.strip()) for v in match[2].split(',')]
    if len(values) != len(RANGES):
        raise ValueError('Unexpected calibration table length')
    for i,name in enumerate(RANGES):
        if name in report:
            values[i] = report[name]['gain_q10']
    text = text[:match.start(2)]+','.join(map(str, values))+text[match.end(2):]
    text = re.sub(r'// Provisional device observations:.*?constexpr uint16_t',
                  '// Values fitted from external DC references with tools/calibrate.py.\n'
                  '// Ranges missing from the supplied CSV keep their previous values.\n'
                  '// See the calibration report for provenance and coverage.\nconstexpr uint16_t',
                  text, flags=re.S)
    header.write_text(text, encoding='utf-8')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv')
    parser.add_argument('--audit', action='store_true', help='check headroom and optional Rdisp_mV; exclude suspect points and retain other ranges')
    parser.add_argument('--display-gains', type=pathlib.Path, help='JSON gain map from the firmware used when recording Rdisp_mV')
    parser.add_argument('--apply', action='store_true', help='update measured ranges in the project header; rebuild afterward')
    parser.add_argument('--report', type=pathlib.Path)
    args = parser.parse_args()
    try:
        if args.audit:
            gains = read_gains(ROOT/'src/vertical_calibration.hpp')
            if args.display_gains:
                gains = json.loads(args.display_gains.read_text(encoding='utf-8-sig'))
                if set(gains) != set(RANGES) or any(not isinstance(v,int) or not 512<=v<=2048 for v in gains.values()):
                    raise ValueError('Display-gain profile must contain all nine Q10 factors')
            result = audit(args.csv, gains)
            measured = result['measured']
            if not measured:
                raise ValueError('No usable reference points after audit')
        else:
            measured = fit(args.csv)
            result = dict(measured=measured, missing=[name for name in RANGES if name not in measured])
        result.update(source=str(pathlib.Path(args.csv).resolve()), applied=args.apply)
        if args.apply:
            apply(measured, ROOT/'src/vertical_calibration.hpp')
        rendered = json.dumps(result, indent=2)
        if args.report:
            args.report.write_text(rendered, encoding='utf-8')
        print(rendered)
    except (ValueError, KeyError, OSError) as error:
        parser.exit(2, str(error)+'\n')

if __name__ == '__main__':
    main()
