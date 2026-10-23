import argparse
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('src/*.cpp', 'src/*.h')
LOWEST_CLIENT_ADDRESS = 0x400000
HIGHEST_CLIENT_ADDRESS = 0xFFFFFF
MAX_RANGE_SPAN = 0x1000
HEX_ADDRESS = re.compile(r'0x([0-9A-Fa-f]{6,8})\b')
README_HEX = re.compile(r'0x([0-9A-Fa-f]{3,8})\b')
README_RANGE = re.compile(r'0x([0-9A-Fa-f]{3,8})`?\s*(?:[–—-]|to)\s*`?(?:0x)?([0-9A-Fa-f]{3,8})\b')


def documented(readme):
    values = {int(match, 16) for match in README_HEX.findall(readme)}
    spans = []
    for low, high in README_RANGE.findall(readme):
        low, high = int(low, 16), int(high, 16)
        if 0 <= high - low <= MAX_RANGE_SPAN:
            spans.append((low, high))
    return values, spans


def is_documented(value, values, spans):
    return value in values or any(low <= value <= high for low, high in spans)


def check(root):
    values, spans = documented((root / 'README.md').read_text(encoding='utf-8'))
    issues = []
    files = sorted({path for pattern in SOURCES for path in root.glob(pattern)})
    for path in files:
        text = path.read_text(encoding='utf-8')
        for number, line in enumerate(text.split('\n'), 1):
            for match in HEX_ADDRESS.finditer(line):
                value = int(match.group(1), 16)
                if LOWEST_CLIENT_ADDRESS <= value <= HIGHEST_CLIENT_ADDRESS and not is_documented(value, values, spans):
                    issues.append({'path': path.relative_to(root).as_posix(), 'line': number,
                                   'address': f'0x{value:08X}'})
    return {'status': 'failed' if issues else 'passed', 'checked_files': len(files), 'issues': issues}


def main(argv=None):
    parser = argparse.ArgumentParser(description='Require README evidence for every client address in the sources')
    parser.add_argument('--root', type=Path, default=ROOT)
    result = check(parser.parse_args(argv).root)
    print(json.dumps(result, indent=2))
    return int(result['status'] != 'passed')


if __name__ == '__main__':
    sys.exit(main())
