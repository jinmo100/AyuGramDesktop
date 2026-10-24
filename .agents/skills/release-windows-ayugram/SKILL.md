---
name: release-windows-ayugram
description: "发布 AyuGram-Jinmo 的 Windows x64 便携 Debug 测试版到 jinmo100/AyuGramDesktop。用户要求发版、打测试包、恢复上传失败的草稿或核验已发布资产时使用；复用 E 盘本机发布脚本，优先 PowerShell 7。"
---

# Windows 便携测试版发布

只发布与已推送源码、成功构建和实际布局回归对应的产物。以仓库发布脚本为门禁来源，沿用干净构建树和既有工具链；运行中的用户程序由用户退出。

仓库根目录是本文件的 `../../..`。先读根目录的 `docs/fork-release.md`、`tools/release-windows.ps1` 和 `tools/fork_release.py`，核对当前参数、配置和证明有效期。此 skill 编排现有实现，不创建第二套发布器。

## 执行清单

- [ ] 确定本次是新发布、恢复草稿还是只读核验。
- [ ] 核对完整源码 SHA、E 盘工作区、递归子模块与进程。
- [ ] 新发布执行原生构建和回归；恢复使用匹配的有效构建证明。
- [ ] 启动器父进程是 `cmd.exe`，不是从 WSL 起 `Start-Process pwsh`。
- [ ] 核验公开状态、精确标签和三项发布资产。
- [ ] 报告下载链接、校验值、验证范围与工作区状态。

用户明确要求发版即授权按这些门禁公开 Pre-release，无需再次确认。只读核验不运行发布入口。源码推送和工作区调整沿用会话已有授权；发现来源不明的修改时保留并停在具体差异处。

## 1. 选定源码与环境

Windows 编排优先使用 `pwsh.exe`（PowerShell 7）；从 WSL 调用也使用它。用 `Get-Command pwsh.exe` 或 `where.exe pwsh` 定位，必要时检查 Windows 的 `C:\Program Files\PowerShell\7\pwsh.exe`。报告缺失或具体兼容性错误后才回退 `powershell.exe`。原生 Windows Git、Python、gh 与 WSL 的登录态和路径分别检查。

在任何 Python、构建或下载命令前，将 TEMP、TMP 和相关缓存定向 E 盘。发布包装器会设置其子进程环境；额外启动器也先将 TEMP/TMP 设到 `E:\TBuild\fork-release\Temp`。依赖、构建目录、暂存和日志均留在 `E:\TBuild`。

确定完整提交 SHA；它应已包含在远端 `origin/dev`，不必强制等于每天可能变化的远端最新提交。核对主仓库与 `E:\TBuild\tdesktop` 各自的根目录、HEAD、origin、磁盘空间、原生 `gh auth status` 和既有发行序号。

用原生 Git检查 E 盘副本：

```powershell
$repo = 'E:\TBuild\tdesktop'
git -C $repo status --porcelain=v1 --untracked-files=all
git -C $repo submodule status --recursive
Get-CimInstance Win32_Process | Where-Object {
    $_.Name -eq 'AyuGram.exe' -and
    $_.ExecutablePath -eq "$repo\out\Debug\AyuGram.exe"
} | Select-Object ProcessId, ExecutablePath
```

每条原生命令后检查 `$LASTEXITCODE`，失败即停止依赖它的后续动作。源码与子模块均须干净，递归状态不得出现 `-`、`+`、`U`。程序仍运行时报告精确路径并等用户退出。

E 盘副本落后时逐项核对修改与已提交内容。对已确认的旧构建修复，保留包含未跟踪文件的 stash 或外部备份并记录位置，再快进到选定源码；未知内容保持原样。发布器本身不会代为提交、推送、清理或切换源码。恢复问题时加载本 skill 的 [recovery.md](references/recovery.md)。

## 2. 执行新发布

在原生 PowerShell 7 中设置为核验后的完整 SHA，再运行：

```powershell
pwsh.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File E:\TBuild\tdesktop\tools\release-windows.ps1 `
  -Repo E:\TBuild\tdesktop `
  -SourceSha $sourceSha
```

`$sourceSha` 必须是确定的 40 位提交值。发行号由脚本按源码基础版本选取下一序号；明确指定时传 `-Tag`，格式以当前脚本为准。工具版本仅在与实际配置树一致时传 `-QtVersion`、`-MsvcVersion`。

长任务必须放到独立进程里跑，避免工具超时中断发布：参数写进 E 盘启动器，记录 PID 和开始时间，用本次运行唯一的日志与退出码文件，成功和异常路径都写退出码。父进程用 `cmd.exe`，脚本本身仍由 `pwsh.exe -File` 执行（见下）。查看进程树和本次 `OutputRoot\logs`，每次等待不超过 60 秒并持续报告进度。

### 启动器：父进程用 cmd，不要从 WSL 起 Start-Process pwsh

从 WSL 经 `Start-Process pwsh.exe` 起的子进程只拿到极简 PATH（实测仅 `C:\Program Files\PowerShell\7` 与 WindowsApps 条目），`git` 直接不可用。手工烘焙完整 PATH、或改从注册表取 `Machine`+`User` PATH，都只让 `git` 与 `python` 找得到，python 里的 `git submodule status` 仍失败——git for Windows 的 shell 子命令拿不到自己的 `usr\bin` 与 `libexec/git-core`：

