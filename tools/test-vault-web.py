"""Run independent Node crypto/codec tests; never contacts Firebase or the Hub."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
tests = sorted((root / 'web/vault/test').glob('*.test.mjs'))
sys.exit(subprocess.run(['node', '--test', *map(str, tests)], cwd=root).returncode)
