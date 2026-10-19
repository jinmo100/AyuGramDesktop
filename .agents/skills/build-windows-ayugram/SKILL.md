---
name: build-windows-ayugram
description: "Build AyuGram Desktop locally on Windows with MSVC v14.44 and all dependencies safely isolated to drive E:, avoiding C: drive or WSL2 disk exhaustion. Use when the user asks to compile, build, package, or debug AyuGram on Windows."
---

# Build AyuGram Desktop on Windows (Drive E: Isolated)

Complete the authorized native Windows Debug build through a fresh exit code, successful link log, and verified executable. A detached process or completed dependency stage is progress, not completion. Keep build writes under `E:\TBuild`; inspect existing artifacts before rebuilding dependencies.

## 1. Architecture & Storage Isolation

Telegram / AyuGram Desktop is a heavy C++ project. Full compilation requires 50+ third-party static libraries (Qt 5.15, WebRTC, FFmpeg, OpenSSL, etc.) taking ~60GB of intermediate objects and debug symbols.

### Directory Layout (Rooted at `E:\TBuild`)
All build inputs, caches, and outputs must be strictly isolated to an NTFS partition with >= 80GB free space (`E:\TBuild`):

```text
E:\TBuild\
├── tdesktop\             # Full Git source checkout with all 36 submodules
│   ├── Telegram\
│   └── out\Debug\        # Build outputs (AyuGram.exe; size varies by configuration)
├── Libraries\win64\      # Compiled third-party C++ static libraries
├── ThirdParty\           # Downloaded tools (msys64, jom, gyp, NuGet, rust)
├── Temp\                 # Redirection target for %TEMP% and %TMP%
└── NuGet\                # Redirection target for %NUGET_PACKAGES%
```

### Critical Environment Redirections
Before invoking any build or python scripts, redirect all temporary and cache environment variables:
```cmd
set "TBUILD=E:\TBuild"
set "TEMP=E:\TBuild\Temp"
set "TMP=E:\TBuild\Temp"
set "NUGET_PACKAGES=E:\TBuild\NuGet"
set "NUGET_HTTP_CACHE_PATH=E:\TBuild\NuGet\http-cache"
set "NUGET_PLUGINS_CACHE_PATH=E:\TBuild\NuGet\plugins-cache"
```
Failure to redirect `%TEMP%` will silently write 5-10GB of decompression artifacts to `C:\Users\<user>\AppData\Local\Temp`.

---

## 2. Shell Architecture: CMD Execution vs PowerShell Orchestration

When working in Windows C++ build environments, adhere strictly to the **CMD Execution + PowerShell Orchestration** pattern:

### Why CMD (`cmd.exe`) is Required for the Execution Layer
1. **MSVC Environment Inheritance**: Visual Studio's `vcvars64.bat` sets ~20 environment variables (`INCLUDE`, `LIB`, `PATH`, etc.). In `cmd.exe`, `call vcvars64.bat` applies them directly to the current session. In PowerShell, running a batch script spawns a child process and discards all environment changes upon return.
2. **Special Character Immunity**: C++ compiler defines (`-D"KHRONOS_STATIC="`, `/D_UNICODE`, `-DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>"`) contain `$`, quotes, and commas. PowerShell aggressively parses `$` as variables (stripping `$arch`, `$CONFIG`), while `cmd.exe` passes literal command lines faithfully.
3. **Upstream Native Tooling**: Qt's `configure.bat`, Telegram's `win.bat`, `jom.exe`, and `nmake.exe` all assume `COMSPEC` (`cmd.exe`) as their subshell runner.

### Why PowerShell is Used for Orchestration & Governance
- **Detached Foreground Execution**: Spawns independent native Windows GUI terminal windows via `Start-Process cmd.exe ...` that persist across AI turn timeouts.
- **Non-blocking Status Monitoring**: Lightweight inspection of process trees (`Get-CimInstance Win32_Process`) and CPU/memory metrics without locking the runner.
- **Source synchronization**: When editing from WSL, use the session's file-edit tools and preserve LF/no-BOM. Synchronize intended source fixes to the E: checkout and compare bytes before building; native PowerShell defaults can change encoding or line endings.

