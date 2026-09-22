# Dorothy Language Support for Visual Studio Code

Visual Studio Code extension providing syntax highlighting, bracket matching, indentation rules, and language configurations for the **Dorothy** programming language (`.dorothy`).

---

## Features

- **Syntax Highlighting**:
  - Keywords (`if`, `else`, `while`, `for`, `return`, `var`, `let`, `func`, `struct`, `class`, `abstract`, `override`, `constructor`, `import`, `null`, `this`)
  - Primitive Types (`int`, `long`, `char`, `float`, `double`, `string`, `String`) & User-defined Classes/Structs
  - String & Character Literals with escape sequence formatting (`\n`, `\t`, `\r`, `\"`, `\\`, etc.)
  - Function Declarations, Constructors, Method Invocations
  - Class inheritance and Type annotations (`: Type`, `-> Type`, `Type?`, `Type[N]`)
  - Arithmetic, Logical, Comparison, Null-safe, and Arrow operators (`->`, `==`, `!=`, `<=`, `>=`, `?`, etc.)
  - C Library (`printf`, `puts`, `print_int`, etc.) and SDL2 / Glinda Framework built-in symbols
- **Code Snippets & Autocompletion**:
  - **Imports**: `import`, `import-stdio`, `import-string`, `import-sdl2`, `import-glinda`, `import-dorothy`
  - **Glinda Framework**: `glinda-app`, `glinda-component`, `glinda-entity`, `glinda-get-component`, `glinda-draw-rect`
  - **SDL2 Library**: `sdl-init`, `sdl-create-window-renderer`, `sdl-poll-event`, `sdl-keyboard-state`, `sdl-set-color`, `sdl-clear`, `sdl-present`, `sdl-delay`
  - **C Standard Library**: `printf`, `puts`, `strlen`, `strcmp`, `malloc`, `free`, `exit`
  - **Language Constructs**: `main`, `func`, `func-return`, `let`, `var`, `class`, `class-extends`, `abstract-class`, `struct`, `override`, `if`, `ifelse`, `while`, `for`
- **Language Configurations**:
  - Comment toggling (`//` for line comment, `/* ... */` for block comment)
  - Auto-closing pairs and surrounding pairs for brackets `{}`, `[]`, `()`, and quotes `""`, `''`
  - Smart indentation rules for braces `{ ... }`

---

## Installation

### Option 1: Direct Link to VS Code Extensions (Recommended for Local Dev)

Run the following command from the root of the `dorothy` repository:

```sh
make vscode
```

Or manually create a symlink / copy to your VS Code extensions folder:

```sh
# macOS / Linux
mkdir -p ~/.vscode/extensions
ln -sfn "$(pwd)/editors/vscode" ~/.vscode/extensions/dorothy-vscode

# Windows (Command Prompt as Admin or PowerShell)
# cmd /c mklink /D "%USERPROFILE%\.vscode\extensions\dorothy-vscode" "%CD%\editors\vscode"
```

After creating the link, reload or restart Visual Studio Code (`Cmd+Shift+P` -> `Developer: Reload Window`).

### Option 2: Package with `vsce`

If you have `@vscode/vsce` installed:

```sh
cd editors/vscode
npx @vscode/vsce package
code --install-extension dorothy-vscode-0.1.0.vsix
```

---

## License

MIT License
