"""Create untracked TLS header from user-generated PEM files. Never prints keys.

Run after generating certificate and key with OpenSSL as in docs/HTTPS_STAGE1.md.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
cert = root / 'include' / 'tls_certificate.pem'
key = root / 'include' / 'tls_private_key.pem'
header = root / 'include' / 'tls_credentials.h'

if header.exists():
    raise SystemExit('tls_credentials.h already exists; refusing to overwrite')

def read_pem(path: Path, kind: str) -> str:
    text = path.read_text(encoding='ascii')
    if f'-----BEGIN {kind}-----' not in text or f'-----END {kind}-----' not in text:
        raise SystemExit(f'Invalid {kind} PEM file: {path.name}')
    if ')KHAIRC_TLS"' in text:
        raise SystemExit('Unsupported PEM content delimiter')
    return text

try:
    certificate = read_pem(cert, 'CERTIFICATE')
    private = key.read_text(encoding='ascii')
    if '-----BEGIN PRIVATE KEY-----' not in private and '-----BEGIN EC PRIVATE KEY-----' not in private:
        raise SystemExit('Expected an unencrypted PEM private key')
    header.write_text(
        '#pragma once\n'
        'static const char HUB_TLS_CERT_PEM[] = R"KHAIRC_TLS(' + certificate + ')KHAIRC_TLS";\n'
        'static const char HUB_TLS_KEY_PEM[] = R"KHAIRC_TLS(' + private + ')KHAIRC_TLS";\n',
        encoding='ascii')
except FileNotFoundError:
    raise SystemExit('Missing tls_certificate.pem or tls_private_key.pem in include/') from None
print('Created ignored include/tls_credentials.h. Do not commit or share this file.')
