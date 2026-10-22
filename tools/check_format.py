import argparse
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SUFFIXES = {'.c', '.cc', '.cpp', '.h', '.hpp', '.hlsl', '.hlsli', '.py', '.cmake'}
MAX_LINE_LENGTH = 120


def checked(name):
    path = Path(name)
    return path.suffix in SUFFIXES or path.name == 'CMakeLists.txt'


def tracked_files(root):
    names = subprocess.run(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=root,
                           check=True, capture_output=True).stdout.decode('utf-8').split('\0')
    return sorted(name for name in names if name and checked(name) and (root / name).is_file())


def file_issues(name, text):
    if text and not text.endswith('\n'):
        yield {'path': name, 'line': text.count('\n') + 1, 'problem': 'no trailing newline'}
    for number, line in enumerate(text.split('\n'), 1):
        line = line.rstrip('\r')
        if '\t' in line:
            yield {'path': name, 'line': number, 'problem': 'tab'}
        if line != line.rstrip():
            yield {'path': name, 'line': number, 'problem': 'trailing whitespace'}
        if len(line) > MAX_LINE_LENGTH:
            yield {'path': name, 'line': number, 'problem': f'{len(line)} columns'}


def check(root):
    files = tracked_files(root)
    issues = []
    for name in files:
        issues.extend(file_issues(name, (root / name).read_bytes().decode('utf-8')))
    return {'status': 'failed' if issues else 'passed', 'checked_files': len(files), 'issues': issues}


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Reject tabs, trailing whitespace, long lines and missing final newlines')
    parser.add_argument('--root', type=Path, default=ROOT)
    result = check(parser.parse_args(argv).root)
    print(json.dumps(result, indent=2))
    return int(result['status'] != 'passed')


if __name__ == '__main__':
    sys.exit(main())
