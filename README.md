# yascript - Fast Tape Interpreter

A blazingly fast, optimizing interpreter for Brainfuck-like esoteric language with a modern command set and Ruby-inspired syntax.

## Features

- **Fast**: Compiled with `-O3 -march=native`, aggressive optimization passes at compile-time
- **Modern Syntax**: Readable command names (no `>` and `<`, no `+` and `-`)
- **Loop Syntax**: Ruby-inspired `repeat N ... end` blocks instead of `[` and `]`
- **Compile-time Optimization**: Multi-pass peephole optimizer that:
  - Merges consecutive operations (`add 3; add 5` → `add 8`)
  - Multiplies simple ops inside loops (`repeat 10 add; end` → `add 10`)
  - Eliminates dead code (`set 10; set 20` → `set 20`)
  - Constant folding

## Building

```bash
make clean && make
```

Requires: g++ with C++23 support (recent GCC/Clang)

## Usage

### Command-line

Run inline code:
```bash
./yascript -e "repeat 72 add; end; output;"
```

Run from a `.ys` file:
```bash
./yascript hello.ys
```

### Commands
|--------------|---------------|------------------------------------------|
| Command      | Syntax        | Description                              |
|--------------|---------------|------------------------------------------|
| Move left    | `left [N]`    | Move pointer N cells left (default 1)    |
| Move right   | `rght [N]`    | Move pointer N cells right (default 1)   |
| Increment    | `add [N]`     | Add N to current cell (default 1)        |
| Decrement    | `sub [N]`     | Subtract N from current cell (default 1) |
| Set value    | `set N`       | Set current cell to N                    |
| Direct Seek  | `goto TARGET` | Seek tape pointer directly to cell TARGET|
| Output       | `output`      | Output current cell as ASCII             |
| Input        | `read`        | Read one byte to current cell            |
| Print number | `print`       | Output current cell as decimal number    |
| Zero cell    | `zero`        | Set current cell to 0                    |
| Loop start   | `repeat N`    | Start loop, repeat N times               |
| Loop end     | `end`         | End loop block                           |
|--------------|---------------|------------------------------------------|
## Examples

### Hello World (`hello.ys`)

```yascript
# Print "Hello World"
repeat 72 add; end   # Cell 0 = 72 ('H')
output;
rght;                # Move to cell 1
repeat 101 add; end  # Cell 1 = 101 ('e')
output;
rght;
repeat 108 add; end  # Cell 2 = 108 ('l')
output;
output;              # 'l' again
rght;
repeat 111 add; end  # Cell 3 = 111 ('o')
output;
rght;
repeat 32 add; end   # Cell 4 = 32 (' ')
output;
rght;
repeat 87 add; end   # Cell 5 = 87 ('W')
output;
rght;
repeat 111 add; end  # Cell 6 = 111 ('o')
output;
rght;
repeat 114 add; end  # Cell 7 = 114 ('r')
output;
rght;
repeat 108 add; end  # Cell 8 = 108 ('l')
output;
rght;
repeat 100 add; end  # Cell 9 = 100 ('d')
output;
```

Run with: `./yascript hello.ys`

### Simple Arithmetic

```yascript
set 100
sub 50
output;  # Outputs character code 50 (ASCII '2')
```

### Loop Optimization

The optimizer recognizes `repeat N op; end` and multiplies the operation:

```yascript
# Before optimization: 10 adds
repeat 10 add; end

# After optimization: automatically becomes one `add 10` instruction
```

### Dead Code Elimination

```yascript
set 10
set 20  # Previous set is eliminated by optimizer
```

## Error Messages

yascript reports parse and runtime errors with source locations and visual indicators pointing directly to the offending code:

```bash
$ ./yascript -e "add print;"
parse error: missing statement separator before 'print' at line 1, column 5; end simple statements with ';' or a newline
 1 | add print;
   |     ^^^^^

$ ./yascript examples/error_pointer_underflow.ys
runtime error: pointer underflow at instruction 0 at line 2, column 1: left 1 from cell 0
 2 | left 1;
   | ^^^^
```

Common diagnostics include missing statement separators, missing numeric arguments, out-of-range numbers, pointer underflow/overflow, value underflow/overflow, and tape growth failures.

## Language Properties

- **Tape**: Unlimited dynamically-expanding tape (zero-initialized)
- **Cells**: 64-bit unsigned integers (uint64_t)
- **Comments**: `#` single-line comments, `//` also supported
- **Whitespace**: Ignored except for newline separators between statements
- **Extension**: Must use `.ys` file extension for file input

## Optimizations

The interpreter performs aggressive compile-time optimizations:

- **Operand merging**: `add 3; add 5` → `add 8`
- **Loop unwrapping**: `repeat 10 add; end` → `add 10`
- **Dead code elimination**: `set 10; set 20` → `set 20`
- **Constant folding**: `set X; add Y` → `set (X+Y)`
- **Buffered output**: 4KB output buffer reduces I/O syscalls

## Performance

The interpreter uses several techniques for fast execution:

1. **Token-based lexer**: Efficient tokenization with line/column tracking
2. **Compile-time optimization**: Multi-pass peephole optimizer runs during parsing
3. **Buffered output**: 4KB output buffer to reduce I/O syscalls
4. **Direct execution**: Executes optimized instructions directly without interpretation overhead
5. **Native compilation**: Compiled with `-march=native` for CPU-specific optimizations

## Architecture

```
Source Code
    ↓
  Lexer    (tokenization, line/column tracking)
    ↓
 Parser    (syntax analysis + multi-pass optimizer)
    ↓
 Runner    (instruction execution with buffering)
    ↓
 Output
```

## Build

Compile with:

```bash
make
```

## Quick Start

Create a file `test.ys`:

```yascript
# Simple program
set 65
output;
```

Run it:

```bash
./yascript test.ys
```

Output: `A`

## Notes

- Commands are whitespace-separated.
- Comments begin with `#` or `//` and extend to the end of the line.
- Simple statements end with `;` or a newline.
- Loops with `repeat` execute exactly `n` times and terminate with `end`.
- `;` and newlines are both accepted as statement separators inside blocks.
- All cells are 64-bit unsigned integers, can't underflow (sub will error).
- The tape dynamically expands as needed.
