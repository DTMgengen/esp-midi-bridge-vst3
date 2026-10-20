"""Check that the accepted tracking engine was extracted without retuning.

python scripts/verify_engine.py ../esp-string-lab
The original checkout must still be at the accepted engine revision.
"""
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'docs/tracking-provenance.json').read_text(encoding='utf-8'))
for name, digest in manifest['extracted_sha256'].items():
    # Git/Windows checkout line endings must not invalidate source provenance.
    canonical = (root / name).read_text(encoding='utf-8').encode('utf-8')
    assert hashlib.sha256(canonical).hexdigest() == digest, f'Engine changed: {name}'
if len(sys.argv) > 1:
    original = Path(sys.argv[1]) / 'Source'
    header = (original / 'Dsp.h').read_text(encoding='utf-8').split('struct Route')[0] + '}\n'
    source = (original / 'Dsp.cpp').read_text(encoding='utf-8')
    source = source[:source.index('float blep(')] + source[source.index('void fft('):]
    source = source.split('float Envelope::tick')[0] + '}\n'
    assert header == (root / 'Source/Dsp.h').read_text(encoding='utf-8')
    assert source == (root / 'Source/Dsp.cpp').read_text(encoding='utf-8')
    for name in ('StrictPitch.cpp', 'StrictPitch.h'):
        assert (original / name).read_text(encoding='utf-8') == (root / 'Source' / name).read_text(encoding='utf-8')
print('PASS: accepted v0.5.0 Tracker/StrictPitch preserved; synthesis removed')
