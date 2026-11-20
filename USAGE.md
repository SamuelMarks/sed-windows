# Usage Guide (`USAGE.md`)

Complete guide to installing, building, invoking, and testing **GNU `sed` `4.10`** across **Native Windows** (`X64`, `X86`, `ARM64`), **32-bit Protected-Mode MS-DOS** (`1.25`, `2.0`, `4.0`, FreeDOS), and **POSIX** hosts (`Linux`, `macOS`).

For internal build architecture and design invariants, see [`ARCHITECTURE.md`](ARCHITECTURE.md).

---

## 1. Installing Prebuilt Releases

Release artifacts are published on GitHub Releases for every `v*` tag alongside `SHA256SUMS.txt`:

| Platform | Artifact Pattern | Contents / Notes |
| :--- | :--- | :--- |
| Windows `x64` | `sed-4.10.0-win-x64.zip` / `.exe` (NSIS) / `.msi` (WiX) | Native 64-bit `bin/sed.exe` + licenses; NSIS installer optionally adds `sed` to `PATH` |
| Windows `ARM64` | `sed-4.10.0-win-arm64.zip` / `.exe` (NSIS) | Native ARM64 `bin/sed.exe` + licenses |
| MS-DOS (`3.0+` / FreeDOS) | `sed-4.10.0-dos32.zip`, `sed-4.10-msdos-4.0.zip` | 32-bit `SED.EXE` + standard `DOS32A.EXE` extender |
| MS-DOS `1.25` / `2.0` | `sed-4.10-msdos-1.25.zip`, `sed-4.10-msdos-2.0.zip` | 32-bit `SED.EXE` + legacy-patched `DOS32A.EXE` |

Verify downloaded archives on Windows PowerShell or POSIX shells:

```powershell
# PowerShell (Windows)
Get-FileHash .\sed-4.10.0-win-x64.zip -Algorithm SHA256
```

```sh
# Linux / macOS
sha256sum -c SHA256SUMS.txt --ignore-missing
```

---

## 2. Building from Source

### 2.1 CMake Configuration Options

