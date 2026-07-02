# yascript

yascript is a minimal tape-based scripting language inspired by Brainfuck, but much simpler and easier to read.
It uses named commands to move a tape pointer, modify values, and print output. The tape is unbounded to the right and stores unsigned values.

## Language overview

The current tape position is called the pointer. It starts at `0`. The tape grows automatically to the right when needed.

Available commands:

- `left` — move the pointer one cell left
- `rght` — move the pointer one cell right
- `add` — increment the current cell
- `min` — decrement the current cell
- `set <n>` — set the current cell to a non-negative integer
- `goto <n>` — move the pointer directly to a specific cell
- `output` — print ASCII characters from the current cell through the last non-zero cell to the right
- `print` — print the current cell as a single ASCII character
- `read` — read one byte from stdin into the current cell
- `zero` — set the current cell to `0`
- `loop <n>` — repeat the block from `loop <n>` until `end` exactly `n` times
- `end` — end a counted loop block

## Runtime rules

- The tape extends automatically to the right.
- Cells hold unsigned values and cannot go below `0`.
- `left` from position `0` is a runtime error.
- `min` on a zero-valued cell is a runtime error.
- `goto` accepts only non-negative positions.

## Build

Compile with a C++ compiler:

```bash
g++ -std=c++23 yascript-interface.cpp yascript-parser.cpp yascript-runner.cpp -o yascript
```

Or use the included `Makefile` if available:

```bash
make
```

## File format

`yascript` only accepts source files with the `.ys` extension when loading files directly. Example:

```bash
./yascript hello.ys
```

Inline execution with `-e` still works for quick testing.

## Example: Hello World

This example uses loop blocks to set ASCII values and output the text.

```text
loop 72 add end
rght loop 101 add end
rght loop 108 add end
rght loop 108 add end
rght loop 111 add end
rght loop 32 add end
rght loop 87 add end
rght loop 111 add end
rght loop 114 add end
rght loop 108 add end
rght loop 100 add end
goto 0
output
```

Run it directly:

```bash
./yascript -e "loop 72 add end rght loop 101 add end rght loop 108 add end rght loop 108 add end rght loop 111 add end rght loop 32 add end rght loop 87 add end rght loop 111 add end rght loop 114 add end rght loop 108 add end rght loop 100 add end goto 0 output"
```

Or save it as `hello.ys` and run:

```bash
./yascript hello.ys
```

## Notes

- Commands are whitespace-separated.
- Comments begin with `#` or `//` and extend to the end of the line.
- Loops are counted and simple: the body executes exactly `n` times.
