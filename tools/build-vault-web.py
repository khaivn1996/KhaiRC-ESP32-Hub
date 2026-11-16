"""Build the pinned, gzip-compressed browser bundle into flash source."""
import gzip
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / 'web/vault'


def main():
    npm, npx = shutil.which('npm'), shutil.which('npx')
    if not npm or not npx:
        raise SystemExit('Node/npm are required only to rebuild the vault bundle')
    subprocess.run([npm, 'ci', '--no-audit', '--no-fund'], cwd=WEB, check=True)
    result = subprocess.run([npx, '--no-install', 'esbuild', 'src/vault.js', '--bundle',
                             '--minify', '--format=iife', '--target=es2020',
                             '--legal-comments=none'], cwd=WEB, check=True, capture_output=True)
    raw = result.stdout
    packed = gzip.compress(raw, compresslevel=9, mtime=0)
    print(f'Vault bundle: raw={len(raw)} bytes, gzip={len(packed)} bytes')
    if len(packed) > 120 * 1024:
        raise SystemExit('Vault gzip exceeds 120 KiB; generated files were not changed')
    header = ('#pragma once\n#include <cstddef>\n#include <cstdint>\n'
              'namespace VaultWebBundle {\nconst std::uint8_t* data();\nstd::size_t size();\n}\n')
    rows = ['    ' + ', '.join(f'0x{b:02x}' for b in packed[i:i+16]) + ','
            for i in range(0, len(packed), 16)]
    source = ('// GENERATED - do not edit. Run python tools/build-vault-web.py\n'
              '#include "VaultWebBundle.h"\n#include <Arduino.h>\n\n'
              'namespace VaultWebBundle {\nnamespace {\nconst std::uint8_t kData[] PROGMEM = {\n' +
              '\n'.join(rows) + '\n};\n}\nconst std::uint8_t* data() { return kData; }\n'
              'std::size_t size() { return sizeof(kData); }\n}\n')
    (ROOT / 'include/VaultWebBundle.h').write_text(header, encoding='utf-8', newline='\n')
    (ROOT / 'src/VaultWebBundle.cpp').write_text(source, encoding='utf-8', newline='\n')


if __name__ == '__main__':
    main()