---

## 3. Prerequisites & Toolchain Verification

1. **Visual Studio 2022 BuildTools (v14.44-17.14)**:
   - Toolset: MSVC v143 (v14.44) x64 compiler (`cl.exe`).
   - SDK: Windows 11 SDK (`10.0.22621.0` or `10.0.26100.0`).
   - **ATL Component (Crucial)**: `Microsoft.VisualStudio.Component.VC.ATL` must be installed. Without it, Breakpad/Crashpad compilation fails with missing `atlbase.h`. Install via:
     ```cmd
     "C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools" --add Microsoft.VisualStudio.Component.VC.ATL --passive --norestart
     ```
2. **Python 3.10+**: Available in system `PATH` (e.g. `E:\Scoop\apps\python\current`).
3. **Git**: Configured and in `PATH`.
4. **Submodules**: All 36 Git submodules in `tdesktop` must be initialized:
   ```bash
   git submodule update --init --depth=1
   ```

---

## 4. Critical Traps & Proven Fixes

### Trap 1: MSYS2 Package Rename (`diffutils`)
- **Symptom**: Step `[2/34](ThirdParty/msys64)` fails with `error: target not found: mingw-w64-x86_64-diffutils`.
- **Root Cause**: Upstream MSYS2 dropped `mingw-w64-x86_64-diffutils` from the mingw64 repo in favor of base `diffutils`.
- **Fix**: In `Telegram/build/prepare/prepare.py`, change `mingw-w64-x86_64-diffutils ^` to `diffutils ^`.

### Trap 2: Overseas MSYS2 Mirrors Network Timeout (China / High Latency)
- **Symptom**: `pacman -Syu` appears completely frozen / blocked for 10+ minutes with no output.
- **Root Cause**: MSYS2 default `mirrorlist.msys` and `mirrorlist.mingw` prioritize `mirror.msys2.org` which times out repeatedly.
- **Fix**: Prepend domestic Tsinghua (TUNA) and Aliyun mirrors to `mirrorlist.msys` and `mirrorlist.mingw`:
  ```text
  Server = https://mirrors.tuna.tsinghua.edu.cn/msys2/msys/$arch/
  Server = https://mirrors.aliyun.com/msys2/msys/$arch/
  Server = https://mirrors.ustc.edu.cn/msys2/msys/$arch/
  ```

### Trap 3: Upstream Python Typo in Stale Rebuild Logic
- **Symptom**: When `silent` mode was passed or stages were stale, the script skipped auto-rebuild or waited for manual keyboard input.
- **Root Cause**: `prepare.py` line 392 contained `checkResult == 'Rebuild'` (equality comparison) instead of `checkResult = 'Rebuild'` (assignment).
- **Fix**: Correct the statement to `checkResult = 'Rebuild'`.

### Trap 4: CMD Batch File Parenthesis Parsing Trap
- **Symptom**: Running batch scripts with `if not exist "%VCVARS%" (...)` fails with `\Microsoft was unexpected at this time`.
- **Root Cause**: The variable `%VCVARS%` contains `(x86)`. In standard Windows `cmd.exe`, the closing parenthesis in `(x86)` prematurely closes the `if (...)` block.
- **Fix**: Use `goto :label` branching instead of multi-line parenthesized blocks:
  ```cmd
  if not exist "%VCVARS%" goto :no_vcvars
  goto :has_vcvars
  :no_vcvars
  ...
  :has_vcvars
  ```

### Trap 5: WSL2 Virtual Disk Expansion
- **Symptom**: C: drive runs out of disk space despite building on WSL.
- **Root Cause**: WSL2 stores its `ext4.vhdx` on C: drive. Compiling inside `/home/...` inflates the VHDX file.
- **Fix**: Clone and build strictly on the Windows NTFS drive (`E:\TBuild\tdesktop`).