```text
git-submodule: line 7: basename: command not found
git-submodule: line 22: .: git-sh-setup: file not found
```

用 `cmd.exe` 作父进程时 PATH 是系统正常值，同一条命令一次通过（40 个递归子模块）。启动器 `E:\TBuild\run-fork-release.cmd`：

```bat
@echo off
set "TEMP=E:\TBuild\fork-release\Temp"
set "TMP=%TEMP%"
if not exist "E:\TBuild\fork-release\logs" mkdir "E:\TBuild\fork-release\logs"
del /q "E:\TBuild\fork-release\manual-exit.txt" 2>nul
"C:\Program Files\PowerShell\7\pwsh.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "E:\TBuild\tdesktop\tools\release-windows.ps1" -Repo "E:\TBuild\tdesktop" -SourceSha "<40 位提交 SHA>" >> "E:\TBuild\fork-release\logs\manual-release.log" 2>&1
echo %ERRORLEVEL%> "E:\TBuild\fork-release\manual-exit.txt"
```

从 WSL 启动：`Start-Process cmd.exe -ArgumentList "/d /c E:\TBuild\run-fork-release.cmd" -WindowStyle Hidden -PassThru`。也可让 Bash 直接起 `pwsh.exe -File ...`（不经 `Start-Process`），实测 preflight 能通过，但要用后台任务承接以免工具调用超时。

`01-python` 退出码 2、步骤日志只有一行 `release refused: command failed (1): git` 时，这是环境问题而不是源码问题：在同一父进程下逐条重跑 preflight 的 git 命令并打印 stderr；同时确认 E 盘副本的 `refs/remotes/origin/dev` 不是过期引用——preflight 会自己 fetch 并校验目标 SHA 是其祖先，提前手动 `git -C E:\TBuild\tdesktop fetch --quiet origin +refs/heads/dev:refs/remotes/origin/dev` 能省掉一次失败。

包装器顺序完成源码门禁、MSVC 初始化、配置、Debug 增量构建、实际 Qt 布局回归、构建证明、白名单打包和上传公开。成功必须有本次退出码 0、成功链接日志及匹配的可执行文件哈希；旧 EXE 或旧退出码不足为证。

沿用 `out`、Qt/MSVC 和发布配置。首次关闭更新器、公共头文件或语言/样式生成变化可能引发大范围重编译；日常少量 `.cpp` 修改通常只编译相关文件再链接，Debug 链接、压缩与上传仍需时间。按实际日志报告进度，不保证固定时长。

依赖或编译失败时加载 `../build-windows-ayugram/SKILL.md`，只修复证据指向的问题。应用代码修复必须先提交推送，再对齐确定源码重新发布；保持用户账号目录和正在测试的进程。不要切换 Release 配置或清空依赖来加速。

## 3. 恢复未公开草稿

先读取实际草稿 tag、源码、日志及 `build-proof.json`，确认无旧发布进程。使用同一 SHA 和 tag：

```powershell
pwsh.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File E:\TBuild\tdesktop\tools\release-windows.ps1 `
  -Repo E:\TBuild\tdesktop -SourceSha $sourceSha -Tag $tag -Resume
```

当前证明有效期为 24 小时；以脚本当前校验为准。恢复跳过构建，但仍校验源码、配置、产物哈希和草稿归属。只有同一源码与校验集合的草稿可继续；已公开版本不覆盖。证明失效或产物变化时按文档使用新序号重新构建，保留并报告旧草稿，不擅自删除远端资产。

## 4. 核验公开结果

使用原生 gh 查询实际发布版本：

```powershell
gh release view $tag --repo jinmo100/AyuGramDesktop --json isDraft,isPrerelease,url,assets
gh api "repos/jinmo100/AyuGramDesktop/git/ref/tags/$tag"
```

完成标准：`isDraft=false`、`isPrerelease=true`；tag 解析到本次完整源码 SHA（注解标签继续解析）；ZIP、`SHA256SUMS`、`build-info.json` 三项资产齐全。下载两个小元数据文件到 E 盘唯一核验目录，比较源码、版本、构建配置及 ZIP SHA256 与 GitHub 资产 digest；必要时下载 ZIP 重算哈希。

轻量标签的 `object.type` 是 `commit`，其 `object.sha` 直接就是源码提交；注解标签才需再取一层 `git/tags/<sha>` 的 `object.sha`。本次运行的证据同时看日志：`logs/*-03-cmake.log` 要有 `Telegram.vcxproj -> ...\out\Debug\AyuGram.exe`，`logs/*-07-layout_regression.log` 要有六条 `PASS`，`build-info.json` 的 `zip_sha256`、GitHub 资产 `digest` 与下载到的 `SHA256SUMS` 三者一致。

包内仅应有应用、许可证、说明和空 `TelegramForcePortable`；通过发布器白名单验证，不能复制整个 Debug 目录。公开包不含 tdata、账号、缓存、日志或 PDB。最后检查两个源码副本状态。

报告公开 Release 链接、直接 ZIP 链接、大小、源码 SHA、已通过的构建/布局检查及校验值；将新包的启动、登录和干净机器依赖验证与历史实测区分。手动下载更新，内置更新器保持关闭。草稿或资产上传成功均不代表公开发布完成。
