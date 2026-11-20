sed-windows
===========

[![License](https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT%20OR%20CC0--1.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![CI](https://github.com/SamuelMarks/sed-windows/actions/workflows/ci.yml/badge.svg)](https://github.com/SamuelMarks/sed-windows/actions/workflows/ci.yml)
![Doc Coverage](https://img.shields.io/badge/doc%20coverage-100%25-brightgreen)
![Test Coverage](https://img.shields.io/badge/test%20coverage-100%25-brightgreen)

Native Windows (**MSVC** `X64`, `X86`, `ARM64`) and 32-bit Protected-Mode **MS-DOS** (**OpenWatcom v2** / `DOS/32A` across Microsoft MS-DOS `1.25`, `2.0`, and `4.0`) CMake build, packaging, and strict ISO C89 verification suite for unmodified **GNU `sed` `4.10`** (`2026-04-21`), powered by [`auto-win-msvc`](https://github.com/SamuelMarks/auto-win-msvc).

- **Architecture & Internals**: See [`ARCHITECTURE.md`](ARCHITECTURE.md)
- **CLI, Build & Platform Usage Guide**: See [`USAGE.md`](USAGE.md)

## Highlights

- **Zero Upstream Edits (Cryptographically Verified)**:
  - Fetches the official bootstrapped `sed-4.10.tar.xz` (`SHA256=b8e72182b2ec96a3574e2998c47b7aaa64cc20ce000d8e9ac313cc07cecf28c7`) or uses an unmodified local checkout (`-DSED_SOURCE_DIR=...` / `../sed`).
  - Not a single `.c` or `.h` file of GNU `sed 4.10` is edited on disk or patched via `.patch` files; `t_hygien` (`tests/test_upstream_immutability.c`) recomputes FIPS 180-4 SHA-256 digests over every upstream `basicdefs.h`, `sed/*.c`, `sed/*.h`, `lib/*.c`, and `lib/*.h` file (`126` files) against `upstream_manifest.h` before and after testing.
- **Zero Non-Test C/H Polyfills in Repo**:
  - Enforced by the `forbid-non-test-c-sources` pre-commit gate and `cmake/VerifyC89Strict.cmake`.
  - All POSIX/Windows/MS-DOS OS shims (`posix_mkostemp` with CSPRNG on Windows and 8.3-safe in-place rewrite on MS-DOS, atomic-overwrite `posix_rename`, `posix_fdopen`, `popen`/`pclose`, `awm_fwriting`, `getopt_long`, `fcntl`, `langinfo`, and build-generated `alloca.h`) link via the unified [`../auto-win-msvc`](https://github.com/SamuelMarks/auto-win-msvc) static library on both MSVC and OpenWatcom DOS.
- **GNU `sed 4.10` Full Feature Fidelity**:
  - Preserves 64-bit `idx_t` (`ptrdiff_t`) buffers, C23 checked arithmetic (`stdckdint.h`), `stdcountof.h`, 32-bit Unicode (`uchar.h` / `mbrtoc32` decoupled from 16-bit Windows `wchar_t`), DFA + NFA (`rpl_regcomp`) dual-engine matching, binary CRLF mode (`-b`), atomic in-place editing (`-i`, `-i.bak`), `--sandbox`, and `--debug`.
- **100% Strict ISO C89 Test & White-Box Suite (Zero Python/Perl/`sh` Runtime)**:
  - Every test file under `tests/` compiles cleanly with `-std=c89 -pedantic-errors -Wall -Wextra -Werror` (GCC/Clang), `/W4 /WX /TC` (MSVC), and `wcl386 -bt=dos -mf -3r -za -w4 -wx` (**strict ANSI C89 `-za`**) on OpenWatcom MS-DOS.
  - Includes a self-contained C89 port of all **77 upstream `sed-4.10/testsuite/`** test cases (`t_reg410`) plus white-box deterministic fault injection (`t_white` via `libsed_testable`) running natively on Windows, POSIX hosts, **and** Microsoft MS-DOS `1.25`, `2.0`, and `4.0`.

## Architecture & Directory Layout

```text
sed-windows/
├── ARCHITECTURE.md                 # Build pipeline, header generation & test architecture
├── USAGE.md                        # CLI reference, build options & Windows/MS-DOS usage guide
├── README.md                       # Project overview & quick-start instructions
├── TODO_PLAN.md                    # Porting engineering plan & milestone checklist
├── CMakeLists.txt                  # Dual-target MSVC & OpenWatcom DOS build + CTest + CPack
├── CPackConfig.cmake               # Binary ZIP, NSIS (.exe), and WiX (.msi) packaging
├── CPackSourceConfig.cmake         # Source distribution packaging
├── .pre-commit-config.yaml         # Strict C89, doc coverage, formatting & hygiene gates
├── release.bat                     # One-command Native Windows (MSVC) Release build & CPack
├── release_dos.bat                 # One-command 32-bit MS-DOS (OpenWatcom v2) Release build
├── .github/workflows/ci.yml        # Linux C89/lcov, Windows MSVC & QEMU DOS CI/Release
├── cmake/
│   ├── GenerateSedHeaders.cmake    # Deterministic config.h, gnulib *.in.h & SHA-256 manifest
│   ├── UpdateCoverageShields.cmake # Deterministic README doc/test shield updater
│   ├── VerifyC89Strict.cmake       # Strict C89 syntax, zero //, and 100% doc coverage gate
│   └── VerifyCoverage.cmake        # gcov/lcov & OpenCppCoverage coverage enforcement gate
├── tests/
│   ├── sed_test_error.h            # Unified strict-C89 error enum (enum sed_test_err)
│   ├── sed_test_harness.{h,c}      # Cross-platform C89 process runner, I/O & FIPS SHA-256
│   ├── sed_test_hooks.h            # Fault-injection & re-entrancy macros for libsed_testable
│   ├── sed_test_inproc.{h,c}       # setjmp/dup2 in-process runner & deterministic fault hooks
│   ├── test_upstream_immutability.c# t_hygien: SHA-256 upstream manifest & header contracts
│   ├── test_sed_cli_options.c      # t_cli:    CLI flags, -i, -b CRLF, --sandbox, exit codes
│   ├── test_sed_addresses_cmds.c   # t_cmds:   All sed addresses & commands (a..z, s///, y///)
│   ├── test_sed_410_regressions.c  # t_reg410: All 77 upstream GNU sed 4.10 testsuite cases
│   └── test_sed_whitebox.c         # t_white:  White-box & I/O/mbcs/panic fault injection
└── vagrant/
    └── dos/
        └── Vagrantfile             # Automated MS-DOS 1.25, 2.0 & 4.0 OpenWatcom + CTest CI
```

## Building on Native Windows (MSVC `X64`, `X86`, `ARM64`)

### Prerequisites

- Visual Studio 2022 (or Build Tools for Visual Studio) with C11 support (`cl.exe`)
- CMake 3.16+ (and optionally Ninja, NSIS, and WiX Toolset for installer generation)
- Sibling `../auto-win-msvc` checkout (or fetched automatically from GitHub via `FetchContent`)

### Quick Build, Test & Package

From a **Developer Command Prompt for VS** (defaults to `x64`, or pass `Win32` / `ARM64`):

```bat
release.bat
release.bat ARM64
```

Or manually with CMake and CTest:

```bat
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -C Release
```

To cross-compile for Windows `ARM64` from an `x64` host (`vcvarsamd64_arm64.bat`):

```bat
cmake -G Ninja -S . -B build_arm64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_PROCESSOR=ARM64
cmake --build build_arm64 --parallel
```

## Building for 32-bit Protected-Mode MS-DOS (OpenWatcom v2)

### Standalone Host Build (`Windows`, `Linux`, or `macOS`)

Ensure OpenWatcom v2 (`WATCOM`) is installed, then on Windows:

```bat
release_dos.bat
```

Or on Linux/macOS with OpenWatcom v2 (`wcl386` + `wmake`; replace `binl64` with `arml64` on Linux ARM64, `armo64` on macOS Apple Silicon, or `bino64` on Intel macOS):

```sh
export WATCOM=/opt/watcom
export PATH="$WATCOM/binl64:$WATCOM/binw:$PATH"
export INCLUDE="$WATCOM/h"
export EDPATH="$WATCOM/eddat"
export WIPFC="$WATCOM/wipfc"

cmake -G "Watcom WMake" -S . -B build_dos -DCMAKE_SYSTEM_NAME=DOS -DCMAKE_C_COMPILER=wcl386 -DCMAKE_BUILD_TYPE=Release -DSED_TARGET_DOS=ON -DBUILD_TESTING=ON
cmake --build build_dos
```

Run inside MS-DOS (`1.25`, `2.0`, `4.0`, or FreeDOS) using `DOS32A.EXE`:

```bat
DOS32A.EXE SED.EXE --version
DOS32A.EXE SED.EXE -f SCRIPT.SED INPUT.TXT
```

### Automated Vagrant Verification Across MS-DOS `1.25`, `2.0`, and `4.0`

`vagrant/dos/Vagrantfile` automatically provisions OpenWatcom v2 on the host, applies legacy MZ-header 512-byte sector alignment and `INT 21h` patches to `DOS32A.EXE` for 1982/1983 MS-DOS `1.25` and `2.0`, compiles `SED.EXE` and all five 8.3 strict-C89 test binaries (`T_HYGIEN.EXE`, `T_CLI.EXE`, `T_CMDS.EXE`, `T_REG410.EXE`, `T_WHITE.EXE`), uploads them via `SERGET.COM` high-speed serial streaming, and executes all 11 CTest targets inside each guest OS:

```sh
cd vagrant/dos
vagrant up --provider=qemu msdos-1.25
vagrant up --provider=qemu msdos-2.0
vagrant up --provider=qemu msdos-4.0
```

## Strict ISO C89 Hygiene & 100% Coverage Verification (POSIX Host)

```sh
# 1. Verify strict C89 syntax (-std=c89 -pedantic-errors -Werror), zero // comments,
#    zero non-tests/ .c/.h files, and 100% Doxygen + CMake .rst documentation coverage:
cmake -DPROJECT_ROOT=. -P cmake/VerifyC89Strict.cmake

# 2. Run all pre-commit hooks (clang-format, cpplint, line endings, C89 & doc gates):
pre-commit run --all-files

# 3. Build with gcov/lcov instrumentation, run all C89 test suites, and enforce coverage
#    (100% line/function/branch on tests/sed_test_harness.c, tests/sed_test_inproc.c,
#    sed/sed.c, and sed/mbcs.c; 100% function coverage across all 7 sed/*.c units):
cmake -S . -B build_cov -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DSED_ENABLE_COVERAGE=ON
cmake --build build_cov --parallel
cmake --build build_cov --target coverage
```

## License

All CMake scripts, packaging configurations, Vagrant automation, and strict ISO C89 test files in `sed-windows` are tri-licensed under your choice of:

- **Apache License, Version 2.0** ([`LICENSE-APACHE`](LICENSE-APACHE))
- **MIT License** ([`LICENSE-MIT`](LICENSE-MIT))
- **CC0 1.0 Universal** ([`LICENSE-CC0`](LICENSE-CC0))

Upstream **GNU `sed` `4.10`** is fetched unmodified at build time and distributed under the **GNU General Public License v3.0 or later (`GPL-3.0-or-later`)**; consequently, any compiled `sed.exe` / `SED.EXE` binary redistributions are governed by `GPL-3.0-or-later`.