### Trap 6: libjxl v0.12.0 MSVC C3861 `_sub_overflow_i32` Missing `<intrin.h>`
- **Symptom**: Step `[23/34](Libraries/libjxl)` fails with `common.h(60): error C3861: '_sub_overflow_i32': identifier not found`.
- **Root Cause**: `lib/jxl/base/common.h` checks `#if JXL_COMPILER_MSVC` to include `<intrin.h>` before `compiler_specific.h` is included, so the macro evaluates to 0 and `<intrin.h>` is never included under newer MSVC compilers (MSVC 19.44 / VS 2022 v17.14).
- **Fix**: Inject hot-patch command in `stage('libjxl')` right after `cd libjxl`:
  ```cmd
  powershell -Command "(Get-Content lib/jxl/base/common.h) -replace '#if JXL_COMPILER_MSVC', '#if defined(_MSC_VER)' | Set-Content lib/jxl/base/common.h"
  ```

### Trap 7: Qt 5.15 `qmake.exe not recognized` & `Feature 'openssl-linked' failed`
- **Symptom**: Step `[30/34](Libraries/qt_5.15.19)` fails with `link /OUT:..\bin\qmake.exe ...` followed by `'qmake.exe' is not recognized as an internal or external command`.
- **Root Cause**:
  1. `configure.bat` attempts to bootstrap `qmake.exe` using `jom`. `jom 1.1.3` fails silently on the 1,850+ character link command line in Windows `cmd.exe`, leaving `qmake.exe` unbuilt.
  2. Qt 5's configure checks `$OPENSSL_LIBS` in the environment rather than `OPENSSL_LIBS_DEBUG` on the CLI.
- **Fix**:
  1. Pass `-make-tool nmake` to `configure` so it uses MSVC `nmake` for the 2-second qmake bootstrap.
  2. Export `SET "OPENSSL_LIBS=%OPENSSL_LIBS_DIR%.dbg\libssl.lib %OPENSSL_LIBS_DIR%.dbg\libcrypto.lib Ws2_32.lib Gdi32.lib Advapi32.lib Crypt32.lib User32.lib"` right before calling `configure`.

### Trap 8: Qt 5.15 `qmake-aux.pro` jom Link Error 2
- **Symptom**: During Qt 5.15 build, `jom` fails at `sub-qmake-qmake-aux-pro-make_first` with `link /OUT:..\qmake\qmake.exe ... [E:\...\qmake.exe] Error 2`.
- **Root Cause**: `qmake-aux.pro` marked `qmake_exe.CONFIG = phony`, forcing `jom` to re-link `qmake.exe` during the main parallel build, which overflows `jom`'s internal buffer.
- **Fix**: Inspect the installed patch and generated command before changing it. In the verified recovery, `0043-fix-qmake-aux-jom-link.patch` used `@rem` for the auxiliary target command to preserve the already-built qmake. Verify qmake runs and installation can find its expected path; do not assume a patch filename implies particular contents.

### Trap 9: `qtbase` Patch Application Failure on Re-run
- **Symptom**: Re-running `prepare.py` fails with `ERROR: Applying patch %%~nxi failed!`.
- **Root Cause**: If a previous build modified `qtbase`, `git apply` fails when trying to re-apply patches over already patched code.
- **Fix**: Inspect `git status`, patch contents, and `git apply --reverse --check` to distinguish an already-applied patch from a partial/conflicting patch. Reuse completed stages when possible. Preserve local fixes before any targeted repair; blanket `git checkout -- .` / `git clean -fd` can destroy the qmake recovery or other intentional edits.

### Trap 10: Gigabytes of Useless Git Commit History on Large Libraries
- **Symptom**: Clones of `openssl3`, `ffmpeg`, `ada` take 10-20 minutes or abort on network interruptions.
- **Root Cause**: Cloning full commit history across 30 years of commits for fixed tag releases.
- **Fix**: Always use `--depth=1` on fixed-tag clone commands in `prepare.py` (`git clone --depth=1 -b <tag> ...`), reducing transfer from 800MB to 20MB.

