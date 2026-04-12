#!/usr/bin/env bash
#
# test-headless.sh — Build botnik and exercise headless mode.
#
# Usage:  bash scripts/test-headless.sh
#
# Exit code 0 = all ran tests passed.  Non-zero = at least one failure.
# Tests that require Ollama are skipped (not failed) when it's unreachable.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BOTNIK="$REPO_ROOT/build/botnik"

# Ensure BOTNIK_EXTRA_CLOCK doesn't interfere with tests.
unset BOTNIK_EXTRA_CLOCK

PASSED=0
FAILED=0
SKIPPED=0
BOTNIK_PID=""

cleanup() {
    if [[ -n "$BOTNIK_PID" ]] && kill -0 "$BOTNIK_PID" 2>/dev/null; then
        kill "$BOTNIK_PID" 2>/dev/null || true
        wait "$BOTNIK_PID" 2>/dev/null || true
    fi
    BOTNIK_PID=""
}
trap cleanup EXIT

pass() { echo "  PASS: $1"; PASSED=$((PASSED + 1)); }
fail() { echo "  FAIL: $1"; FAILED=$((FAILED + 1)); }
skip() { echo "  SKIP: $1"; SKIPPED=$((SKIPPED + 1)); }

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
echo "=== Building botnik ==="
(cd "$REPO_ROOT" && cmake -B build >/dev/null 2>&1 && cmake --build build >/dev/null 2>&1)
if [[ ! -x "$BOTNIK" ]]; then
    echo "FAIL: build did not produce $BOTNIK"
    exit 1
fi
echo "  Build OK"
echo

# ---------------------------------------------------------------------------
# Test 1: Startup + clean shutdown via EOF
# ---------------------------------------------------------------------------
echo "=== Test 1: startup and clean shutdown ==="

# Launch headless with stdin from a pipe that closes immediately (EOF).
echo "" | timeout 10 "$BOTNIK" --headless >/dev/null 2>/dev/null
EXIT_CODE=$?

if [[ $EXIT_CODE -eq 0 ]]; then
    pass "headless starts and exits cleanly on EOF"
else
    fail "headless exited with code $EXIT_CODE (expected 0)"
fi
echo

# ---------------------------------------------------------------------------
# Test 2: Startup + delayed shutdown via EOF
# ---------------------------------------------------------------------------
echo "=== Test 2: startup, hold open, then EOF ==="

# Use a FIFO so we control when stdin closes.
FIFO=$(mktemp -u /tmp/botnik-test-XXXXXX)
mkfifo "$FIFO"

"$BOTNIK" --headless < "$FIFO" >/dev/null 2>/dev/null &
BOTNIK_PID=$!

# Give the compositor a moment to initialise.
sleep 1

# Verify the process is still running.
if kill -0 "$BOTNIK_PID" 2>/dev/null; then
    pass "headless stays running while stdin is open"
else
    fail "headless exited prematurely"
    BOTNIK_PID=""
    rm -f "$FIFO"
    echo
    # Skip to Ollama test since the process died.
    exec 3>&1  # keep going
fi

# Close stdin → trigger clean exit.
exec 3>"$FIFO"   # open write end
exec 3>&-        # close it → EOF for the reader

if wait "$BOTNIK_PID" 2>/dev/null; then
    pass "headless exits cleanly after delayed EOF"
else
    fail "headless did not exit cleanly after EOF"
fi
BOTNIK_PID=""
rm -f "$FIFO"
echo

# ---------------------------------------------------------------------------
# Test 3: Send a chat message (requires Ollama)
# ---------------------------------------------------------------------------
echo "=== Test 3: chat round-trip (requires Ollama) ==="

OLLAMA_UP=false
if curl -sf http://localhost:11434/api/tags >/dev/null 2>&1; then
    OLLAMA_UP=true
fi

if [[ "$OLLAMA_UP" == "false" ]]; then
    skip "Ollama not reachable at localhost:11434 — skipping chat test"
else
    FIFO=$(mktemp -u /tmp/botnik-test-XXXXXX)
    mkfifo "$FIFO"
    OUTFILE=$(mktemp /tmp/botnik-out-XXXXXX)

    "$BOTNIK" --headless < "$FIFO" > "$OUTFILE" 2>/dev/null &
    BOTNIK_PID=$!

    sleep 1

    # Send a test message.
    exec 3>"$FIFO"
    echo "hello" >&3

    # Wait up to 30 seconds for any output.
    DEADLINE=$((SECONDS + 30))
    GOT_RESPONSE=false
    while [[ $SECONDS -lt $DEADLINE ]]; do
        if [[ -s "$OUTFILE" ]]; then
            GOT_RESPONSE=true
            break
        fi
        sleep 0.5
    done

    if [[ "$GOT_RESPONSE" == "true" ]]; then
        pass "received a response from the model"
    else
        fail "no response within 30 seconds"
    fi

    # Clean up: close stdin to trigger exit.
    exec 3>&-
    timeout 5 wait "$BOTNIK_PID" 2>/dev/null || kill "$BOTNIK_PID" 2>/dev/null || true
    wait "$BOTNIK_PID" 2>/dev/null || true
    BOTNIK_PID=""
    rm -f "$FIFO" "$OUTFILE"
fi
echo

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo "=== Results ==="
echo "  Passed:  $PASSED"
echo "  Failed:  $FAILED"
echo "  Skipped: $SKIPPED"

if [[ $FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
