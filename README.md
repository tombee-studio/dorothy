# Dorothy

A simple programming language that compiles to a custom bytecode VM or LLVM IR.

## Prerequisites

- g++ (C++17 or later)
- clang (for LLVM IR compilation)
- make

## Installation

Clone this repository and run:

```sh
git clone https://github.com/tombee-studio/dorothy.git
cd dorothy
mkdir -p Lib obj dist
make cli
```

`make cli` builds the library, links the CLI binary, and installs it to `/usr/local/bin/dorothy`.

To build without installing:

```sh
make lib
g++ --std=c++17 -o dist/dorothy cli/main.cpp Lib/libdorothy.a
```

## Usage

### Run on the built-in VM

```sh
dorothy script.txt
```

The exit code of the process is the return value of `main()`.

### Emit LLVM IR

```sh
dorothy --emit-llvm script.txt
```

Outputs LLVM IR to stdout. Pipe it to clang to produce a native binary:

```sh
dorothy --emit-llvm script.txt > out.ll
clang -o program out.ll
./program
```

## Language Reference

### Hello World equivalent

The language has no string output built-in; use `import put` and `import nl`:

```
import put;
import nl;

func main() {
    put(72);  put(101); put(108); put(108); put(111);
    nl();
    return 0;
}
```

### Variables

All variables are integers (`int`).

```
int a;
a = 42;
```

Pointer operations using `&` and `*`:

```
int a;
int b;
b = &a;
*b = 10;
```

### Arrays

Fixed-size integer arrays:

```
int arr[5];
arr[0] = 1;
arr[1] = 2;
```

Initialized at declaration:

```
int arr[5] = {1, 2, 3, 4, 5};
```

String literals expand to character arrays with a null terminator:

```
int str[14] = "Hello, World!";
```

### Control Flow

```
if (a > 0) {
    return a;
} else {
    return 0;
}

while (i < 10) {
    i = i + 1;
}

for (i = 0; i < 10; i = i + 1) {
    arr[i] = i;
}
```

### Functions

```
func add(int a, int b) {
    return a + b;
}

func main() {
    return add(3, 4);
}
```

Recursive functions work:

```
func fibonacci(int a) {
    if (a > 1) {
        return a + fibonacci(a - 1);
    } else {
        return 1;
    }
}

func main() {
    return fibonacci(10);
}
```

### Importing built-in functions

```
import print;
import put;
import nl;
```

| Function | Description |
|----------|-------------|
| `print(n)` | Print an integer value |
| `put(c)` | Print a character (integer as ASCII) |
| `nl()` | Print a newline |

### Operators

| Category | Operators |
|----------|-----------|
| Arithmetic | `+` `-` `*` `/` `%` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Assignment | `=` |
| Address / Dereference | `&` `*` |

## Examples

`script/test001.txt` — simple arithmetic:

```
func main() {
    return 1 + 2;
}
```

```sh
dorothy script/test001.txt; echo $?   # 3
```

`script/test002.txt` — recursive fibonacci:

```sh
dorothy --emit-llvm script/test002.txt | clang -x ir - -o fib
./fib; echo $?   # 55
```

`script/test003.txt` — arrays and for loop:

```sh
dorothy script/test003.txt; echo $?   # 3
```

## Project Structure

```
dorothy/
├── cli/        # CLI entry point (main.cpp)
├── include/    # Headers (lexer, parser, AST, codegen)
├── src/        # Implementation (lexer, parser, AST, VM, LLVM codegen)
├── script/     # Example programs
├── unittest/   # Unit tests
├── Lib/        # Built static library
├── obj/        # Object files
└── dist/       # Built binary
```

## Authors

- **tomoya** — [tombee-studio](https://github.com/tombee-studio/)

## License

MIT License
