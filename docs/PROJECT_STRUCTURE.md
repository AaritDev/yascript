# Project Structure

yascript is now a general-purpose scripting language that compiles Ruby/Python-inspired syntax to a high-performance tape VM. The low-level tape commands are still supported, and the newer scripting layer lowers variables, expressions, conditionals, and loops into VM instructions.

## Directory Organization

```
yascript/
├── src/                          # Implementation files
│   ├── yascript-interface.cpp    # CLI entry points and source/file execution
│   ├── yascript-lexer.cpp        # Zero-copy lexer over string_view
│   ├── yascript-parser.cpp       # Parser, symbol table, lowering, optimizer
│   └── yascript-runner.cpp       # Tape VM with threaded dispatch and switch fallback
│
├── include/                      # Public project headers
│   ├── yascript-diagnostics.hpp  # Caret-style diagnostic formatting
│   ├── yascript-interface.hpp
│   ├── yascript-lexer.hpp
│   ├── yascript-parser.hpp
│   └── yascript-runner.hpp
│
├── bin/                          # Built executable
│   └── yascript
│
├── build/                        # Object files and dependency files
│   ├── *.o
│   └── *.d
│
├── examples/                     # Example .ys programs and error fixtures
│   ├── hello.ys
│   ├── simple.ys
│   ├── optimizations.ys
│   ├── goto_persistence.ys
│   └── error_*.ys
│
├── tests/                        # Shell test suites
│   ├── run_tests.sh              # Core tape command and optimizer tests
│   └── run_scripting_tests.sh    # Variables, expressions, if/else, while tests
│
├── docs/                         # Documentation
│   ├── OPTIMIZATIONS.md
│   ├── PROJECT_STRUCTURE.md
│   └── assets/
│
├── Makefile                      # Build, test, install, uninstall targets
├── README.md                     # User-facing overview and language guide
├── install.sh
└── uninstall.sh
```

## Execution Pipeline

1. `yascript-interface.cpp` reads inline source from `-e` or loads a `.ys` file.
2. `yascript-lexer.cpp` tokenizes commands, identifiers, numeric literals, operators, parentheses, separators, and block keywords.
3. `yascript-parser.cpp` parses statements and expressions, resolves variables/constants through a compile-time symbol table, and emits VM instructions.
4. The parser back-patches repeat, if/else, and while jump targets.
5. The optimizer runs multi-pass peephole rewrites and remaps control-flow targets after instruction removal or folding.
6. `yascript-runner.cpp` executes the program on a dynamically growing `uint64_t` tape.
7. Output is buffered and flushed at program end or before runtime errors.

## Language Layers

### Scripting Layer

The scripting layer adds:

- `let` declarations with optional initializers
- `const` declarations with required initializers
- Assignment to existing variables
- Arithmetic expressions with `+`, `-`, `*`, `/`, `%`, and parentheses
- Comparisons with `==`, `!=`, `<`, `<=`, `>`, `>=`
- `if`, `else`, and `while` blocks
- `print expr`, `output expr`, and `read variable`

Variables and constants are compile-time aliases for tape cells. Expression temporaries are allocated from cells above the declared-variable range while parsing.

### Low-Level Tape Layer

The original tape commands remain supported:

- Pointer movement: `left`, `rght`, `goto`
- Cell mutation: `add`, `sub`, `set`, `zero`
- I/O: `output`, `print`, `read`
- Counted blocks: `repeat N ... end`

Simple statements require a semicolon or newline separator. Block statements can end at `end` without a trailing separator.

## VM Instruction Groups

### Tape Instructions

`Left`, `Right`, `Add`, `Sub`, `Set`, `Goto`, `Output`, `Read`, `Print`, `Zero`, `RepeatStart`, and `RepeatEnd` implement the low-level language.

### Scripting Instructions

`Copy`, `AddVar`, `SubVar`, `MulVar`, `DivVar`, `ModVar`, comparison opcodes, `Jump`, `JumpIfFalse`, `PrintVar`, `OutputVar`, and `ReadVar` implement the scripting layer.

The runner pre-sizes the tape based on explicit `goto` targets and all cell operands used by variable instructions. It grows the tape dynamically if later instructions need larger cells.

## Build System

The Makefile uses:

- C++23
- `-O3`
- `-Wall -Wextra`
- `-fno-exceptions`
- `-flto`
- `-pipe`
- `-Iinclude`
- Optional `-march=native` with `make NATIVE=1`

Common commands:

```bash
make clean
make
make NATIVE=1
make test
sudo make install
sudo make uninstall
```

## Testing

The core test suite covers tape commands, existing optimizer behavior, separators, diagnostics, overflow/underflow handling, and goto correctness:

```bash
make test
```

The scripting test suite covers declarations, assignment, expression precedence, parenthesized expressions, variable I/O, comparisons, `if`/`else`, `while`, and scripting-layer runtime errors:

```bash
./tests/run_scripting_tests.sh
```

## Current Optimization Coverage

The optimizer still focuses on low-level tape instructions and control-flow target maintenance:

- Repeat unwrapping and repeat body folding
- Empty repeat elimination
- Consecutive arithmetic and pointer operation merging
- Constant folding for `set` plus `add`/`sub`
- Dead overwrite elimination for `set`/`zero`
- Zero/add rewrites
- Dead pointer-shift removal before `goto`
- Consecutive `goto` elimination
- Repeat and jump target remapping after optimization

## Future Enhancement Opportunities

- Constant folding for scripting expressions
- Overflow checks for `AddVar`
- Dead temporary elimination
- Scope-aware symbol tables
- More complete diagnostics for likely misspelled identifiers
- Integrating `run_scripting_tests.sh` into `make test`
