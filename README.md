# C++ Project .gitignore


[Korean](README.ko.md)

---

This `.gitignore` file is a **comprehensive ignore list** for C++ projects, covering a wide range of IDEs, editors, and build systems.

## Included Environments

- **IDEs / Editors**:
  - Visual Studio 2019/2022
  - Visual Studio Code (VSCode)
  - CLion / JetBrains IDEs
  - Qt Creator
  - Eclipse CDT
  - Xcode
  - Neovim / Vim
  - Emacs
  - Sublime Text
  - Atom
  - Code::Blocks
  - CodeLite
  - KDevelop
  - NetBeans (C/C++)
  - Geany
  - Dev-C++
  - JetBrains Fleet
  - Kate

- **Build systems**:
  - CMake
  - Ninja
  - Meson
  - SCons

- **Package managers**:
  - Conan
  - vcpkg
  - Hunter
  - CPM.cmake

- **Language servers**:
  - clangd
  - ccls
  - gtags / ctags

## Purpose

This `.gitignore` aims to:
- Keep repositories clean from temporary build outputs.
- Avoid committing local IDE/editor configuration files.
- Exclude cache and log files that are machine/user-specific.

## Usage

Place the `.gitignore` file in the root of your C++ project:

```bash
curl -o .gitignore https://example.com/path/to/.gitignore
```

Git will automatically ignore files and directories matching the patterns.

## Notes

- If you need to track some files listed in `.gitignore`, use `git add -f <file>` to force-add them.
- If you are manually managing `Makefile`, remove the `Makefile` entry in this `.gitignore`.

