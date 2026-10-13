
#!/bin/bash

PROGRAM="./boxforge"
PASSED=0
FAILED=0

if [ ! -x "$PROGRAM" ]; then
    echo "ERROR: boxforge executable not found. Run make first."
    exit 1
fi

expect_failure() {
    local name="$1"
    shift

    if "$@" >/dev/null 2>&1; then
        echo "FAIL: $name"
        FAILED=$((FAILED + 1))
    else
        echo "PASS: $name"
        PASSED=$((PASSED + 1))
    fi
}

expect_failure "Missing arguments are rejected" \
    "$PROGRAM"

expect_failure "Relative command path is rejected" \
    "$PROGRAM" bash

expect_failure "Missing root filesystem is rejected" \
    "$PROGRAM" --rootfs ./rootfs_does_not_exist /bin/bash

echo
echo "Tests passed: $PASSED"
echo "Tests failed: $FAILED"

if [ "$FAILED" -eq 0 ]; then
    exit 0
else
    exit 1
fi

