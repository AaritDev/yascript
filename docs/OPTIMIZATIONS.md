# yascript Optimizations Guide

This document describes the compile-time optimizations currently performed by the yascript parser and the lowering details that matter for those optimizations.

## Overview

yascript lowers source code into a tape-VM instruction stream, then runs a **multi-pass peephole optimizer**. Each pass scans the instruction stream, folds local patterns, removes redundant instructions, and then remaps repeat/jump targets so control flow still points at the correct optimized instruction indexes.

The optimizer repeats until no pass changes the program, with a safety limit of 50 passes.

## Lowering Model

Low-level tape commands map directly to VM instructions:

```yascript
add 10
rght
set 65
output
```

The scripting layer lowers to cell-based VM operations:

```ruby
let a = 40
let b = 2
let c = a + b
print c
```

That uses variable cells plus temporary cells and emits instructions such as `Copy`, `AddVar`, and `PrintVar`. `if`, `else`, and `while` lower to `JumpIfFalse` and `Jump` instructions whose targets are back-patched after parsing.

Variables and constants are resolved at parse time. They have no name lookup during VM execution.

## Optimization Patterns

### 1. Repeat Unwrapping

**Pattern**: `repeat N simple_op; end` becomes a single multiplied operation when the body is foldable.

```yascript
repeat 74 add; end      # add 74
repeat 10 left; end     # left 10
repeat 5 sub; end       # sub 5
repeat 42 add 2; end    # add 84
```

Foldable repeat body operations are `add`, `sub`, `left`, `rght`, `set`, and `zero`.

### 2. Empty and Single-Run Repeat Handling

**Pattern**: `repeat 0` blocks are removed; `repeat 1` blocks are unwrapped.

```yascript
repeat 0 add; end       # removed
repeat 1 add 5; end     # add 5
repeat 1 zero; end      # zero
```

### 3. Consecutive Operation Merging

**Pattern**: Adjacent operations of the same arithmetic or pointer type are merged.

```yascript
add 10; add 20;                  # add 30
left 5; left 3;                  # left 8
sub 100; sub 50;                 # sub 150
rght 1; rght 1; rght 1; rght 1;  # rght 4
```

This applies to `add`, `sub`, `left`, and `rght`.

### 4. Constant Folding on Current Cell

**Pattern**: Known current-cell values are folded through immediate arithmetic.

```yascript
set 50; add 15;      # set 65
set 100; sub 30;     # set 70
set 0;               # zero
```

Constraints:

- `set X; add Y` folds only when `X + Y` fits in `uint64_t`.
- `set X; sub Y` folds only when `X >= Y`.
- Unsafe folds are left as runtime-checked instructions.

### 5. Dead Overwrite Elimination

**Pattern**: Writes that are immediately overwritten are removed.

```yascript
set 10; set 20; print;     # set 20; print
zero; set 50; print;       # set 50; print
set 99; zero; print;       # zero; print
zero; zero; add 5; print;  # zero; add 5; print
```

### 6. Zero/Add Rewrites

**Pattern**: Adding to a known-zero cell becomes a set.

```yascript
zero; add 65; output;      # set 65; output
```

`set 0` is normalized to `zero`, so this also applies after `set 0`.

### 7. Dead Pointer Shifts Before Goto

**Pattern**: A relative pointer shift immediately followed by an absolute `goto` is removed.

```yascript
left 10; goto 5; print;    # goto 5; print
rght 100; goto 0;          # goto 0
```

Only pointer shifts are removed. Cell mutations and I/O instructions before `goto` are preserved.

### 8. Consecutive Goto Elimination

**Pattern**: An absolute `goto` immediately overwritten by another absolute `goto` is removed.

```yascript
goto 1; goto 2; add 10; print;
# goto 2; add 10; print;
```

## Control-Flow Safety

Optimization can change instruction indexes, so the parser maintains an `oldToNew` map during each pass. After instructions are folded or removed, targets are remapped for:

- `RepeatStart`
- `RepeatEnd`
- `Jump`
- `JumpIfFalse`

The parser also rebuilds repeat links before and after optimization. This keeps low-level `repeat` blocks and scripting-layer `if`/`while` jumps consistent after peephole rewrites.

## Multi-Pass Example

Optimizations can create new optimization opportunities:

```yascript
set 10; add 5; set 20; add 30;
```

Pass results:

```yascript
set 10; add 5;  # set 15
set 15; set 20; # set 20
set 20; add 30; # set 50
```

Final result:

```yascript
set 50
```

## Runtime Optimizations

The runner uses:

- A dynamically growing `uint64_t` tape
- Initial tape pre-sizing from `goto` targets and variable instruction operands
- A 4KB output buffer
- GCC/Clang computed-goto threaded dispatch when available
- A switch-based VM fallback for other compilers

## Current Limits

The optimizer is intentionally conservative:

- It does not currently constant-fold scripting expressions like `let x = 2 + 3`.
- It does not eliminate temporary cells emitted while lowering expressions.
- It does not perform global data-flow optimization.
- It does not move code across I/O or control-flow boundaries.

## Testing Optimizations

Core optimizer checks:

```bash
make test
```

Additional scripting checks:

```bash
./tests/run_scripting_tests.sh
```

Useful spot checks:

```bash
./bin/yascript -e "repeat 65 add; end; output;"
./bin/yascript -e "set 40; add 25; print;"
./bin/yascript -e "goto 1; goto 2; add 5; print;"
./bin/yascript -e "let i = 1; let s = 0; while i <= 5; s = s + i; i = i + 1; end; print s"
```
