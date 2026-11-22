#!/usr/bin/env python3
# Minimal resolver for the one header-only package in the minimal cross build.
# It reads the actual upstream package version and checks the header exists.
# Unknown packages/options fail instead of fabricating dependency detection.
import os, pathlib, re, sys
root=pathlib.Path(os.environ['LR_NV_HEADERS'])
version=re.search(r'^Version: (.+)$', (root/'ffnvcodec.pc.in').read_text(), re.M).group(1)
args=sys.argv[1:]
if args==['--version']:
    print('0.1'); sys.exit(0)
if not (root/'include/ffnvcodec/nvEncodeAPI.h').is_file(): sys.exit(1)
plain=[a for a in args if not a.startswith('--')]
if not plain or plain[0]!='ffnvcodec': sys.exit(1)
def ver(x): return tuple(map(int, x.split('.')))+(0,)*(4-len(x.split('.')))
for match in re.finditer(r'ffnvcodec\s*(>=|<=|=|>|<)\s*([0-9.]+)', ' '.join(plain)):
    op,wanted=match.groups();a,b=ver(version),ver(wanted)
    if not {'>=':a>=b,'<=':a<=b,'=':a==b,'>':a>b,'<':a<b}[op]: sys.exit(1)
if '--modversion' in args: print(version)
elif '--cflags' in args or '--cflags-only-I' in args: print('-I'+str(root/'include'))
elif '--variable=includedir' in args: print(root/'include')
elif '--libs' in args or '--exists' in args: pass
else: sys.exit(1)
