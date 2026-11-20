# Architecture of `sed-windows`

`sed-windows` builds, packages, and verifies unmodified **GNU `sed` `4.10`** (`2026-04-21`) across Native Windows (**MSVC** `X64`, `X86`, `ARM64`), 32-bit Protected-Mode **MS-DOS** (**OpenWatcom v2** + `DOS/32A` on Microsoft MS-DOS `1.25`, `2.0`, `4.0`, and FreeDOS), and POSIX coverage hosts (**GCC** / **Clang**).

For build, CLI, and platform usage instructions, see [`USAGE.md`](USAGE.md) and [`README.md`](README.md).

---

## 1. Core Architectural Invariants

The repository is structured around four mechanically enforced invariants:

1. **Zero Upstream Source Edits (Cryptographically Verified)**
   - Upstream GNU `sed 4.10` (`sed-4.10.tar.xz`, `SHA256=b8e72182b2ec96a3574e2998c47b7aaa64cc20ce000d8e9ac313cc07cecf28c7`) is never patched, edited in place, or checked into the repository with modifications.
   - At configure time, `cmake/GenerateSedHeaders.cmake` hashes all `126` upstream `.c` and `.h` files (`basicdefs.h`, `sed/*.c`, `sed/*.h`, `lib/*.c`, `lib/*.h`) into `${CMAKE_CURRENT_BINARY_DIR}/sed_generated/upstream_manifest.h`.
   - At test time, `t_hygien` (`tests/test_upstream_immutability.c`) recomputes FIPS 180-4 SHA-256 digests in pure ISO C89 and fails if a single upstream byte changed.

