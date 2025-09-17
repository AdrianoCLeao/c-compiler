# C Compiler Driver Manual

This project includes a CLI driver that runs and inspects different stages of the compilation pipeline for simple C programs.

The driver can lex, parse, generate an intermediate representation (TACKY), produce assembly, build an executable via the system C compiler (cc), and dump artifacts to files for inspection.

## Build

- Build: `make`
- Run: `make run ARGS="<flags> <source.c>"`
- Help: `make help`

The compiled binary is at `bin/main.exe` (invoked as `./bin/main.exe` on Unix-like systems).

## Usage

```
./bin/main.exe [--lex | --parse | --validate | --tacky | --codegen] [-S] \
  [--dump-tokens[=<path>]] \
  [--dump-ast[=txt|dot|json] [--dump-ast-path=<path>]] \
  [--dump-tacky[=txt|json] [--dump-tacky-path=<path>]] \
  [--quiet] [--run] [--help|-h] <source.c>
```

### Stages (choose at most one)

- `--lex`: Run lexer only (no files written).
- `--parse`: Run lexer + parser (no files written).
- `--validate`: Run lexer + parser + semantic validation (no files written).
- `--tacky`: Run up to TACKY IR generation (no files written).
- `--codegen`: Run up to assembly IR generation (no emission).

If no stage flag is provided, the full pipeline runs (lex → parse → TACKY → assembly), prints AST and assembly to stdout unless `--quiet` is set, and then assembles+links by piping the assembly into `cc` to produce an executable.

### Emission

- `-S`: Emit an assembly `.s` file next to the source (no assembling or linking). Example: `examples/neg.c` → `examples/neg.s`.
  - Note: When any partial stage flag is used (`--lex`, `--parse`, `--tacky`, `--codegen`), `-S` is ignored.
  - Without `-S`, the default full pipeline assembles+links via `cc` using a pipe (no intermediate `.s` file).

### Running

- `--run`: After building the executable (full pipeline), run it and print the exit code, even if non‑zero.

### Dumpers

Dumpers write files under `./out` by default, using the input basename.

- `--dump-tokens[=<path>]`: Dump token stream to `<path>` or `out/<name>.tokens`.
- `--dump-ast[=fmt]`: Dump AST in the chosen format.
  - Formats: `txt` (default), `dot`, `json`.
  - `--dump-ast-path=<path>`: Override AST dump path.
- `--dump-tacky[=fmt]`: Dump TACKY IR in the chosen format.
  - Formats: `txt` (default), `json`.
  - `--dump-tacky-path=<path>`: Override TACKY dump path.

The `out/` folder is created automatically if needed.

### Output Control

- `--quiet`: Suppress stdout prints for AST/assembly during full builds.
- `--help`, `-h`: Show help and exit.

### Notes

- Only one stage flag may be provided.
- Exactly one `source.c` file must be provided.
- Partial stages do not write files unless an explicit dumper flag is used.

## Examples

- Full pipeline, printing AST and assembly, and producing an executable:
  - `./bin/main.exe examples/neg.c`

- Lex only:
  - `./bin/main.exe --lex examples/neg.c`

- Parse and dump AST to DOT under `out/`:
  - `./bin/main.exe --parse --dump-ast=dot examples/neg.c`

- Generate TACKY and dump to JSON:
  - `./bin/main.exe --tacky --dump-tacky=json examples/neg.c`

- Full pipeline but quiet, and also emit `.s` next to the source (no assemble/link):
  - `./bin/main.exe -S --quiet examples/neg.c`

- Full pipeline and run the program, printing its exit code:
  - `./bin/main.exe --run examples/chapter_4/valid1.c`

Tip: In any shell, you can manually check a program's exit code using `echo $?` right after running it, e.g., `./a.out; echo $?`.

## Notes on Assembling/Linking

- The driver invokes the system C compiler (`cc`/`clang`/`gcc`) to assemble and link, which keeps platform-specific flags and startup objects handled by the toolchain.
- On Linux, non-PIE linking is requested (`-no-pie`) to simplify execution of the produced binaries.
- On macOS, assembly symbol names are emitted with an underscore prefix (e.g., `_main`) to match Mach-O conventions.
- On Apple Silicon (arm64) hosts, the driver passes `-arch x86_64` to `cc` because the current backend emits x86_64 AT&T assembly.

## Default Output Paths

- Tokens: `out/<basename>.tokens`
- AST: `out/<basename>.ast.txt` | `out/<basename>.ast.dot` | `out/<basename>.ast.json`
- TACKY: `out/<basename>.tacky.txt` | `out/<basename>.tacky.json`

To override a dumper path, use the corresponding `--*-path=` option.
