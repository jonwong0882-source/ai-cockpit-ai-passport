#!/usr/bin/env python3
"""Check glyph coverage for the community firmware's actual ASCII fonts."""
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'main/token_main.c').read_text()
for literal in re.findall(r'"(?:[^"\\]|\\.)*"',main):
    assert all(ord(c)<127 for c in literal), 'UI text exceeds the selected UNSCII font'
assert '&lv_font_unscii_8' in main
assert 'CONFIG_LV_FONT_UNSCII_8=y' in (root/'sdkconfig.defaults').read_text()
source=(root/'assets/fonts/token_digits.c').read_text()
required=set(map(ord,'-.0123456789'))
for face in ('active','dim','light','shade'):
    offsets=re.search(rf'{face}_unicode\[\]=\{{([^}}]+)\}}',source)
    start=re.search(rf'{face}_cmaps\[\]=\{{\{{\.range_start=(\d+)',source)
    assert offsets and start, face
    codes={int(start.group(1))+int(n) for n in offsets.group(1).split(',')}
    assert required<=codes, face
    assert f'token_digits_{face}=' in source
print('Community ASCII labels and four digit-font faces: PASS')