2. **Zero Non-Test C/H Polyfills in This Repository**
   - Both `.pre-commit-config.yaml` (`forbid-non-test-c-sources`) and `cmake/VerifyC89Strict.cmake` (`sed_verify_no_stray_c_files`) forbid any `.c`, `.h`, or `.h.in` file outside `tests/`.
   - All POSIX/Windows/MS-DOS portability shims live in [`auto-win-msvc`](https://github.com/SamuelMarks/auto-win-msvc) and link via the unified `auto-win-msvc` static library target.

3. **Strict ISO C89 (`ANSI X3.159-1989`) Test Infrastructure**
   - All test harnesses and suites under `tests/` are written strictly in ISO C89 (`/* ... */` comments only, declarations at top of block, zero VLAs, zero designated initializers, zero Python/Perl/`sh` test runners).
   - Compiled with `-std=c89 -pedantic-errors -Wall -Wextra -Werror` (GCC/Clang), `/W4 /WX` (MSVC), and `wcl386 -bt=dos -mf -3r -za -w4 -wx` (OpenWatcom strict ANSI `-za`).

4. **100% Documentation & Enforced Code Coverage**
   - `cmake/VerifyC89Strict.cmake` enforces `.rst` module/function documentation on every `.cmake` file and `/** ... */` Doxygen documentation (`@file`, `@brief`, `@param`, `@return`) on every file, struct, enum, non-guard macro, and top-level function in `tests/`.
   - `cmake/VerifyCoverage.cmake` enforces `100%` line, function, and branch coverage on `tests/sed_test_harness.c`, `tests/sed_test_inproc.c`, `sed/sed.c`, and `sed/mbcs.c`, `100%` function coverage (`113/113`) across all seven `sed/*.c` units, and a `>=97%` aggregate line floor across unmodified `sed/*.c`.

---

## 2. High-Level Build & Target Graph

```text
  +-----------------------------------+     +-----------------------------------+
  |  Upstream GNU sed 4.10 (Read-Only)|     |   auto-win-msvc (POSIX/Win/DOS)   |
  |  basicdefs.h, sed/*.c, lib/*.c    |     |  posix-core, fcntl, stat, getopt, |
  |  config_h.in, lib/*.in.h          |     |  langinfo, AutoWinMsvcTransform   |
  +-----------------+-----------------+     +-----------------+-----------------+
                    |                                         |
                    | (Configure-Time Read Only)              |
                    v                                         |
  +-----------------------------------+                       |
  | cmake/GenerateSedHeaders.cmake    |                       |
  | -> build/sed_generated/           |                       |
  |    config.h, version.{h,c},       |                       |
  |    upstream_manifest.h,           |                       |
  |    stdckdint.h, stdcountof.h,     |                       |
  |    uchar.h, obstack.h, error.h,   |                       |
  |    uni*.h, selinux/*.h,           |                       |
  |    malloc/*.gl.h, [alloca.h]      |                       |
  +-----------------+-----------------+                       |
                    |                                         |
        (DOS only: auto_win_msvc_transform_sources            |
         -> build/transformed_lib & build/transformed_sed)    |
                    |                                         |
                    v                                         v
  +-----------------------------------------------------------------------------+
  |                     libsed_gnulib (STATIC LIBRARY)                          |
  |   57 upstream gnulib lib/*.c modules + generated version.c + auto-win-msvc  |
  +------------------------+-------------------------------+--------------------+
                           |                               |
            Production     |                               |  BUILD_TESTING=ON
            Binary         v                               v  (SED_TESTABLE_BUILD=1)
        +----------------------+               +--------------------------------+
        | sed / sed.exe /      |               | libsed_testable (STATIC)       |
        | SED.EXE              |               | sed/*.c compiled with          |
        | Zero test hooks      |               | main=sed_testable_main +       |
        +----------+-----------+               | tests/sed_test_hooks.h         |
                   |                           +---------------+----------------+
                   | (Spawned E2E)                             | (In-Process + Faults)
                   v                                           v
        +-----------------------------------------------------------------------+
        |                Strict ISO C89 Test Binaries (8.3 Names)               |
        |  t_hygien  |  t_cli  |  t_cmds  |  t_reg410 (77 cases)  |  t_white    |
        +-----------------------------------------------------------------------+
```

---

## 3. Dependency Discovery (`CMakeLists.txt`)

### 3.1 Upstream GNU `sed 4.10` Resolution
`CMakeLists.txt` locates unmodified GNU `sed 4.10` in priority order (`_sed_is_valid_upstream_tree`, checking `sed/sed.h`, `sed/execute.c`, `lib/dfa.c`, and `lib/stdckdint.in.h`):
1. Explicit `-DSED_SOURCE_DIR=<path>` cache variable.
2. Sibling `../sed` or local `./sed` directory.
3. CMake `FetchContent` downloading `sed-4.10.tar.xz` with mirror failover (`ftp.gnu.org`, `ftpmirror.gnu.org`, `mirrors.kernel.org`) pinned to `SHA256=b8e72182b2ec96a3574e2998c47b7aaa64cc20ce000d8e9ac313cc07cecf28c7`.

### 3.2 `auto-win-msvc` Resolution
`CMakeLists.txt` resolves `auto-win-msvc` via:
1. Explicit `-DAUTO_WIN_MSVC_DIR=<path>`.
2. Sibling `../auto-win-msvc` checkout.
3. CMake `FetchContent` cloning `https://github.com/SamuelMarks/auto-win-msvc.git` (`HEAD`).

`AUTO_WIN_MSVC_BUILD_TESTS` is set to `OFF` when consumed as a subproject so `sed-windows` controls CTest targets directly.

---

## 4. Build-Time Header & Manifest Generation (`cmake/GenerateSedHeaders.cmake`)

GNU `sed` normally relies on Autoconf (`./configure`) and Automake (`Makefile.am`) to instantiate `config.h` and Gnulib `.in.h` headers inside the source tree. `GenerateSedHeaders.cmake` replaces that step deterministically in pure CMake, writing exclusively into `${CMAKE_CURRENT_BINARY_DIR}/sed_generated` (`_sed_write_if_different` preserves mtimes across incremental builds):

| Generated File(s) | Source Template | Purpose |
| :--- | :--- | :--- |
| `config.h` | `config_h.in` + target rules | Package metadata (`4.10`), feature flags (`GNULIB_NO_VLA=1`, `GNULIB_XALLOC=1`), regex symbol isolation (`rpl_regcomp`, `rpl_re_*`), inline helpers (`mbszero`, `memeq`, `streq`, `strnul`), and `auto-win-msvc` polyfill bindings |
| `version.h`, `version.c` | `configure.ac` / `.tarball-version` | Defines `extern char const *Version;` and `Version = "4.10"` |
| `upstream_manifest.h` | `basicdefs.h`, `sed/*`, `lib/*` | `g_sed_upstream_manifest[]` SHA-256 table (`126` entries) verified by `t_hygien` |
| `stdckdint.h`, `stdcountof.h` | `lib/stdckdint.in.h`, `lib/stdcountof.in.h` | C23 checked integer arithmetic (`ckd_add`/`ckd_sub`/`ckd_mul`) and `countof()` |
| `uchar.h`, `uni*.h` | `lib/uchar.in.h`, `lib/uni*.in.h` | 32-bit `char32_t` / `mbrtoc32` Unicode classification & case mapping (decoupled from Windows 16-bit `wchar_t`) |
| `obstack.h`, `error.h` | `lib/obstack.in.h`, `lib/error.in.h` | Gnulib obstack allocator and error reporting declarations with spliced `c++defs.h` / `arg-nonnull.h` / `warn-on-use.h` |
| `selinux/*.h` | `lib/se-*.in.h` | Header-only no-op SELinux stubs (`ENOSYS`) |
| `malloc/*.gl.h` | `lib/malloc/*.h` | Gnulib `dynarray` and `scratch_buffer` internal headers |
| `eloop-threshold.h` | Synthesized | Defines `__ELOOP_THRESHOLD()` fallback (`40`) for symlink loop bounds |
| `alloca.h` | `lib/alloca.in.h` | Instantiated when `TARGET_WIN32` or `TARGET_DOS` is active (`_alloca` via `<malloc.h>`) |

### Include Precedence & Force-Inclusion
`_sed_configure_upstream_target` prepends `BEFORE PRIVATE` include directories in strict priority order:
1. `${SED_GEN_DIR}` (`build/sed_generated`)
2. `${CMAKE_CURRENT_BINARY_DIR}/transformed_sed` and `transformed_lib` (only when `SED_TARGET_DOS=ON`)
3. `${SED_SOURCE_DIR}`
4. `${SED_SOURCE_DIR}/sed`
5. `${SED_SOURCE_DIR}/lib`
6. `auto-win-msvc` `PUBLIC` include paths (`${CMAKE_BINARY_DIR}/posix_includes`, `${auto_win_msvc_SOURCE_DIR}/include`)

Every upstream translation unit force-includes `config.h` (`-FI` on MSVC, `-fi=` on OpenWatcom, `-include` on GCC/Clang) so Gnulib type macros and symbol renames take effect prior to any system header.

### Regex Namespace Isolation
`auto-win-msvc` exports POSIX `regcomp`/`regexec` (backed by PCRE2), whereas GNU `sed` requires Gnulib's internal DFA + `re_compile_pattern` / `re_search` / `struct re_registers` engine (`lib/regex.c`). `config.h` maps all Gnulib regex entry points to `rpl_*` (`rpl_regcomp`, `rpl_re_compile_pattern`, `rpl_re_search`, etc.), preventing linker collisions while preserving 100% upstream GNU regex semantics.

---

## 5. Platform Adaptation Details

### 5.1 Native Windows (`MSVC` `X64`, `X86`, `ARM64`)
- **Compiler & Linker Flags**: `/std:c11 /utf-8 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE /wd4996`, `/STACK:8388608` (`8 MB` stack for deep regex/DFA recursion), and `setargv.obj` on `sed.exe` for native unquoted wildcard expansion (`*.txt`) in `cmd.exe`.
- **Atomic In-Place Editing (`-i`)**: `config.h` maps `mkostemp` to `posix_mkostemp` (`BCryptGenRandom` CSPRNG + exclusive `_sopen_s`) and `rename` to `posix_rename` (`MoveFileExW` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED`), enabling atomic overwrite of existing destination and `.bak` files.
- **Binary vs. Text Streams**: `_O_BINARY` enables GNU `sed`'s `-b` / `--binary` flag so CRLF (`\r\n`) byte sequences can be preserved verbatim when requested.
- **MSVC Invalid Parameter Handlers**: Compiles upstream `lib/msvc-inval.c` and `lib/msvc-nothrow.c` into `libsed_gnulib`.

### 5.2 32-bit Protected-Mode MS-DOS (`OpenWatcom v2` + `DOS/32A`)
- **Flat 32-bit Memory Model**: Compiles with `wcl386 -bt=dos -mf -3r` producing a 32-bit Linear Executable (`LE`) bound to or invoked under the `DOS/32A` protected-mode extender (`512 KB` stack on `SED.EXE`, `160 KB` stack on test suites), lifting 16-bit `64 KB` segment ceilings while maintaining `idx_t` safety.
- **Out-of-Tree AST Lowering (`AutoWinMsvcTransform`)**: Because GNU `sed 4.10` and Gnulib use C99/C11 mid-block declarations and `for`-loop initializers not supported by OpenWatcom `wcl386`, `CMakeLists.txt` runs `auto_win_msvc_transform_sources` to emit C89-lowered copies into `${CMAKE_CURRENT_BINARY_DIR}/transformed_lib` and `transformed_sed` without touching `${SED_SOURCE_DIR}`.
- **Legacy MS-DOS `1.25`, `2.0`, and `4.0` Compatibility**:
  - **Missing `argv[0]` on MS-DOS `1.25` / `2.0`**: Pre-3.0 MS-DOS lacks an environment tail containing the executable path, causing Watcom startup code to pass garbage bytes in `argv[0]`. `config.h` overrides `initialize_main(&argc, &argv)` on Watcom to sanitize empty or non-ASCII `argv[0]` to `"sed"`.
  - **FAT 8.3 & Flat-Directory `mkostemp`**: `sed/utils.c` (`ck_mkstemp`) allocates `"./sedXXXXXX"` (`11` chars), whose `"./"` prefix fails on flat FCB-only MS-DOS `1.25` and whose 9-character stem violates FAT 8.3. `posix_mkostemp` in `auto-win-msvc` strips `"./"` and rewrites the template in place to `SED00000`..`SED99999`.
  - **Handle-Constrained `popen` (`sed_watcom_popen`)**: MS-DOS `1.25` and `2.0` provide tight per-process file handle tables (`5`–`20` handles). `config.h` wraps `popen` with `sed_watcom_popen`, satisfying simple `echo ...` subcommands (`e` / `s///e`) via a scratch file without spawning a nested `COMMAND.COM` when no shell metacharacters are present, falling back to `auto-win-msvc`'s `popen` otherwise.
  - **`DOS32A.EXE` Legacy Binary Patching (`vagrant/dos/Vagrantfile` & `.github/workflows/ci.yml`)**: Standard `DOS32A.EXE` checks `INT 21h AH=30h` for DOS `>= 3.0` and reads uninitialized `INT 2Ah`/`2Fh` IVT vectors on DOS `1.25`/`2.0`. Build/test automation patches a staged copy of `DOS32A.EXE` (and runs a transient `FIX125.COM` IVT/IO.SYS helper on MS-DOS `1.25`) so unmodified 32-bit `SED.EXE` runs across 1982 MS-DOS `1.25`, `2.0`, and `4.0`.

### 5.3 POSIX Coverage Hosts (`Linux` / `macOS`)
- Uses native POSIX `<langinfo.h>`, `<unistd.h>`, `symlink`/`readlink`, and `fwriting.c`.
- When `-DSED_ENABLE_COVERAGE=ON`, compiles `libsed_gnulib`, `sed`, `libsed_testable`, `sed_test_harness`, `sed_test_inproc`, and all five test suites with `-O0 -g --coverage`.

---

## 6. Dual-Target White-Box & Strict-C89 Test Architecture (`tests/`)

### 6.1 Production `sed` vs. `libsed_testable`
To achieve full white-box and panic-path coverage without editing upstream `sed/*.c`:
- **`sed` (`sed.exe` / `SED.EXE`)** compiles `sed/*.c` cleanly with zero test macros.
- **`libsed_testable`** compiles the exact same `sed/*.c` files with `-DSED_TESTABLE_BUILD=1 -Dmain=sed_testable_main` (plus `-Dlint=1` on DOS for clean obstack teardown). When `SED_TESTABLE_BUILD` is defined, `config.h` includes `tests/sed_test_hooks.h`, which:
  1. Redirects `exit()` to `sed_hook_exit()`, which restores leak-tracked resources and `longjmp`s back into `sed_test_inproc.c` (`sed_inproc_runner` / `sed_inproc_call_trap`).
  2. Redirects `fwrite`, `fread`, `fflush`, `ferror`, `fclose`, `mkostemp`, `rename`, `mbrtowc`, `isatty`, and `popen` to deterministic fault-injection wrappers controlled by `struct sed_fault_config`.
  3. Resets static globals across `sed/sed.c` (`initialize_main`), `sed/compile.c` (`obstack_init`), and `sed/execute.c` (`sed_testable_reset_execute`, emitted at the tail of `transformed_sed/execute.c` on DOS or via white-box hooks) so `sed_testable_main` can be invoked hundreds of times inside a single process—critical on MS-DOS `1.25` where nested 32-bit DOS-extender process spawning is memory- and handle-constrained.

### 6.2 Test Suites (8.3 Executable Names)

| Target | Source File | Execution Mode | Scope |
| :--- | :--- | :--- | :--- |
| `t_hygien` | `tests/test_upstream_immutability.c` | Host / Win32 / DOS | NIST FIPS 180-4 SHA-256 vectors, `126`-file upstream SHA-256 manifest verification, generated header token contracts, and `100%` unit coverage of `sed_test_harness.c` & `sed_test_error.h` |
| `t_cli` | `tests/test_sed_cli_options.c` | Spawned + In-Process | `--version`, `--help`, `-n`, `-e`, `-f`, `-i` / `-i.bak` / `-iB_*`, `-b` CRLF preservation, `-E`/`-r`, `-l` & `COLS`, `-s`, `-u`, `-z`, `--follow-symlinks`, `--posix`, `POSIXLY_CORRECT`, `--sandbox`, `--debug`, and exit codes `0`/`1`/`2`/`4` |
| `t_cmds` | `tests/test_sed_addresses_cmds.c` | Spawned (In-Proc on DOS) | All address forms (`N`, `$`, `/re/`, `\crec`, `first~step`, ranges, `0,/re/`, `+N`, `~N`, `!`, `{}`) and all commands (`a`..`z`, `s///`, `y///`, `q`/`Q`, `r`/`R`, `w`/`W`, `v`, `F`, obsolete `L` error) |
| `t_reg410` | `tests/test_sed_410_regressions.c` | Spawned (In-Proc on DOS) | Self-contained C89 port of all **77 upstream `sed-4.10/testsuite/`** cases, including `4.10` headline fixes (`backref-anchor`, `bre-star-star`, `bug80573`, `debug-label`, `posix-backslash-warn`, `posix-bracket-esc`, UTF-8 & DFA/NFA transitions) |
| `t_white` | `tests/test_sed_whitebox.c` | In-Process (`libsed_testable`) | Deterministic I/O, `mkostemp`, `rename`, `mbrtowc`, `isatty`, and `popen` fault injection; `sed/utils.c`, `sed/mbcs.c`, `sed/compile.c`, `sed/execute.c`, `sed/regexp.c`, and `sed/debug.c` edge paths |

On MS-DOS (`SED_TARGET_DOS=ON`), `CMakeLists.txt` additionally registers `6` direct `SED.EXE` end-to-end CTest cases (`sed_dos_e2e_version`, `sed_dos_e2e_subst`, `sed_dos_e2e_ere_palin`, `sed_dos_e2e_bre_star`, `sed_dos_e2e_sandbox`, `sed_dos_e2e_prgfile`) executed via `COMMAND.COM` + `DOS32A.EXE` (`11` total DOS CTest targets).

---

## 7. Packaging & CI/CD Pipeline

- **CPack (`CPackConfig.cmake`, `CPackSourceConfig.cmake`)**:
  - **Windows**: Produces `sed-4.10.0-win-<arch>.zip`, NSIS `.exe` installer (`CPACK_NSIS_MODIFY_PATH=ON`), and WiX `.msi` installer (`CPACK_WIX_UPGRADE_GUID=8F4E5B2A-61D4-4C93-B9E1-72A4D6F0410C`), bundling `LICENSE-GPL3` (upstream `COPYING`) alongside `LICENSE-APACHE`, `LICENSE-CC0`, and `LICENSE-MIT`.
  - **MS-DOS**: Produces `sed-4.10.0-dos32.zip` (plus per-OS `msdos-1.25`, `msdos-2.0`, and `msdos-4.0` ZIPs in CI bundling `SED.EXE` and `DOS32A.EXE`).
- **GitHub Actions (`.github/workflows/ci.yml`)**:
  1. `qa-c89-and-coverage` (`ubuntu-latest`): Runs `VerifyC89Strict.cmake`, `pre-commit`, full C89 Debug coverage build, `VerifyCoverage.cmake` (`lcov`), and post-build SHA-256 immutability re-check.
  2. `build-windows-msvc` (`windows-latest`, `x64` & `arm64`): Builds `sed.exe` + C89 tests under `cl.exe`, runs CTest + `OpenCppCoverage` (`x64`), and packages `ZIP`/`NSIS`/`WIX` installers.
  3. `build-dos-openwatcom` (`ubuntu-latest`): Builds 32-bit DOS `SED.EXE` + strict-C89 (`-za`) test binaries with OpenWatcom v2, packages legacy-patched DOS archives, and boots FreeDOS 1.3 under headless `qemu-system-i386` to run `SED.EXE` and the C89 test binaries.
  4. `release` (`v*` tags): Generates `SHA256SUMS.txt` and publishes all Windows and MS-DOS release artifacts to GitHub Releases.
