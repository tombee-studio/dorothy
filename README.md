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

### Primitive Types

| Type | Size | Description |
|------|------|-------------|
| `char` | 1 byte | Integer (LLVM: `i8`) |
| `int` | 4 bytes | Integer (LLVM: `i32`) |
| `long` | 8 bytes | Integer (LLVM: `i64`) |
| `float` | 4 bytes | Floating point (LLVM: `float`) |
| `double` | 8 bytes | Double precision float (LLVM: `double`) |

```
char  c;
int   i;
long  l;
float f;
double d;

c = 65;
i = 100;
l = 1000000;
f = 3.14;
d = 2.71828;
```

Floating point literals use decimal notation (`3.14`, `1.0`):

```
func main() {
    float x;
    float y;
    float z;
    x = 1.5;
    y = 2.5;
    z = x + y;   // 4.0 (float arithmetic)
    return 0;
}
```

Mixed integer/float arithmetic promotes to `double`:

```
func main() {
    int   n;
    double result;
    n = 3;
    result = n + 1.5;   // int promoted to double → 4.5
    return 0;
}
```

Pointer operations using `&` and `*`:

```
int a;
int b;
b = &a;
*b = 10;
```

### Arrays

Fixed-size arrays (element type follows the type keyword):

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

Function parameters use the same type keywords as variables:

```
func add(int a, int b) {
    return a + b;
}

func main() {
    return add(3, 4);   // 7
}
```

`char` and `long` parameters:

```
func is_upper(char c) {
    if (c >= 65) {
        if (c <= 90) { return 1; }
    }
    return 0;
}

func main() {
    return is_upper(65);   // 1 ('A')
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
    return fibonacci(10);   // 55
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

### Primitive types sample

All five primitive types in one program:

```
func main() {
    char  c;
    int   i;
    long  l;
    float f;
    double d;

    c = 65;
    i = 100;
    l = 1000000;
    f = 3.14;
    d = 2.71828;

    return 0;
}
```

Run on the VM:

```sh
dorothy types.txt; echo $?   # 0
```

Emit LLVM IR to inspect generated types:

```sh
dorothy --emit-llvm types.txt
```

Expected IR (excerpt):

```llvm
%c.addr.0 = alloca i8
%i.addr.1 = alloca i32
%l.addr.2 = alloca i64
%f.addr.3 = alloca float
%d.addr.4 = alloca double
```

### Float arithmetic sample

```
func circle_area(float r) {
    float pi;
    float area;
    pi   = 3.14159;
    area = pi * r * r;
    return 0;
}

func main() {
    return circle_area(5.0);
}
```

```sh
dorothy --emit-llvm circle.txt
# → fmul double instructions for float arithmetic
```

### char range check

```
func is_digit(char c) {
    if (c >= 48) {
        if (c <= 57) { return 1; }
    }
    return 0;
}

func main() {
    return is_digit(51);   // '3' → 1
}
```

```sh
dorothy digit.txt; echo $?   # 1
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
