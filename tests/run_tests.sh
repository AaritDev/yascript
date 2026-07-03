#!/bin/bash
YASCRIPT="./bin/yascript"
PASS=0
FAIL=0

test_case() {
    local name="$1"
    local code="$2"
    local expected="$3"
    local output
    output=$($YASCRIPT -e "$code" 2>&1)
    if [ "$output" = "$expected" ]; then
        echo "✓ $name"
        ((PASS++))
    else
        echo "✗ $name"
        echo "  Expected: '$expected'"
        echo "  Got:      '$output'"
        ((FAIL++))
    fi
}

test_error_case() {
    local name="$1"
    local code="$2"
    local expected_fragment="$3"
    local output
    output=$($YASCRIPT -e "$code" 2>&1)
    if [[ "$output" == *"$expected_fragment"* ]]; then
        echo "✓ $name"
        ((PASS++))
    else
        echo "✗ $name"
        echo "  Expected error containing: '$expected_fragment'"
        echo "  Got: '$output'"
        ((FAIL++))
    fi
}

echo "Running yascript test suite..."
echo "=============================="

# Basic operations
test_case "set and print" "set 65; print;" "65"
test_case "add operation" "add 10; print;" "10"
test_case "multiple adds" "add 5; add 10; print;" "15"

# Optimizations
test_case "repeat unwrapping" $'repeat 50 add; end\nprint;' "50"
test_case "constant folding" "set 40; add 25; print;" "65"
test_case "dead set elimination" "set 10; set 20; print;" "20"
test_case "zero add optimization" "zero; add 30; print;" "30"
test_case "repeat one block" $'repeat 1 add 5; add 6; end\nprint;' "11"
test_case "repeat zero block" $'set 9;\nrepeat 0 add 5; end\nprint;' "9"
test_case "repeat multiply body arg" $'repeat 42 add 2; end\nprint;' "84"
test_case "nested repeat block" $'repeat 2\n    repeat 3 add; end\nend\nprint;' "6"

# Statement separators
test_case "newline separator" $'add 5\nadd 10\nprint;' "15"
test_case "indented block" $'repeat 2\n    add 1;\nend\nprint;' "2"
test_error_case "missing separator" "add print;" "missing statement separator"
test_error_case "repeat missing separator" "repeat 2 add end" "missing statement separator"
test_error_case "unknown command suggestion" "right;" "did you mean 'rght'"
test_error_case "missing number argument" "set;" "set expects a number argument"
test_error_case "number out of range" "add 18446744073709551616;" "number is out of uint64 range"

# I/O
test_case "output character" "set 65; output;" "A"
test_case "hello substring" $'repeat 72 add; end\noutput;\nrght;\nrepeat 101 add; end\noutput;' "He"

# Arithmetic
test_case "sub operation" "set 100; sub 30; print;" "70"
test_case "left/right movement" "add 10; rght; add 20; left; print;" "10"
test_error_case "value underflow message" "sub;" "value underflow"
test_error_case "value overflow message" "set 18446744073709551615; add;" "value overflow"
test_error_case "pointer underflow message" "left;" "pointer underflow"

# Visual Diagnostics & Goto Correctness
test_error_case "visual caret error indicators" "add print;" "^^^^^"
test_case "cyclic goto compilation does not hang" "goto 0; print;" "0"
test_error_case "unmatched repeat points to repeat line" $'repeat 10\nadd 5;' "missing 'end' for repeat at line 1"
test_case "goto preservation of cell values" "add 10; goto 5; goto 0; print;" "10"


echo "=============================="
echo "Passed: $PASS"
echo "Failed: $FAIL"
exit $FAIL
