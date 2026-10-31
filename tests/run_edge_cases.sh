#!/usr/bin/env bash
# Usage: [VERBOSE=1] run_edge_cases.sh <path-to-hydro> [cases-file]. Runs in a temp dir; prints FAIL lines (every case with VERBOSE=1).
HYDRO=$(realpath "$1"); CASES=$(realpath "${2:-$(dirname "$0")/edge_cases.txt}")
TMP=$(mktemp -d); cd "$TMP" || exit 1; pass=0; fail=0
while IFS= read -r line; do
  [[ -z "$line" || "$line" == \#* ]] && continue
  want=$(echo "${line%%|*}" | tr -d ' '); prog="${line#*|}"; prog="${prog# }"
  printf '%b' "$prog" > t.hy; rm -f out out.o out.asm
  if timeout 5 "$HYDRO" t.hy >/dev/null 2>&1 && [[ -x out ]]; then timeout 2 ./out; got=$?; else got=err; fi
  [[ "$got" == 124 ]] && got=timeout
  if [[ "$got" == "$want" ]]; then pass=$((pass+1)); [[ -n "$VERBOSE" ]] && echo "ok   want=$want got=$got | $prog"; else fail=$((fail+1)); echo "FAIL want=$want got=$got | $prog"; fi
done < "$CASES"
echo "passed $pass, failed $fail"; rm -rf "$TMP"; [[ $fail -eq 0 ]]
 