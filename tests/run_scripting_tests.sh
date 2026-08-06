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

test_case_file() {
    local name="$1"
    local file="$2"
    local expected="$3"
    local output
    output=$($YASCRIPT "$file" 2>&1)
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

echo "Running yascript scripting test suite..."
echo "========================================="

# Variable declarations
test_case "let declaration" $'let x = 42\nprint x' "42"
test_case "let default zero" $'let x\nprint x' "0"
test_case "const declaration" $'const N = 100\nprint N' "100"
test_case "variable reassignment" $'let x = 5\nx = 10\nprint x' "10"

# Arithmetic expressions
test_case "addition expression" $'let a = 10\nlet b = 20\nlet c = a + b\nprint c' "30"
test_case "subtraction expression" $'let a = 50\nlet b = 8\nlet c = a - b\nprint c' "42"
test_case "multiplication expression" $'let a = 6\nlet b = 7\nlet c = a * b\nprint c' "42"
test_case "division expression" $'let a = 84\nlet b = 2\nlet c = a / b\nprint c' "42"
test_case "modulo expression" $'let a = 100\nlet b = 58\nlet c = a % b\nprint c' "42"
test_case "chained arithmetic" $'let x = 2 + 3 * 4\nprint x' "14"
test_case "parenthesized arithmetic" $'let x = (2 + 3) * 4\nprint x' "20"

# Output as char
test_case "output variable as char" $'let ch = 65\noutput ch' "A"
test_case "output computed char" $'let ch = 60 + 6\noutput ch' "B"

# Comparisons and conditionals
test_case "if true branch" $'let x = 10\nif x > 5\n    print x\nend' "10"
test_case "if false branch skipped" $'let x = 2\nif x > 5\n    print x\nend' ""
test_case "if-else true branch" $'let x = 10\nif x > 5\n    print x\nelse\n    print 0\nend' "10"
test_case "if-else false branch" $'let x = 2\nif x > 5\n    print x\nelse\n    print 0\nend' "0"
test_case "equal comparison" $'let x = 7\nif x == 7\n    print x\nend' "7"
test_case "not equal comparison" $'let x = 7\nif x != 8\n    print x\nend' "7"
test_case "less equal comparison" $'let x = 5\nif x <= 5\n    print x\nend' "5"
test_case "greater equal comparison" $'let x = 5\nif x >= 5\n    print x\nend' "5"

# While loops
test_case "while loop countdown" $'let x = 3\nlet s = 0\nwhile x > 0\n    s = s + x\n    x = x - 1\nend\nprint s' "6"
test_case "while loop never enters" $'let x = 0\nwhile x > 0\n    x = x - 1\nend\nprint x' "0"
test_case "while loop to 5" $'let i = 1\nlet s = 0\nwhile i <= 5\n    s = s + i\n    i = i + 1\nend\nprint s' "15"

# Error cases
test_error_case "undefined variable" $'print x' "undefined variable"
test_error_case "const reassignment" $'const N = 5\nN = 10' "cannot reassign constant"
test_error_case "duplicate let" $'let x = 1\nlet x = 2' "already declared"
test_error_case "division by zero" $'let a = 10\nlet b = 0\nlet c = a / b\nprint c' "division by zero"
test_error_case "modulo by zero" $'let a = 10\nlet b = 0\nlet c = a % b\nprint c' "modulo by zero"
test_error_case "const needs initializer" $'const N' "requires an initializer"

echo "========================================="
echo "Passed: $PASS"
echo "Failed: $FAIL"
exit $FAIL