### Trap 11: AI Agent Synchronous Runner Timeouts on Heavy Builds
- **Symptom**: Running heavy compilations (Qt 5.15, FFmpeg) synchronously inside AI tool wrappers causes `Command aborted`.
- **Root Cause**: Qt 5.15 compilation takes 15-25 minutes across thousands of translation units, exceeding single-command timeouts.
- **Fix**: Launch heavy builds into an independent desktop CMD window using `Start-Process cmd.exe -ArgumentList '/k E:\TBuild\build_ayugram.bat' -WorkingDirectory 'E:\TBuild'` and monitor progress asynchronously with non-blocking PowerShell queries.

### Trap 12: `prepare.py` Internal Command Validator Forbids Raw `$`
- **Symptom**: `prepare.py` halts immediately with `[ERROR] Bad command: <command>` and exit code 1.
- **Root Cause**: `winFailOnEach` checks `if re.search(r'\$[^<]', command): error('Bad command: ' + command)`. It permits `$` ONLY when followed by `<` (CMake generator expressions like `$<CONFIG:Debug>`). Inserting PowerShell `-replace [regex]::Escape('\$(MAKE)')` triggers this regex and fails validation.
- **Fix**: Use Git patches (e.g. `0043-fix-qmake-aux-jom-link.patch`) rather than injecting PowerShell commands containing `$` into `prepare.py` command blocks.

### Trap 13: Qt Stage Missing Explicit `%QT%` Environment Variable Definition
- **Symptom**: Build halts with `fatal: Remote branch v-lts-lgpl not found in upstream origin` or patch loop fails because `..\..\patches\qtbase_\*` path is invalid.
- **Root Cause**: In `prepare.py`, `$QT` is written inside common command blocks before `win:`. On Windows, `winFailOnEach` substitutes `$QT` with `%QT%`, but `QT` was never exported as an environment variable in CMD. In batch, undefined `%QT%` expands to empty string, turning `qt_%QT%` into `qt_` and `v%QT%-lts-lgpl` into `v-lts-lgpl`.
- **Fix**: In `prepare.py` stage `qt_` win block, explicitly export `SET QT=""" + qt + """` and inject `""" + qt + """` directly into python string interpolation for clone and directory paths.

### Trap 14: Patch Whitespace and Line Ending Incompatibilities on Windows
- **Symptom**: `git apply %%i -v` in batch loop fails with `error: patch failed` or `error: corrupt patch`.
- **Root Cause**: CRLF vs LF differences on Windows checkouts cause strict patch matching to fail.
- **Fix**: Always pass `--ignore-space-change --ignore-whitespace` to `git apply %%i -v` in `prepare.py`.

### Trap 15: Compiled Qt Is Only Partially Installed
- **Symptom**: Static libraries exist, but configuration cannot find qmake, QtSvg, or image-plugin metadata.
- **Recovery**: Inspect source/build tree `Libraries\win64\qt_5.15.19` and install prefix `Libraries\win64\Qt-5.15.19` separately. Check qmake with `-query`; library files alone do not prove installation is complete.
- Reuse the existing `qtbase\bin\qmake.exe` if valid. In this recovery, installation also needed a copy at `qtbase\qmake\qmake.exe` and the prefix's `bin\qmake.exe`. Copy only to a missing path after verifying the source; preserve any existing binary.
- In the initialized MSVC CMD, run `E:\TBuild\ThirdParty\jom\jom.exe -j8 install` from the existing Qt build root. If QtSvg or image-plugin metadata remains missing, run the same install target separately from `qtsvg` and `qtimageformats`. Inspect each exit code and log, then reconfigure the application. Full Qt recompilation is unnecessary when the compiled artifacts are intact.

