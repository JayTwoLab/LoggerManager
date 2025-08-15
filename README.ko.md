# C++ 프로젝트용 .gitignore

[English](README.md)

---

이 `.gitignore` 파일은 다양한 IDE, 에디터, 빌드 시스템을 포괄하는 **C++ 프로젝트 전용 종합 무시 목록**입니다.

## 포함된 환경

- **IDE / 에디터**:
  - Visual Studio 2019/2022
  - Visual Studio Code (VSCode)
  - CLion / JetBrains IDE
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

- **빌드 시스템**:
  - CMake
  - Ninja
  - Meson
  - SCons

- **패키지 관리자**:
  - Conan
  - vcpkg
  - Hunter
  - CPM.cmake

- **언어 서버**:
  - clangd
  - ccls
  - gtags / ctags

## 목적

이 `.gitignore`의 목적은 다음과 같습니다:
- 빌드 과정에서 생성되는 임시 산출물을 저장소에서 제거
- 로컬 IDE/에디터 설정 파일을 커밋하지 않도록 방지
- 사용자/PC별로 달라지는 캐시 및 로그 파일 제외

## 사용 방법

C++ 프로젝트의 루트 디렉터리에 `.gitignore` 파일을 배치합니다:

```bash
curl -o .gitignore https://example.com/path/to/.gitignore
```

Git은 `.gitignore`에 명시된 패턴과 일치하는 파일/디렉터리를 자동으로 무시합니다.

## 참고 사항

- `.gitignore`에 포함되어 있어도 특정 파일을 강제로 추가하려면 `git add -f <file>` 명령을 사용하세요.
- `Makefile`을 직접 관리하는 경우, `.gitignore`의 `Makefile` 항목을 삭제하세요.

