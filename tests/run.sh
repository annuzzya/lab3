#!/bin/sh
cd "$(dirname "$0")/.." || exit 1

CC=${CC:-cc}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
have_node=$(command -v node >/dev/null 2>&1 && echo 1 || echo 0)
pass=0
fail=0

check() {
    if cmp -s "$tmp/expected" "$2"; then
        printf '  %-22s %s ok\n' "$1" "$3"
        pass=$((pass + 1))
    else
        printf '  %-22s %s FAILED\n' "$1" "$3"
        diff "$tmp/expected" "$2" | sed 's/^/      /' | head -10
        fail=$((fail + 1))
    fi
}

echo "programs (reference: $CC)"
for src in examples/*.c; do
    name=$(basename "$src")
    "$CC" -w -o "$tmp/ref" "$src" -lm || { echo "  $name: $CC failed"; fail=$((fail + 1)); continue; }
    "$tmp/ref" > "$tmp/expected"
    ./cparse "$src" > "$tmp/interp"
    check "$name" "$tmp/interp" "interpreter"
    if [ "$have_node" = 1 ]; then
        ./cparse --js "$tmp/out.js" "$src" && node "$tmp/out.js" > "$tmp/js"
        check "$name" "$tmp/js" "javascript "
    fi
done

echo "diagnostics"
for src in examples/errors/*.c; do
    name=$(basename "$src")
    if ./cparse "$src" > /dev/null 2> "$tmp/err"; then
        printf '  %-22s expected an error\n' "$name"
        fail=$((fail + 1))
    else
        printf '  %-22s rejected ok\n' "$name"
        pass=$((pass + 1))
    fi
done

[ "$have_node" = 1 ] || echo "note: node is not installed, JavaScript output was not checked"
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]
