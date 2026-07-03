# yascript Optimizations Guide

This document explains all the compile-time optimizations performed by the yascript interpreter during the parsing and compilation phase.

## Overview

The yascript optimizer uses a **multi-pass peephole optimization** strategy that repeatedly scans the instruction stream looking for patterns it can simplify. The optimizer continues making passes until no more optimizations are possible (with a safety limit of 50 passes).

Blocks terminate with `end`; simple statements terminate with `;` or a newline.

## Optimization Patterns

### 1. Loop Unwrapping: `repeat N simple_op; end` → `op N`

**Pattern**: A repeat block containing exactly one simple operation

```yascript
repeat 74 add; end      # Unwraps to: add 74
repeat 10 left; end     # Unwraps to: left 10
repeat 5 sub; end       # Unwraps to: sub 5
```

**Why**: Loop overhead is eliminated; the operation is simply multiplied by the count.

**Edge Case**: 
```yascript
repeat 1 add; end       # Unwraps to: add 1 (and then may merge with adjacent adds)
```

---

### 2. Empty Loop Elimination: `repeat 0` or `repeat 1` blocks → `...`

**Pattern**: A repeat block with count of 1

```yascript
repeat 1 add; end       # Becomes: add 1
repeat 1 left; end      # Becomes: left 1

repeat 0 add; end       # Becomes: nothing
repeat 0 print; end     # Becomes: nothing
```

**Why**: Repeating once is equivalent to the body; repeating zero times is a no-op.

---

### 3. Repeat Body Folding

**Pattern**: A repeat block with a single pure instruction

```yascript
repeat 42 add 2; end    # Becomes: add 84
repeat 7 zero; end      # Becomes: zero
repeat 5 set 99; end    # Becomes: set 99
```

**Why**: Pure single-instruction blocks can be collapsed safely before execution.

---

### 4. Consecutive Operation Merging

**Pattern**: Two consecutive operations of the same type

```yascript
add 10; add 20;                  # Becomes: add 30
left 5; left 3;                  # Becomes: left 8
sub 100; sub 50;                 # Becomes: sub 150
rght 1; rght 1; rght 1; rght 1;  # Becomes: rght 4
```

**Why**: Fewer instructions = faster execution.

**Applies to**: `add`, `sub`, `left`, `right`

---

### 5. Constant Folding: `set X; add Y` → `set (X+Y)`

**Pattern**: Set followed by arithmetic operation on same cell

```yascript
set 50; add 15;      # Becomes: set 65
set 100; add 200;    # Becomes: set 300
set 100; sub 30;     # Becomes: set 70 (if 100 >= 30)
```

**Why**: Evaluate arithmetic at compile time.

**Constraints**:
- For `add`: Only applies when the folded value fits in `uint64_t`
- For `sub`: Only applies if `X >= Y` (to avoid underflow)

**Examples**:
```yascript
set 10; add 5; print;      # Becomes: set 15 print (output: 15)
set 100; sub 60; add 20; print;  # Becomes: set 40 add 20 print -> set 60 print (output: 60)
```

---

### 6. Dead Code Elimination: Sequential Overwrites

#### 5a. Dead Set: `set X; set Y` → `set Y`

**Pattern**: Two consecutive `set` operations

```yascript
set 10; set 20; print;     # Becomes: set 20 print (first set is dead)
set 0; set 100; add 50; print;  # Becomes: set 100 add 50 print -> set 150 print
```

**Why**: The first value is never used.

#### 5b. Dead Zero: `zero; set X` → `set X`

**Pattern**: Zero followed by set

```yascript
zero; set 50; print;      # Becomes: set 50 print
```

**Why**: Setting to a value already zeros the cell.

#### 5c. Zero + Add: `zero; add X` → `set X`

**Pattern**: Zero followed by add

```yascript
zero; add 65; output;     # Becomes: set 65 output
```

**Why**: Zero is an identity; adding to zero is equivalent to setting.

#### 5d. Set 0 + Add: `set 0; add X` → `add X`

**Pattern**: Set 0 followed by add (identity)

```yascript
set 0; add 50; print;     # Becomes: add 50 print
```

**Why**: Adding to zero is equivalent to just adding.

#### 5e. Dead Pointer Shifts Before Goto: `shift; goto X` → `goto X`

**Pattern**: Pointer shift operation (`left`, `rght`) immediately before `goto`

```yascript
left 10; goto 5; print;   # Becomes: goto 5 print (left is dead)
rght 100; goto 0;         # Becomes: goto 0 (rght is dead)
```

**Why**: A goto directly modifies the tape pointer, overriding any relative pointer movements immediately before it. Tape cell modifications (like `add`, `sub`, `set`, `zero`) are preserved.