### Trap 16: Interrupted Submodules Contain Only `.git`
- **Symptom**: CMake cannot find a submodule's source/CMake files despite an existing submodule HEAD.
- **Recovery**: Inspect that exact submodule's HEAD, index, worktree contents, and staged changes. Only when the directory is proven empty apart from `.git`, the index has no files, and HEAD contains the expected sources, restore with `git -C <submodule> restore --source=HEAD --staged --worktree -- .`.
- This restored `Telegram/codegen`, `lib_icu`, `lib_qr`, `lib_spellcheck`, `lib_storage`, `lib_tl`, `lib_ui`, and `lib_webrtc` in the verified build. These paths are examples, not a blanket restore list; preserve populated or modified checkouts.

### Trap 17: WebView2 Missing from the Configured NuGet Source
- **Symptom**: Configuration cannot install `Microsoft.Web.WebView2` from a local NuGet source.
- **Recovery**: Read the required version from current CMake files. With the E: environment redirections set, install into the exact package layout expected by CMake. Verified command for the then-required version:
  ```cmd
  E:\TBuild\ThirdParty\NuGet\nuget.exe install Microsoft.Web.WebView2 -Version 1.0.1901.177 -ExcludeVersion -OutputDirectory E:\TBuild\tdesktop\out\packages -Source https://api.nuget.org/v3/index.json -NonInteractive -DirectDownload -NoCache
  ```
- Confirm package installation and rerun configuration. The version is historical evidence, not a pin to apply to future source trees.

### Trap 18: Stale Exit Markers and Misnamed Artifacts
- Remove the previous run's exit marker before launch; record the process ID and start time. Check that all failure paths write a numeric result, including environment setup and configuration failures.
- A prior `1` or `9009` file can remain while a later build is still running. Read only the current run's marker after its process exits, and inspect the stage log.
- The CMake target is `Telegram`, but the verified executable is `out\Debug\AyuGram.exe`. Use the successful link line to resolve the actual output name. An old executable's existence is insufficient evidence.

### Trap 19: Prepare Stage Re-entry Deletes Useful Work
- Inspect the current `prepare.py` stage-selection interface before resuming. Completed dependencies should be reused; explicitly selecting a stage can force rebuilding or removal.
- Check directory tests against the stage's actual parent, such as `os.path.join(stage['directory'], stage['name'])`, rather than assuming CWD is the library parent.
- Keep CMD patch loops on one line inside generated `winFailOnEach` blocks; injected per-line error checks can break multi-line `for (...)` syntax. Preserve inherited prepare fixes and Qt sources during recovery.

### Trap 20: Build Succeeds but the Feature Paints Nothing
- **Symptom**: Top Posts toolbar appears while cards, progress, and empty-state text are missing.
- **Verified cause**: `resizeGetHeight()` only calculates a height. Calling it directly and discarding the result leaves widget geometry unchanged. Use `resizeToWidth()` to apply it, and propagate the scroll area's width to its owned content during section layout. Banner visibility changes must apply the new height before notifying layout subscribers.
- **Regression**: Run `.scratch/channel-top-posts/test_layout_regression.py` with the repository root and an E: output directory, then follow its documented native CMake/build/run commands. It extracts the actual layout method bodies and tests real Qt widgets through a minimal marginless adapter. Six checks failed before the fix and passed afterward; the harness does not exercise session/network behavior.
- **Runtime evidence**: Verify actual rows, sorting, and a deliberately empty filter after launching the rebuilt executable. Record exactly what passed; neither a visible toolbar nor a successful build establishes usability.
- **Windows inspection**: If the session UI tools cannot see Windows roots, PowerShell UI Automation can inspect `AutomationElement.FromHandle` for the exact checkout process and invoke named controls. Scope all queries/actions to that process; capture only the feature pane. Call `SetProcessDPIAware` before Win32 screenshots so UI Automation's screen rectangles agree with captured pixels.
- A running checkout executable may belong to the user's test. Obtain explicit authorization before stopping it, verify its exact path immediately before termination, and preserve other clients. Stop further interaction when the user takes over testing.

---

## 5. Standard Build Execution Workflow

