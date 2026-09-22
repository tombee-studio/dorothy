# Dorothy

A modern programming language that compiles to a custom bytecode VM or LLVM IR, featuring strong typing, Null-safety, reference-counted object lifecycle (ARC), and the Glinda ECS Game Framework.

## Prerequisites

- g++ (C++17 or later)
- clang (for LLVM IR compilation)
- make
- sdl2 (optional, required for Glinda Framework / 2D games)

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

### Emit LLVM IR & Compile to Native Binary

```sh
dorothy --emit-llvm script.txt > out.ll
clang -o program out.ll
./program
```

Or run directly in a pipeline:

```sh
dorothy --emit-llvm script.txt | clang -x ir - -o program
./program
```

---

## Language Reference

### Hello World

Dorothy supports standard C library functions (such as `puts`, `printf`) and native `string` types:

```dorothy
import "stdio.h";

func main() {
    let msg: string = "Hello, Dorothy!";
    puts(msg);
    return 0;
}
```

### Variables and Constants (`let` and `var`)

Dorothy distinguishes between mutable variables (`var`) and immutable constants (`let`).

#### Constants (`let`)
Constants must be initialized when declared and cannot be reassigned:

```dorothy
let maxUsers: int = 100;
let greeting = "Hello";  // Type inferred as string

maxUsers = 200; // Compile Error: cannot assign to constant: maxUsers
```

#### Variables (`var`)
Variables can be reassigned:

```dorothy
var count: int = 0;
count = count + 1;
```

#### Type Inference
Type annotations can be omitted when an initializer expression is provided:

```dorothy
let a = 42;             // inferred as long / int
let msg = "Dorothy";    // inferred as string
var pi = 3.14159;       // inferred as double
```

---

### Primitive Types

| Type | Size | LLVM IR | Description |
|------|------|---------|-------------|
| `char` | 1 byte | `i8` | 8-bit integer |
| `int` | 4 bytes | `i32` | 32-bit integer |
| `long` | 8 bytes | `i64` | 64-bit integer |
| `float` | 4 bytes | `float` | Single-precision floating point |
| `double` | 8 bytes | `double` | Double-precision floating point |
| `string` | Pointer | `ptr` | Dynamically allocated null-terminated string |
| `Array<T>` / `T[]` | Pointer | `ptr` | Dynamic heap-allocated array with element type `T` |

#### Strings (`string`)
Dorothy supports native heap-allocated string operations:

- **String Literals**: `"Hello, World!"`
- **Concatenation (`+`)**: Chained string concatenation with dynamic memory allocation
- **Comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`)**: Lexicographical comparison

```dorothy
import "stdio.h";

func main() {
    let s1: string = "Hello, ";
    let s2: string = "World!";
    let full: string = s1 + s2; // "Hello, World!"

    if (full == "Hello, World!") {
        printf("%s\n", full);
    }
    return 0;
}
```

#### Floating-Point & Mixed Arithmetic
Mixed integer and float arithmetic automatically widens to `double`:

```dorothy
func main() {
    let n: int = 3;
    let result: double = n + 1.5;   // 4.5
    return 0;
}
```

---

### Null-Safety

Dorothy provides compile-time and runtime null-safety guarantees.

- **Non-nullable by default**: Variables of class types cannot be assigned `null` or left uninitialized.
- **Nullable types (`T?`)**: Append `?` to class type names to permit `null`.
- **Runtime Guard**: Attempting to access members on a `null` reference safely halts execution with a `NullPointerException`.

```dorothy
class Player {
    var name: string;
    func Player(n: string) {
        this.name = n;
    }
}

func main() {
    var p1: Player = Player("Alice"); // Non-nullable, must be initialized
    // p1 = null;                     // Compile Error!

    var p2: Player? = null;           // Nullable type allows null
    p2 = Player("Bob");
    p2 = null;                        // Reassignment to null allowed

    // p2.name;                       // Triggers NullPointerException if null
}
```

---

### Classes & Object-Oriented Programming

Dorothy provides a full class-based OOP system with Automatic Reference Counting (ARC) memory management:

- **Classes & Constructors**: `class Name { ... func Name(...) { ... } }`
- **Inheritance & Polymorphism**: `class Dog: Animal { override func speak() { ... } }`
- **Abstract Classes**: `abstract class Shape { abstract func area() -> int; }`
- **Reference Semantics**: Objects are passed by reference and automatically cleaned up when references reach zero.

```dorothy
import "stdio.h";

abstract class Animal {
    abstract func speak();
}

class Dog: Animal {
    var name: string;
    func Dog(name: string) {
        this.name = name;
    }