**Note**: I/O operations (`output`, `print`, `read`) are also NOT eliminated before gotos.

---

### 7. Consecutive Goto Elimination: `goto X; goto Y;` → `goto Y`

**Pattern**: Two consecutive `goto` operations

```yascript
goto 1; goto 2; add 10; print;
# Becomes: goto 2; add 10; print;
```

**Why**: The second `goto` immediately overrides the tape pointer destination set by the first `goto`, making the first `goto` dead code.

---

### 8. Zero Elimination: `zero; zero` → `zero`

**Pattern**: Consecutive zero operations

```yascript
zero; zero; add 5; print;  # Becomes: zero add 5 print
```

**Why**: Zeroing an already-zero cell is redundant.

---

## Multi-Pass Strategy

The optimizer runs multiple passes because optimizations can create new optimization opportunities:

```yascript
set 10; add 5; set 20; add 30;
# Pass 1:
#   set 10 add 5 → set 15
#   set 15 set 20 → set 20 (dead set)
#   set 20 add 30 → set 50
#   Result: set 50
```

**Safety limit**: Maximum 50 passes to prevent infinite loops.

---

## Interaction Examples

These show how multiple optimizations work together:

### Example 1: Tight Loop
```yascript
repeat 5 add; end
add 10;
add 20;
# Pass 1: repeat 5 add; end -> add 5
# Pass 1: add 5; add 10; add 20; -> add 35
# Final: add 35
```

### Example 2: Dead Code Removal
```yascript
set 50; add 10; set 100; print;
# Pass 1: set 50; add 10; -> set 60
# Pass 1: set 60; set 100; -> set 100 (dead)
# Final: set 100 print (output: 100)
```

### Example 3: Complex Pattern
```yascript
set 0; add 65; output; left 1; add 72; output;
# Pass 1: set 0; add 65; -> set 65
# Pass 1: output; left 1; add 72; -> (no merge, left-add different types)
# Pass 2: output; left 1; -> (output stops merge)
# Pass 2: left 1; add 72; -> (left-add different types)
# Final: set 65; output; left 1; add 72; output;
```

### Example 4: Consecutive Gotos
```yascript
goto 1; goto 2; goto 3; add 10; print;
# Pass 1: goto 1; goto 2; (consecutive) -> goto 2
# Pass 2: goto 2; goto 3; (consecutive) -> goto 3
# Final: goto 3; add 10; print;
```

---

## Performance Impact

These optimizations provide significant benefits:

1. **Fewer instructions**: Reduces memory usage and instruction cache pressure
2. **Compile-time computation**: Values computed at compile time don't need runtime evaluation
3. **Reduced loops**: Loop unwrapping eliminates loop overhead for simple operations
4. **Better pipelining**: Fewer instructions = CPU pipeline more efficient

**Benchmark note**: A program with 1000 `repeat 1 add; end` blocks becomes just `add 1000` after optimization.

---

## Edge Cases Handled

| Case | Behavior |
|------|----------|
| `repeat 0 op; end` | Optimizes to no operation |
| `set UINT64_MAX; add 1;` | Reports value overflow at runtime |
| `goto [past_end]` | Runtime safety check (handled by runner) |
| `zero; zero; zero` | Merges to single `zero` |
| `add 10; output; add 5;` | Keeps `add 10` and `add 5` separate (output breaks merge) |
| `set X; print; set Y` | Keeps separate (print is I/O boundary) |

---

## Testing Optimizations

To verify optimizations are working:

```bash
# Compact form (should output 65)
./yascript -e "repeat 65 add; end; output;"

# Expanded form (equivalent, shows mergeable adds)
./yascript -e "add 10; add 20; add 20; add 15; output;"

# Dead code elimination
./yascript -e "set 10; set 20; print;"    # Output: 20

# Goto chaining
./yascript -e "goto 1; goto 2; add 5; print;"  # Output: 5

# Constant folding
./yascript -e "set 50; add 15; print;"    # Output: 65
```

---

## Implementation Notes

- Optimizer runs before execution (compile-time only)
- Uses `std::vector<Instruction>` for efficient manipulation
- Multi-pass approach with cycle detection (50-pass limit)
- Jump chain resolution uses recursion (depth limit 1000)
- No dataflow analysis beyond immediate instruction patterns

---

## Future Optimizations (Not Yet Implemented)

- Dead code path elimination (unreachable code after unconditional jumps)
- Register allocation (tracking which cells are actively used)
- Loop invariant code motion
- Common subexpression elimination
- Peephole patterns for common idioms (e.g., multiply using nested loops)