### Step 1: Inspect and Select the Entry Point
- [ ] Confirm the user requested a build. Read the checkout instructions and current build scripts; keep Release builds out of this workflow.
- [ ] Verify the active source checkout, E: checkout HEAD and local changes, available E: disk space, MSVC/SDK, CMake, Python, and Qt prefix. The validated toolchain was MSVC 19.44 / v14.44, SDK 10.0.22621.0, Qt 5.15.19; inspect current requirements instead of treating these as permanent versions.
- [ ] Initialize MSVC with `call vcvars64.bat` in the same CMD process that runs configure/build. Apply the E: temp and NuGet redirections from section 1 and create those directories if needed.
- [ ] For a fresh environment, inspect and run `E:\TBuild\build_ayugram.bat`, including its `Telegram\build\prepare\win.bat skip-release silent` dependency step. For existing dependencies, use the incremental path. Recover a missing dependency individually after inspecting stage-selection behavior.
- [ ] When source fixes originate in WSL, synchronize the intended files to the E: checkout, compare bytes, and retain unrelated existing edits.

### Step 2: Configure and Compile Incrementally
Use `E:\TBuild\compile_debug.bat` when present, after reviewing its contents. It should initialize the MSVC environment, set the E: caches, clear the old exit marker, configure x64 with the already-authorized API configuration, and then run:

```cmd
cmake --build E:\TBuild\tdesktop\out --config Debug --target Telegram --parallel 12
```

Reuse the configured tree directly when configuration inputs have not changed. Reconfigure after build configuration changes, dependency recovery, or source/style additions as required by the project. Tune parallelism to available memory; Debug linking can use over 10 GiB.

A launcher must record configuration and build logs separately, write a numeric exit marker on every terminal path, and create the portable directory only after a successful build. Do not run another build against the same tree while one is active. Launch from WSL with:

```bash
powershell.exe -NoLogo -NoProfile -Command 'Start-Process cmd.exe -ArgumentList "/d /c E:\TBuild\compile_debug.bat" -WorkingDirectory "E:\TBuild" -PassThru | Select-Object Id'
```

Monitor the recorded process, compiler/linker activity, log timestamp, and concrete diagnostics. Keep waits at most 60 seconds and provide progress updates during long builds. If compilation fails, fix the reported source/API mismatch, synchronize it, and resume until the build succeeds or a concrete blocker requires user action. Static reviews do not replace actual MSVC compilation.

### Step 3: Verify Completion and Optional Runtime Testing
- [ ] The current build process has exited and its fresh numeric marker is `0`.
- [ ] The current build log contains the successful link output and no unresolved compile/link errors.
- [ ] The linked executable exists; record its full path, modification time, version, byte size, and SHA-256. Account for unchanged timestamps on a no-op incremental build.
- [ ] Confirm `TelegramForcePortable` exists beside the executable before any launch. The directory does not prove runtime behavior; inspect project portable-mode handling and avoid copying an installed account's data.
- [ ] Report compile/link verification separately from launch, login, scanning, and UI tests. Perform runtime testing only within the requested scope; compilation alone does not establish feature correctness.
- [ ] Update the handoff and relevant notes, run `npm run verify-notes` / `git diff --check` when applicable, and verify synchronization of modified files.

Verified output layout:

```text
E:\TBuild\tdesktop\out\Debug\
├── AyuGram.exe
└── TelegramForcePortable\
```

Historical acceptance snapshot (2026-10-20): native Windows x64 Debug configure, language/style generation, compile, and link succeeded with exit `0`; `AyuGram.exe` version `7.2.9.0`, 520677376 bytes. Portable marker creation was verified; application launch and authenticated Top Posts interactions were not tested. Keep future acceptance evidence in the run's handoff rather than assuming this snapshot proves later builds.

Follow-up on 2026-10-20: fixed the blank-content geometry regression, rebuilt successfully, and verified real channel rows plus reaction/view/comment ordering and the Files empty-state message in the running application. The user took over further testing; full rescan, FLOOD_WAIT recovery, and long-running sync remain outside this runtime evidence.