    override func speak() {
        printf("%s says: Woof!\n", this.name);
    }
}

func main() {
    let dog: Animal = Dog("Pochi");
    dog.speak();
    return 0;
}
```

---

### Structures (`struct`)

Value-type structures with custom constructors:

```dorothy
struct Point {
    var x: int;
    var y: int;

    constructor(px: int, py: int) {
        this.x = px;
        this.y = py;
    }
}

func main() {
    let p: Point = Point(10, 20);
    return 0;
}
```

---

### Dynamic Arrays & Generics (`Array<T>` / `T[]`)

Dorothy supports dynamically sized arrays with generic type parameters (`Array<T>`) or shorthand array type syntax (`T[]`):

- **Type Annotations**: `int[]`, `string[]`, `Array<int>`, `Array<string>`, `Array<Player>`
- **Array Literals**: `[]`, `[1, 2, 3]`, `["Alice", "Bob"]`
- **Dynamic Resizing**: Arrays automatically expand on heap memory as elements are added.
- **Methods and Properties**:
  - `arr.push(elem)`: Appends an element to the end of the array.
  - `arr.remove(elem)`: Removes the first matching element (value equality for primitive types, instance reference equality for objects). Returns `1` if an element was removed, `0` otherwise.
  - `arr[index]`: Reads or writes (`arr[index] = val`) the element at `index` (0-indexed). Bounds checking is enforced at runtime, raising an `IndexOutOfBoundsException` if violated.
  - `arr.length` / `arr.size()`: Returns the current number of elements in the array (`long`).

```dorothy
import "stdio.h";

class Player {
    var name: string;
    func Player(n: string) {
        this.name = n;
    }
}

func main() -> int {
    // Array creation with literal
    var numbers: int[] = [10, 20, 30];
    numbers.push(40);

    // Modify element by index
    numbers[0] = 100;

    // Loop through elements
    var i: int = 0;
    for (i = 0; i < numbers.length; i = i + 1) {
        printf("%d\n", numbers[i]);
    }

    // Generic object array
    var p1: Player = Player("Alice");
    var p2: Player = Player("Bob");
    var team: Array<Player> = [p1, p2];

    team.remove(p1); // Removes by reference equality
    printf("Remaining players: %ld\n", team.length);

    return 0;
}
```

---

### Control Flow

```dorothy
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

---

## Glinda Framework

Glinda is an Entity-Component System (ECS) framework for Dorothy that enables building 2D games and interactive graphical applications using SDL2.

### Prerequisites & Installation

To run Glinda applications, install SDL2 and pkg-config:

```sh
# macOS (Homebrew)
brew install sdl2 pkg-config

# Ubuntu / Debian
sudo apt-get install -y libsdl2-dev pkg-config

# Fedora / RHEL
sudo dnf install -y SDL2-devel pkgconf-pkg-config
```

### Running Examples

To compile and run the provided Breakout game example:

```sh
# 1. Emit LLVM IR and compile with Clang linking SDL2
dorothy --emit-llvm example/breakout.dorothy | clang -x ir - -o breakout $(pkg-config --cflags --libs sdl2)

# 2. Run the executable
./breakout
```

Or run the ECS feature demo:

```sh
dorothy --emit-llvm example/glinda_demo.dorothy | clang -x ir - -o glinda_demo $(pkg-config --cflags --libs sdl2)
./glinda_demo
```
---

## Editor Support (VS Code)

Dorothy provides syntax highlighting and language support for Visual Studio Code.

### Installation

To install the extension locally:

```sh
make vscode
```

Or manually link the extension:

```sh
mkdir -p ~/.vscode/extensions
ln -sfn "$(pwd)/editors/vscode" ~/.vscode/extensions/dorothy-vscode
```

After installation, reload VS Code (`Cmd+Shift+P` -> `Developer: Reload Window`).

---

## Project Structure

```
dorothy/
├── cli/        # CLI entry point (main.cpp)
├── editors/    # Editor support extensions (VS Code syntax highlighting)
├── frameworks/ # High-level frameworks (e.g., Glinda ECS framework)
├── include/    # Headers (lexer, parser, AST, codegen, typechecker)
├── src/        # Implementation (lexer, parser, AST, VM, LLVM codegen, typechecker)
├── example/    # Example programs (Breakout, Glinda demo, etc.)
├── tests/      # Unit & integration tests (Google Test)
├── Lib/        # Built static library
├── obj/        # Object files
└── dist/       # Built binary
```

## Authors

- **tomoya** — [tombee-studio](https://github.com/tombee-studio/)

## License

MIT License

