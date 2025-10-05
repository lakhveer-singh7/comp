#!/usr/bin/env bash
set -euo pipefail

# Build with Flex/Bison
make clean
make -j

# Ensure outputs dir exists
mkdir -p outputs

# Discover test cases
CASE_DIR="tests/cases"
NEG_DIR="tests/negative"
EXP_DIR="tests/expected"

pass=0
fail=0

declare -a cases
while IFS= read -r -d '' f; do cases+=("$f"); done < <(find "$CASE_DIR" -maxdepth 1 -type f -name '*.mc' -print0 | sort -z)

for mc in "${cases[@]}"; do
  name=$(basename "$mc" .mc)
  out="outputs/${name}.ll"
  if ./mycc -o "$out" "$mc"; then
    echo "PASS compile: $mc -> $out"
    # Optional check: grep patterns from expected file if exists
    check="$EXP_DIR/${name}.check"
    if [[ -f "$check" ]]; then
      ok=1
      while IFS= read -r line; do
        [[ "$line" =~ ^;;[[:space:]]*CHECK: ]] || continue
        pat=${line#*CHECK:}
        if ! grep -E -q "$pat" "$out"; then
          echo "FAIL check: $name missing pattern: $pat"
          ok=0
          break
        fi
      done < "$check"
      if [[ $ok -eq 1 ]]; then echo "PASS checks: $name"; ((pass++)); else ((fail++)); fi
    else
      ((pass++))
    fi
  else
    echo "FAIL compile: $mc"; ((fail++))
  fi
done

# Negative tests: expect failure
while IFS= read -r -d '' f; do
  name=$(basename "$f" .mc)
  out="outputs/${name}.ll"
  if ./mycc -o "$out" "$f"; then
    echo "FAIL negative (succeeded): $f"; ((fail++))
  else
    echo "PASS negative (failed as expected): $f"; ((pass++))
  fi
done < <(find "$NEG_DIR" -maxdepth 1 -type f -name '*.mc' -print0 | sort -z)

echo "Summary: $pass passed, $fail failed"
[[ $fail -eq 0 ]] || exit 1
