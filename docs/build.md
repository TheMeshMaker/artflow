# Building ArtFlow (Pencil2D base) on Windows

ArtFlow is a private fork of [Pencil2D](https://github.com/pencil2d/pencil)
(GPL-2.0) with a beginner shell on top. One build produces `pencil2d.exe`
(stock, for comparison) and `artflow.exe` / `artflow-console.exe`.

Verified on 2026-10-26, Windows 11 Pro 26200. A full ArtFlow build is
about 190 compile steps and takes a few minutes; the deployed app folder is
about 80 MB.

## Toolchain (portable, nothing on PATH)

| Piece | Version | Where |
|---|---|---|
| Source | Pencil2D `master` at `408563a3` (2026-10-26), branch `artflow/main` | `git clone --depth 1 -o upstream https://github.com/pencil2d/pencil.git artflow` |
| Qt | 6.8.3 LTS, `win64_llvm_mingw` + qtmultimedia, qtimageformats | `uvx --from aqtinstall aqt install-qt windows desktop 6.8.3 win64_llvm_mingw -O H:\afdev\qt6 -m qtmultimedia qtimageformats` (768 MB) |
| Compiler | llvm-mingw `20251118` UCRT (clang 21.1.6) | `H:\afdev\tools\llvm-mingw-20251118-ucrt-x86_64` (Qt's kit is built with clang 17; libc++ is backward compatible) |
| CMake / Ninja | 3.31.12 / 1.13.2 | `H:\afdev\tools\` |

`H:\afdev\afenv-qt6.bat` puts only Qt, the compiler, CMake, Ninja, Git and
`C:\WINDOWS` on PATH and sets `CMAKE_PREFIX_PATH` to the Qt kit.

## Build

```bat
call H:\afdev\afenv-qt6.bat
cmake -S C:\Users\Meshal\Documents\Codex\2026-09-07\are-x20\artflow -B H:\afdev\b_p2d -G Ninja ^
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
ninja -C H:\afdev\b_p2d artflow artflow-console
```

`-DBUILD_ARTFLOW=OFF` builds only stock Pencil2D (`ninja pencil2d`).

## Deploy and run

```bat
set DIST=H:\afdev\artflow-dist
copy H:\afdev\b_p2d\artflow.exe %DIST%  &  copy H:\afdev\b_p2d\artflow-console.exe %DIST%
windeployqt --no-translations --qtpaths H:\afdev\qt6\6.8.3\llvm-mingw_64\bin\qtpaths6.exe --dir %DIST% %DIST%\artflow.exe
rem plus libc++.dll, libunwind.dll, libwinpthread-1.dll from llvm-mingw\x86_64-w64-mingw32\bin
%DIST%\artflow.exe
```

`windeployqt --release` fails with "Unable to find the platform plugin" for
this kit; run it without `--release`.

| Variable | Effect |
|---|---|
| `ARTFLOW_MODE=flip\|motion\|studio` | start in that mode (default: the last one used) |
| `ARTFLOW_SELFTEST=<folder>` | run the self-test, write `report.txt` and screenshots there |
| `ARTFLOW_SELFTEST_QUIT=1` | quit when done (exit code 0 = all checks passed) |

The self-test sends synthetic tablet and mouse events to the canvas widget
only; the system cursor never moves.

## Settings

ArtFlow keeps its own settings in `HKCU\Software\ArtFlow\ArtFlow` (the
core's `QSettings(PENCIL2D, PENCIL2D)` is compiled with `PENCIL2D="ArtFlow"`).
First start writes, once per version: the beginner key map
(`ArtFlowKeyMapVersion`), the newer undo system (`ArtFlowDefaultsVersion`),
and opens maximized (`ArtFlowStartedBefore`).

The one launch of stock `pencil2d.exe` during set-up created
`HKCU\Software\Pencil` and `%APPDATA%\Pencil2D`; they were left in place.

## Upstream files patched

| File | Change |
|---|---|
| `core_lib/src/util/pencildef.h` | `#ifndef PENCIL2D` guard around the settings name |
| `CMakeLists.txt` | `BUILD_ARTFLOW` option that includes `shell/shell.cmake` |

Everything else under `core_lib/` and `app/` is untouched; ArtFlow's code
is in `studiocore/` and `shell/`.

## The earlier Krita-based spike

`are-x20/artflow-krita-spike` (branch `artflow/phase-0-1`) holds the first
spike on the Krita 5.2 engine, with its own `docs/build.md`. Its build
lives in `H:\afdev\b_krita` and `H:\afdev\_install` (about 6 GB with the
dependency cache) and is no longer needed by ArtFlow.
