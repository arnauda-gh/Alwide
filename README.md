# Alwide — A LightWeight IDE

> **"Sublime Text" in the terminal.** Alwide is a fast, powerful, and user-friendly TUI IDE. It aims to provide the same
> user experience as a graphical IDE, but right in your terminal. Need an easy editor over a simple SSH connection?
> Looking for something lighter than VS Code or the JetBrains suite? Or is Vim sometimes too rough for quick actions?


[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C](https://img.shields.io/badge/language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Tree-Sitter](https://img.shields.io/badge/highlighting-Tree--Sitter-green.svg)](https://tree-sitter.github.io/tree-sitter/)
[![LSP](https://img.shields.io/badge/intelligence-LSP-yellow.svg)](https://microsoft.github.io/language-server-protocol/)

<p align="center">
  <img width="700" alt="Alwide Screenshot" src="https://github.com/user-attachments/assets/f73b961f-0fc2-4c0a-81b6-3e391a96031b" />
</p>

https://github.com/user-attachments/assets/c6f40db1-bc5e-4c90-88a5-c7e5a5c72059



---

## The Modern Terminal Experience

Alwide is designed for users who want more than `nano` but find `vim` or `emacs` too complex or rusty. It’s the perfect
companion for everything from editing quick configuration files and scripts (Bash, Python, etc.) to working on larger
projects.

- **Zero Learning Curve:** Full mouse support means you can click, drag-select, and scroll just like in a desktop app.
  It’s the friendliest way to work in a terminal.
- **Sublime-Inspired:** We aim to bring the speed and "vibe" of Sublime Text to the terminal, extended with powerful
  modern features like LSP.
- **Fast & Lightweight:** Written in pure C. It starts in milliseconds, with a single binary size of around 10MB.
- **Advanced Features:** Built-in **Tree-sitter** for high-quality syntax highlighting and **LSP** support for VS
  Code-like intelligence (completions, hover docs, and goto definition) directly in your terminal.
- **Persistent State:** Alwide provides a fully persistent experience. Quit and reopen files as if nothing happened—your
  tabs, cursor positions, workspace setup, and even undo/redo history are fully preserved. Copy in Alwide, paste into
  your terminal.
- **Clean Codebase:** Want to understand how it works or add a feature? Clone, read, write, and compile. It is highly
  readable and perfect for education or curiosity.

### Supported Languages

Many languages are supported out of the box. If your preferred language is missing, you can add support in just a few
minutes by cloning the repo and updating the configuration (ask you best llm friend)!

**C/C++, Python, Java, Go, Rust, JavaScript/TypeScript, Dart, Lua, Bash, HTML, CSS, JSON, Markdown, VHDL, Assembly, and
more.**

#### LSP Server Installation

To enable optional language intelligence (auto-completion, hover definitions, go-to-definition), you can install the
corresponding Language Server (LSP) on your system.

> [!NOTE]
> LSPs are **completely optional** and are not required for Alwide to function. The editor will run perfectly fine
> without any LSP installed. Additionally, you are free to use any LSP server of your choice and configure its binary
> name
> and command-line arguments in `~/.config/alwide/languages-features.json`.

Here are some example of lsp servers :

| Language                    | LSP Server                    | Command/Package Example                       |
|:----------------------------|:------------------------------|:----------------------------------------------|
| **C / C++**                 | `clangd`                      | `apt install clangd` or `dnf install clangd`  |
| **Python**                  | `pylsp`                       | `pip install python-lsp-server`               |
| **Java**                    | `jdtls`                       | `eclipse-jdtls` package                       |
| **Go**                      | `gopls`                       | `go install golang.org/x/tools/gopls@latest`  |
| **JavaScript / TypeScript** | `typescript-language-server`  | `npm install -g typescript-language-server`   |
| **HTML**                    | `vscode-html-language-server` | `npm install -g vscode-langservers-extracted` |
| **CSS / SCSS**              | `vscode-css-language-server`  | `npm install -g vscode-langservers-extracted` |
| **JSON**                    | `vscode-json-language-server` | `npm install -g vscode-langservers-extracted` |
| **Bash**                    | `bash-language-server`        | `npm install -g bash-language-server`         |
| **Markdown**                | `marksman`                    | `marksman` binary                             |
| **Lua**                     | `lua-language-server`         | `lua-language-server` package                 |
| **Dart**                    | `dart`                        | Included in Dart SDK (`dart language-server`) |
| **C#**                      | `omnisharp`                   | `omnisharp` package                           |
| **Makefile**                | `makefile-lsp`                | `cargo install makefile-lsp`                  |
| **VHDL**                    | `vhdl_ls`                     | `cargo install rust_hdl`                      |
| **Assembly**                | `asm-lsp`                     | `asm-lsp` binary                              |
| **Latex**                   | `texlab`                      | `texlab` package                              |

---

## Essential Shortcuts

| Shortcut           | Action                      |
|:-------------------|:----------------------------|
| `Ctrl + S`         | **Save** & Auto-format      |
| `Ctrl + Q`         | **Quit** Alwide             |
| `Ctrl + E`         | **File Explorer** toggle    |
| `Ctrl + O`         | **Open File** toggle        |
| `Ctrl + Space`     | **Completion Popup** toggle |
| `Ctrl + W`         | **Close** Current Tab       |
| `Ctrl + Shift + /` | **Toggle Comment**          |
| `Ctrl + R`         | **Format** Code (LSP)       |
| `Ctrl + Z/Y`       | **Undo / Redo**             |
| `Shift + Arrows`   | **Select Text**             |

---

## Installation

### Quick Install (Linux x86_64)

The easiest way to install Alwide and its assets is using the official installation script. Open your terminal and run:

```bash
curl -fsSL https://raw.githubusercontent.com/arnauda-gh/Alwide/main/install.sh | bash
```

> **Note:** This script will download the latest binary and assets, place the binary in `~/.local/bin/al`, and setup the
> configuration in `~/.config/alwide/`. Make sure `~/.local/bin` is in your `PATH`.

### Manual Installation & Packages

You can also find pre-built binaries, AppImage, `.deb`, and `.rpm` packages in
the [Releases section](https://github.com/arnauda-gh/Alwide/releases).

---

## Compilation from Source

If you prefer to compile Alwide yourself or use an unsupported architecture, follow the instructions below:

### Submodules

This project depends on several external libraries as submodules. To make the checkout much faster, you can perform a
**shallow clone** (only pulling the latest commit history) and download submodules in **parallel**:

To clone the repository and its submodules quickly:

```bash
git clone --recurse-submodules --shallow-submodules --depth 1 https://github.com/arnauda-gh/Alwide.git
cd Alwide
```

Or if you have already cloned the repository and want to fetch the submodules quickly:

```bash
git submodule update --init --recursive --depth 1 --jobs 8
```

### Dependencies :

#### Ubuntu/Debian :

- `apt install make gcc libncursesw5-dev`

#### Clang

Any recent version of Clang (e.g., version 11 or higher) should work.

Install Clang if it's not already available:

#### Install tree-sitter api.h

May be useless now. Skip for first try.

- Be in the root folder
- `make -C lib/tree-sitter/ install`

#### Install tree-sitter cli

Some tree-sitter parsers need to use `tree-sitter generate` to convert grammars to `parser.c` (for now only latex need
it).

use :

- `npm install -g tree-sitter-cli`

#### Install rust/rustup/cargo packages

- `rustc --version`

It's really important to be up to date ! You will be in most cases not up to date.

Ubuntu :

- `apt install rustup`
- `rustup update stable`

Others distro may not have rustup package. Check for your personnal distro. You might find this
useful : https://rustup.rs/

#### Compile :

In the root folder, compile the default debug/development version:

- `make`

Or compile the production/release version:

- `make release`

> [!WARNING]
> The default debug/development mode (`make`) compiles with address sanitizers (`-fsanitize=address`) and verbose
> logging enabled. This makes the editor **extremely slow**. For daily use or production, you should compile the release
> version instead using:
> ```bash
> make release
> ```

#### To install Alwide:

You can install Alwide system-wide or locally.

**1. System-wide installation (requires sudo):**
This installs the binary and the default assets for all users.

```bash
sudo make install
```

**2. User configuration (recommended):**
This copies the default themes and language settings to your home directory so you can customize them.

```bash
make install-config
```

---

## Configuration & Assets

Alwide looks for its configuration (themes, language rules) in this order:

1. **Environment Variable**: `ALWIDE_ASSETS_PATH` (if set).
2. **User Folder**: `~/.config/alwide/`.
3. **System Folder**: `/usr/local/share/alwide/` (or your custom `PREFIX`).

### Customizing Alwide

Your personal settings live in `~/.config/alwide/`:

- `languages-features.json`: Custom LSP commands and per-language tweaks.
- `theme/`: Color schemes for the editor.
- `queries/`: Tree-sitter highlighting rules.

---

### Nix-based systems

*Not fully tested.*

Alwide is Nix-compatible. You can use the provided `flake.nix` for a reproducible environment.

```bash
nix develop  # To enter the dev environment
make release # To compile
./al         # Run locally (uses local assets automatically)
```

To install it via Nix:

```bash
nix profile install .
```

---

## Contributing & Reporting Issues

Alwide needs testing across different terminal emulators, mouse drivers, and distributions. Any feedback, bug reports,
or contributions are highly appreciated!

### How to Report Issues

If you encounter crashes, bugs, or unexpected behavior, please report them! To help us debug, compile Alwide with
logging enabled.

The [Makefile](file:///home/arno/dev/Alwide/Makefile) has two build configurations:

1. **Debug & Logging Build (Default):** Compiles with debug symbols (`-g`), AddressSanitizer (`-fsanitize=address`) to
   catch memory bugs, and redirects standard error to log files.
2. **Release Build:** Optimized for speed (`-O3`), with asserts and logging disabled (`-DNDEBUG`).

*Note: The build system automatically tracks configuration/mode changes. You don't need to run `make clean` or use `-B`
when switching between debug and release builds; it will detect changes and rebuild accordingly.*

#### Steps to Generate Logs

1. Compile the default debug version:
   ```bash
   make
   ```
2. Run the compiled editor (`./al` or `al`) and reproduce the issue.
3. Locate the log files in the directory where you ran `al`:
    * **Application Logs:** `.logs.txt` (captures editor warnings, errors, and crashes)
    * **LSP Logs:** `.lsp_logs.txt` (captures Language Server Protocol communication)

Please attach these logs when opening an issue.

---

Check out our [**Technical Documentation**](doc/architecture.md) to dive into the internals.

---

## License

Distributed under the **MIT License**. See `LICENSE` for more information.


