#!/usr/bin/env python3
import subprocess, sys, re, os, glob, shlex

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MYCC = os.path.join(ROOT, 'mycc')
OUTDIR = os.path.join(ROOT, 'outputs')
CASES_DIR = os.path.join(ROOT, 'tests', 'cases')
NEG_DIR = os.path.join(ROOT, 'tests', 'negative')
EXP_DIR = os.path.join(ROOT, 'tests', 'expected')

os.makedirs(OUTDIR, exist_ok=True)

ANSI_GREEN='\033[32m'
ANSI_RED='\033[31m'
ANSI_RESET='\033[0m'


def check_file(ir_text: str, check_path: str) -> (bool, str):
    # Simple FileCheck-like: lines starting with ';; CHECK:' are regex that must
    # appear in order in the IR text.
    pattern_lines = []
    for line in open(check_path, 'r', encoding='utf-8'):
        line = line.rstrip('\n')
        if line.startswith(';; CHECK:'):
            pattern_lines.append(line.split('CHECK:',1)[1].strip())
    pos = 0
    for i, pattern in enumerate(pattern_lines, 1):
        m = re.search(pattern, ir_text[pos:], re.MULTILINE)
        if not m:
            return False, f"Missing pattern #{i}: {pattern}"
        pos += m.end()
    return True, "OK"


def run_case(mc_path: str) -> (bool, str):
    name = os.path.splitext(os.path.basename(mc_path))[0]
    out_ll = os.path.join(OUTDIR, f"{name}.ll")
    cmd = [MYCC, '-o', out_ll, mc_path]
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        return False, f"compiler exit {r.returncode}: {r.stderr.strip()}"
    check_path = os.path.join(EXP_DIR, f"{name}.check")
    if os.path.exists(check_path):
        with open(out_ll, 'r', encoding='utf-8') as f:
            ok, msg = check_file(f.read(), check_path)
            if not ok:
                return False, msg
    return True, "OK"


def run_negative(mc_path: str) -> (bool, str):
    name = os.path.splitext(os.path.basename(mc_path))[0]
    out_ll = os.path.join(OUTDIR, f"{name}.ll")
    cmd = [MYCC, '-o', out_ll, mc_path]
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if r.returncode == 0:
        return False, "expected failure but succeeded"
    # Optional error checks
    check_path = os.path.join(EXP_DIR, f"{name}.check")
    if os.path.exists(check_path):
        # Lines starting with ';; ERR:' must appear (regex) in stderr
        pos = 0
        for i, line in enumerate(open(check_path, 'r', encoding='utf-8'), 1):
            line = line.rstrip('\n')
            if line.startswith(';; ERR:'):
                pattern = line.split('ERR:', 1)[1].strip()
                m = re.search(pattern, r.stderr[pos:], re.MULTILINE)
                if not m:
                    return False, f"missing error pattern #{i}: {pattern}"
                pos += m.end()
    return True, "OK"


def main():
    total = 0
    passed = 0
    results = []

    for mc in sorted(glob.glob(os.path.join(CASES_DIR, '*.mc'))):
        total += 1
        ok, msg = run_case(mc)
        results.append((mc, ok, msg))
        if ok: passed += 1

    for mc in sorted(glob.glob(os.path.join(NEG_DIR, '*.mc'))):
        total += 1
        ok, msg = run_negative(mc)
        results.append((mc, ok, msg))
        if ok: passed += 1

    for mc, ok, msg in results:
        color = ANSI_GREEN if ok else ANSI_RED
        status = 'PASS' if ok else 'FAIL'
        print(f"{color}{status}{ANSI_RESET} {os.path.relpath(mc, ROOT)} - {msg}")

    print(f"Summary: {passed}/{total} passed")
    sys.exit(0 if passed == total else 1)

if __name__ == '__main__':
    main()
