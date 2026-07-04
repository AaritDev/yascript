# yascript - Fast Tape Interpreter

A blazingly fast interpreter for Brainfuck-like esoteric language with a modern command set and Ruby-inspired syntax.

## Features

- **Fast**: Optimized build (`-O3`), optional CPU-specific optimization via `make NATIVE=1`
- **Modern Syntax**: Readable command names (no `>` and `<`, no `+` and `-`)
- **Loop Syntax**: Ruby-inspired `repeat N ... end` blocks instead of `[` and `]`
- **Optimization Pipeline**: Multi-pass optimizer during parsing/execution that:
  - Merges consecutive operations (`add 3; add 5` → `add 8`)
  - Collapses loop effects where safe (`repeat 10 add; end` → `add 10`)
  - Eliminates redundant state changes (`set 10; set 20` → `set 20`)
  - Performs limited constant folding where semantics are preserved

## Building

```bash
make
```

Optional CPU-optimized build:

```bash
make NATIVE=1
```

Run tests:

```bash
make test
```

Install:

```bash
sudo make install
```

Uninstall:

```bash
sudo make uninstall
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

## Examples

### Hello World

```yascript
repeat 72 add; end
output;
rght;

repeat 101 add; end
output;
```

## Language Properties

- Tape: Unlimited dynamically-expanding tape (zero-initialized)
- Cells: 64-bit unsigned integers (uint64_t)
- Comments: `#` and `//`
- Extension: `.ys`

## Performance

1. Token-based lexer
2. Multi-pass optimization
3. Buffered output (4KB)
4. Direct execution of optimized instructions
5. Optional CPU-specific optimization via `make NATIVE=1`

## Build

```bash
make
```

## Quick Start

```bash
./yascript test.ys
```

Output: `A`

## Install Scripts

```bash
./install.sh
./uninstall.sh
