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
- **Fast & Lightweight:** Written in pure C. It starts in milliseconds, with a single binary size of around 3MB.
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


## Keyboard & Mouse Shortcuts

Alwide features a comprehensive list of shortcuts and mouse gestures designed to provide a modern, mouse-friendly, but also keyboard-efficient IDE experience in the terminal.

Note that for now the shortcut are static defined in the raw code but will be later available to be changed. Note that it's pretty easy to changes thoses keybindings editing `editor_input.c` file.

### Mouse & Rich Interactive Gestures

| Gesture | Action |
|:---|:---|
| `Left Click` | Position the cursor at the clicked character. |
| `Left Click & Drag` | Select text dynamically. |
| `Double Click` | Select the word under the cursor. |
| `Ctrl + Left Click` | **Go to Definition** of the clicked symbol (via LSP). |
| `Ctrl + Mouse Hover` | **Show Hover Docs** / type tooltips for the hovered symbol (via LSP). |
| `Scroll Wheel` | Scroll the editor viewport vertically. |
| `Shift + Scroll Wheel` | Scroll the editor viewport horizontally. |
| `Mouse Side Buttons (8 / 9)`| Navigate Backward / Forward in jump history. |
| `Left Click` on Tabs | Switch active document tab. |
| `Drag & Drop` on Tabs | Reorder active document tabs. |
| `Scroll Wheel` on Tabs | Scroll through document tabs. |
| `Double Click` in Explorer | Open file (switch focus to editor) or expand/collapse folder. |
| `Right Click` in Explorer | Open context menu (New File, New Folder, Rename, Delete). |
| `Click` on Language (Status Bar) | Open language selector popup. |

### General Keyboard Shortcuts

| Shortcut | Action |
|:---|:---|
| `Ctrl + S` | **Save** active file & auto-format (via LSP). |
| `Ctrl + W` | **Close** current document tab. |
| `Ctrl + Q` | **Quit** Alwide (prompts to save if files are modified). |
| `Ctrl + Z` | **Undo** last edit. |
| `Ctrl + Y` | **Redo** last undone edit. |
| `Ctrl + F` | **Search / Find** (pre-fills search input with current selection). |
| `Ctrl + R` | **Format** code manually (via LSP). |

### Navigation & Selection

| Shortcut | Action |
|:---|:---|
| `Arrow Keys` | Move cursor character-by-character / line-by-line. |
| `Shift + Arrow Keys` | **Select text** character-by-character / line-by-line. |
| `Ctrl + Left / Right` | Move cursor to previous / next word boundary. |
| `Ctrl + Shift + Left / Right`| **Select text** word-by-word. |
| `Ctrl + Up / Down` | Select the word under the cursor. |
| `Ctrl + Shift + Up / Down` | Switch to the next / previous opened tab. |
| `Ctrl + I / J / K / N` | Alternate navigation (Up / Left / Right / Down). |
| `Ctrl + Shift + I / J / K / N`| Alternate selection (Up / Left / Right / Down). |
| `Home` | Move cursor to start of line. |
| `End` or `Ctrl + ;` | Move cursor to end of line. |
| `Shift + End` | **Select text** to the end of the line. |
| `Ctrl + A` | **Select all** text in the active buffer. |
| `Ctrl + U` | Navigate backward in cursor jump history. |
| `Ctrl + P` | Navigate forward in cursor jump history. |

### Text Editing & Manipulation

| Shortcut | Action |
|:---|:---|
| `Enter` | Insert new line (inherits indentation from the line above). |
| `Shift + Enter` | Move cursor to end of line and insert new line. |
| `Tab` | Indent selection (if multi-line) or insert indentation characters. |
| `Shift + Tab` | De-indent selection (if multi-line). |
| `Backspace` | Delete character to the left (or delete selection). |
| `Delete` / `Suppr` | Delete character to the right (or delete selection). |
| `Ctrl + Backspace` / `Ctrl + H`| Delete the word to the left of the cursor. |
| `Ctrl + Delete` / `Ctrl + Suppr`| Delete the word to the right of the cursor. |
| `Ctrl + D` | **Delete current line** (or delete selection). |
| `Ctrl + Shift + /` or `Ctrl + _`| **Toggle comment** on current line / selection. |

### Window & Panel Focus

| Shortcut | Action |
|:---|:---|
| `Ctrl + E` | Toggle **File Explorer** (FEW) sidebar. |
| `Ctrl + L` | Toggle **Open Files** (OFW) tab list popup. |
| `Ctrl + B` | Toggle **Status / Diagnostics** sidebar. |
| `Ctrl + Space` | Trigger auto-completion popup. |
| `Escape` or `Ctrl + [` | Close current popup or clear selection. |

### Sidebar (File Explorer) View Mode

When the File Explorer sidebar is focused, use the following shortcuts:

| Shortcut | Action |
|:---|:---|
| `Up` / `k` or `Down` / `j` | Navigate up / down the directory tree. |
| `Enter` / `Space` | Toggle folder expand state, or open selected file. |
| `R` / `F5` | Reload folder tree from disk. |
| `Escape` | Switch focus back to the editor window. |

### Popup List Interaction (Completion / Goto list)

When a popup list (e.g., autocompletions or Goto Definition list) is open:

| Shortcut | Action |
|:---|:---|
| `Up` / `Down` | Move selection highlight. |
| `Enter` / `Tab` | Accept and insert selection. |
| `Escape` | Cancel and close popup. |


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