| CMake Variable | Default | Description |
| :--- | :--- | :--- |
| `SED_SOURCE_DIR` | Auto (`../sed` or `FetchContent`) | Path to an unmodified GNU `sed 4.10` source directory; automatically downloads and verifies `sed-4.10.tar.xz` if omitted |
| `AUTO_WIN_MSVC_DIR` | Auto (`../auto-win-msvc` or `FetchContent`) | Path to [`auto-win-msvc`](https://github.com/SamuelMarks/auto-win-msvc) checkout; automatically fetched if omitted |
| `SED_TARGET_DOS` | `ON` under Watcom/DOS, else `OFF` | Enables 32-bit Protected-Mode MS-DOS build (`SED.EXE`, 8.3 test names, `AutoWinMsvcTransform`) |
| `BUILD_TESTING` | `ON` | Builds `libsed_testable` and all five strict ISO C89 test suites (`t_hygien`, `t_cli`, `t_cmds`, `t_reg410`, `t_white`) |
| `SED_ENABLE_COVERAGE` | `OFF` | Enables `--coverage` (`gcov`/`lcov`) on GCC/Clang or `/Zi` (`OpenCppCoverage`) on MSVC and adds the `coverage` build target |
| `CMAKE_BUILD_TYPE` | `Release` (recommended) | Standard CMake build configuration (`Release`, `Debug`, `RelWithDebInfo`, `MinSizeRel`) |

### 2.2 Native Windows (MSVC `X64`, `X86`, `ARM64`)

Using the one-step batch script from a **Developer Command Prompt for VS**:

```bat
REM Build, run CTest, and package Release archives for x64 (default):
release.bat

REM Build for 32-bit x86 (Win32) or ARM64:
release.bat Win32
release.bat ARM64
```

Manual CMake + Ninja workflow:

```bat
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -C Release
```

### 2.3 32-bit Protected-Mode MS-DOS (OpenWatcom v2)

On **Windows** (with `WATCOM` set or installed at `C:\WATCOM`):

```bat
release_dos.bat
```

On **Linux** or **macOS** (set the host binary directory matching your OS/CPU: `binl64` on Linux `x86_64`, `arml64` on Linux `ARM64`, `armo64` on macOS Apple Silicon, or `bino64` on Intel macOS):

```sh
export WATCOM=/opt/watcom
export PATH="$WATCOM/binl64:$WATCOM/binw:$PATH"
export INCLUDE="$WATCOM/h"
export EDPATH="$WATCOM/eddat"
export WIPFC="$WATCOM/wipfc"

cmake -G "Watcom WMake" -S . -B build_dos \
  -DCMAKE_SYSTEM_NAME=DOS \
  -DCMAKE_C_COMPILER=wcl386 \
  -DCMAKE_BUILD_TYPE=Release \
  -DSED_TARGET_DOS=ON \
  -DBUILD_TESTING=ON
cmake --build build_dos
```

### 2.4 Linux / macOS Host Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

---

## 3. Command-Line Reference (`sed` / `sed.exe` / `SED.EXE`)

### 3.1 Synopsis

```text
sed [OPTION]... {script-only-if-no-other-script} [input-file]...
```

If neither `-e`/`--expression` nor `-f`/`--file` is supplied, the first non-option argument is interpreted as the `sed` script and all remaining arguments are input file paths (`-` or no files reads standard input).

### 3.2 Options

| Short | Long Option | Description |
| :--- | :--- | :--- |
| `-n` | `--quiet`, `--silent` | Suppress automatic printing of pattern space at the end of each cycle |
| | `--debug` | Print the compiled `sed` program and annotate input, pattern space, and regex registers on stdout |
| `-e SCRIPT` | `--expression=SCRIPT` | Append `SCRIPT` to the commands to execute (may be specified multiple times) |
| `-f FILE` | `--file=FILE` | Append commands read from `FILE` (`-f -` reads script from stdin) |
| | `--follow-symlinks` | Follow symbolic links when editing in place (`-i`) on platforms supporting symlinks |
| `-i[SUFFIX]` | `--in-place[=SUFFIX]` | Edit files atomically in place; creates a backup if `SUFFIX` is given (e.g. `-i.bak` or `-i"orig_*"`) |
| `-b` | `--binary` | Open files in binary mode (`O_BINARY`), treating `\r` as an ordinary byte and preserving CRLF (`\r\n`) |
| `-l N` | `--line-length=N` | Specify line-wrap column width `N` for the `l` (unambiguous list) command (`0` disables wrapping; default `70`) |
| | `--posix` | Disable all GNU `sed` extensions and enforce strict POSIX.1-2024 conformance |
| `-E`, `-r` | `--regexp-extended` | Use Extended Regular Expressions (ERE) instead of Basic Regular Expressions (BRE) |
| `-s` | `--separate` | Treat multiple input files as independent line streams (resetting line numbers and `$` per file; implied by `-i`) |
| | `--sandbox` | Disable external command execution and file creation/reading commands (`e`, `s///e`, `r`, `R`, `w`, `W`) |
| `-u` | `--unbuffered` | Read minimal input chunks and flush output after every write |
| `-z` | `--null-data`, `--zero-terminated` | Separate records by ASCII `NUL` (`\0`) bytes instead of newline (`\n`) |
| | `--help` | Display usage help and exit with status `0` |
| | `--version` | Display version (`GNU sed 4.10`), copyright, and license notice, then exit with status `0` |

### 3.3 Environment Variables

| Variable | Effect |
| :--- | :--- |
| `POSIXLY_CORRECT` | Enables POSIX compatibility mode (allowing GNU extensions that do not conflict with POSIX, unlike `--posix` which disables all GNU extensions) |
| `COLS` | Sets default wrap width (`COLS - 1`) for the `l` command when `COLS > 1` and `-l` is not specified |
| `LC_ALL`, `LC_CTYPE`, `LANG` | Controls multibyte character decoding (`UTF-8` vs single-byte `C` locale), collation, and character classes (`[[:alpha:]]`, etc.) |

### 3.4 Exit Codes

| Exit Code | Internal Constant | Meaning |
| :--- | :--- | :--- |
| `0` | `EXIT_SUCCESS` | Successful execution |
| `1` | `EXIT_BAD_USAGE` | Invalid CLI flag, missing script, script syntax error, or command forbidden by `--posix` / `--sandbox` |
| `2` | `EXIT_BAD_INPUT` | One or more input files could not be opened or read |
| `4` | `EXIT_PANIC` | Unrecoverable runtime/I/O error (missing `-f` script file, `-i` with no files, disk full / broken pipe panic) |
| `0`..`255` | User-specified | Explicit exit code passed to `q <code_num>` or `Q <code_num>` |

---

## 4. GNU `sed 4.10` Address & Command Reference

### 4.1 Addresses
- `N`: Match line number `N` (`1`-based; `0` valid in `0,/REGEXP/` and `0r`).
- `$`: Match the last line of input (or last line of each file with `-s` / `-i`).
- `/REGEXP/` or `\cREGEXPc`: Match lines matching `REGEXP` using `/` or custom delimiter `c` (`I`/`i` suffix for case-insensitive, `M`/`m` for multi-line).
- `FIRST~STEP`: GNU step address matching `FIRST`, `FIRST + STEP`, `FIRST + 2*STEP`, ... (e.g. `1~2` matches odd lines, `0~2` matches even lines).
- `ADDR1,ADDR2`: Inclusive range from `ADDR1` through `ADDR2`.
- `ADDR1,+N`: Match `ADDR1` and the following `N` lines.
- `ADDR1,~N`: Match `ADDR1` through the next line whose number is a multiple of `N`.
- `ADDR!`: Negate address or range (apply command to non-matching lines).

### 4.2 Commands

| Command | Summary |
| :--- | :--- |
| `s/REGEXP/REPL/FLAGS` | Substitute matches (`g` global, `1`..`N` Nth match, `p` print, `i`/`I` ignore case, `m`/`M` multiline, `w FILE` write, `e` execute replacement via shell; `REPL` supports `&`, `\1`..`\9`, `\L`, `\U`, `\l`, `\u`, `\E`) |
| `y/SRC/DST/` | Transliterate characters in `SRC` to corresponding characters in `DST` (multibyte-aware) |
| `d` / `D` | Delete pattern space (`D` deletes up to first newline and restarts cycle if non-empty) |
| `p` / `P` | Print pattern space (`P` prints up to first newline) |
| `n` / `N` | Read next line (`n` replaces pattern space after default print; `N` appends `\n` + next line) |
| `h` / `H`, `g` / `G`, `x` | Copy/append pattern space to hold space (`h`/`H`), hold space to pattern space (`g`/`G`), or swap (`x`) |
| `a TEXT`, `i TEXT`, `c TEXT` | Append after cycle (`a`), insert immediately (`i`), or replace matching line/range (`c`) |
| `r FILE` / `R FILE` | Queue whole `FILE` (`r`) or single next line of `FILE` (`R`) for output at end of cycle |
| `w FILE` / `W FILE` | Write pattern space (`w`) or first line of pattern space (`W`) to `FILE` |
| `q [CODE]` / `Q [CODE]` | Quit immediately (`q` prints pattern space unless `-n`; `Q` quits silently without printing) |
| `:LABEL`, `b [LABEL]`, `t [LABEL]`, `T [LABEL]` | Define `:LABEL`; branch unconditionally (`b`), on successful `s///` (`t`), or when no `s///` succeeded (`T`) |
| `l [WIDTH]` | Print pattern space unambiguously with C escapes and trailing `$` |
| `=` / `F` | Print current line number (`=`) or current input filename (`F`) followed by newline |
| `z` | Zap (empty) pattern space without restarting cycle |
| `v [VERSION]` | Fail if GNU `sed` is older than `VERSION` (e.g. `v 4.10` succeeds; `v 99.0` exits non-zero) |
| `{ ... }` / `# comment` | Group commands under an address (`{ ... }`); comment to end of line (`#n` on line 1 enables `-n`) |

---

## 5. Platform-Specific Recipes & Best Practices

### 5.1 Windows (`cmd.exe` vs. PowerShell)

**Quoting Rules**:
- In **`cmd.exe`**, single quotes `'...'` are literal characters—always wrap `sed` scripts in **double quotes** `"..."`:
  ```bat
  sed.exe -E "s/([0-9]{4})-([0-9]{2})-([0-9]{2})/\3\/\2\/\1/g" dates.txt
  ```
- In **PowerShell**, prefer **single quotes** `'...'` so `$` (`$p`, `foo$`) and backslashes are not interpolated by PowerShell:
  ```powershell
  sed.exe -n -E 's/^version = "([^"]+)"/\1/p' Cargo.toml
  ```

**CRLF Line Endings & `-b` (`--binary`)**:
- Without `-b`, Windows text streams translate `\r\n` to `\n` on input and emit `\r\n` on output, so `s/foo$/bar/` matches lines ending in `\r\n`.
- With `-b` (`--binary`), exact bytes are preserved without translation: `\r` remains at the end of pattern space before `\n`, allowing lossless conversion between CRLF and LF:
  ```bat
  REM Convert CRLF to LF in-place using binary mode:
  sed.exe -b -i "s/\r$//" script.sh

  REM Convert LF to CRLF in-place using binary mode:
  sed.exe -b -i "s/\r\?$/\r/" readme.txt
  ```

**In-Place Editing & Wildcards on Windows**:
```bat
REM Edit all *.ini files in place, keeping *.ini.bak backups (overwrites old .bak atomically):
sed.exe -i.bak "s/^debug=false/debug=true/" *.ini

REM Place backups in a prefix pattern:
sed.exe -i"orig_*" "s/localhost/127.0.0.1/g" app.conf
```

### 5.2 MS-DOS (`1.25`, `2.0`, `4.0`, and FreeDOS)

1. **Invoking with `DOS32A.EXE`**:
   Keep `DOS32A.EXE` in the current directory or on `PATH` and run:
   ```bat
   DOS32A.EXE SED.EXE --version
   DOS32A.EXE SED.EXE -f TRANSFORM.SED INPUT.TXT > OUTPUT.TXT
   ```
2. **Prefer `-f SCRIPT.SED` for Complex Expressions**:
   MS-DOS `COMMAND.COM` splits unquoted `,`, `;`, `=`, and interprets `<`, `>`, and `|` before passing arguments to child programs. Putting scripts containing ranges (`1,10p`), multiple commands (`s/a/b/;s/c/d/`), or alternation (`|`) into an 8.3 `.SED` file avoids all `COMMAND.COM` parsing quirks:
   ```bat
   ECHO 1,5s/foo/bar/g > EDIT.SED
   DOS32A.EXE SED.EXE -f EDIT.SED DATA.TXT
   ```
3. **8.3 Filename Conventions for `-i` Backups**:
   On FAT12/FAT16 filesystems, appending `.bak` to `DATA.TXT` (`DATA.TXT.bak`) has two dots. Either edit a stem without an extension (`-i.BAK DATA`) or use a prefix pattern (`-iB_* DATA.TXT`, which writes backup `B_DATA.TXT`).
4. **MS-DOS `1.25` Flat Directory Note**:
   MS-DOS `1.25` predates hierarchical subdirectories (`MKDIR`/`CHDIR`); place `DOS32A.EXE`, `SED.EXE`, script files, and target files directly on the root drive (`A:` or `C:`).

---

## 6. Running Verification, Coverage & Vagrant MS-DOS Suites

### 6.1 Running CTest & Individual Test Binaries

```sh
# Run all registered CTest suites:
ctest --test-dir build --output-on-failure -V

# Run an individual C89 test binary directly (passing path to sed executable as argv[1]):
./build/t_hygien ./build/sed
./build/t_cli    ./build/sed
./build/t_cmds   ./build/sed
./build/t_reg410 ./build/sed
./build/t_white  ./build/sed
```

### 6.2 Strict ISO C89 Hygiene & 100% Coverage Gate

```sh
# Static C89 syntax, zero // comments, zero stray .c/.h files, and 100% doc coverage:
cmake -DPROJECT_ROOT=. -P cmake/VerifyC89Strict.cmake

# Full pre-commit suite (clang-format, cpplint, line endings, C89 & doc gates):
pre-commit run --all-files

# Build and enforce gcov/lcov coverage gates:
cmake -S . -B build_cov -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DSED_ENABLE_COVERAGE=ON
cmake --build build_cov --parallel
cmake --build build_cov --target coverage
```

### 6.3 Automated MS-DOS `1.25`, `2.0`, and `4.0` Vagrant Verification

Requires Vagrant, QEMU (`qemu-system-i386`), and registered `bento/msdos-{1.25,2.0,4.0}` base boxes:

```sh
cd vagrant/dos

# Boot each guest, auto-provision OpenWatcom v2 + patched DOS32A.EXE, build, and run all 11 CTest targets:
vagrant up --provider=qemu msdos-1.25
vagrant up --provider=qemu msdos-2.0
vagrant up --provider=qemu msdos-4.0

# Re-run provisioning & verbose CTest on a running guest:
CTEST_VERBOSE=1 vagrant provision msdos-4.0
```
