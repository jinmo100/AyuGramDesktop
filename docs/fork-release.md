# AyuGram-Jinmo 同步与测试版发布

本 fork 每天跟踪 `AyuGram/AyuGramDesktop:dev`，通过轻量门禁后合并到自己的 `dev`。Windows x64 测试版由维护者在 E 盘构建，并通过 GitHub Releases 手动下载更新。

## 一次性设置上游同步

工作流必须先进入此 fork 的默认分支 `dev`。运行配置向导：

```bash
bash tools/setup-fork-sync.sh
```

在当前 WSL 环境中，若没有 Linux `gh`，向导会复用 `E:\Scoop` 已安装的原生 Windows GitHub CLI 及其登录态。

向导分四步：登录 GitHub CLI；创建 GitHub App；只安装到 `jinmo100/AyuGramDesktop` 并生成私钥；将 App ID 和私钥分别保存为仓库变量 `SYNC_APP_ID` 与 Secret `SYNC_APP_PRIVATE_KEY`。私钥内容不会写入本地配置文件；向导只在 Git 元数据目录保存非秘密 App ID。不要把 PEM 放进源码或发布包。

App 需要此仓库的 Contents、Pull requests 和 Workflows 写权限。若分支规则禁止直接快进写入，需允许此 App 执行对应操作；脚本不会修改分支保护。镜像分支 `upstream-sync` 由同步机器人独占写入，人工修改前先暂停此工作流。`dev` 仍可正常人工开发。

随后在 Actions → **Sync AyuGram upstream** 手动运行一次。此后每天 UTC 03:23（北京时间 11:23）检查。GitHub 定时任务可能延迟，公开仓库长期无活动可能停用，需要重新启用。订阅该工作流和同步 PR 的通知。

同步会复用一个 PR，在独立的无写入凭据任务中检查确切候选合并。检查包括笔记、工作流语法与四组独立逻辑测试；这些测试不保证 Windows 编译和真实 UI 行为。若候选修改工作流、门禁脚本、测试或工具依赖，则停住等待人工审查，防止候选自己把检查改成无条件成功。上游历史改写、冲突、验证失败或分支移动也会停止。

最终提交是带两个父提交的普通 merge。脚本推送已经验证的确切合并提交，使 GitHub 自动识别同步 PR 已合并；不会 squash、rebase 或改写 `dev`。`dev` 的精确 lease 防止覆盖并行开发；镜像分支附带同一原子推送中的 lease，并由工作流并发组串行管理。不要让其他机器人同时更新镜像。

## 本机发布

Agent 执行新发布、草稿恢复或资产核验时，加载项目 skill [release-windows-ayugram](../.agents/skills/release-windows-ayugram/SKILL.md)。Windows 编排优先 PowerShell 7（`pwsh.exe`）；实际发布门禁由现有脚本执行。
需要在原生 Windows 环境安装 Git、GitHub CLI、Python 3.10+，并已有 E 盘 MSVC、Qt 和配置好的构建目录。先用原生 Windows 的 `gh auth login` 登录；WSL 的登录与 Windows 的登录相互独立。GitHub CLI 的凭据保存在其配置/系统凭据存储，不在源码内。

把发布源码推送到本 fork 的 `dev`，在 `E:\TBuild\tdesktop` 检出要发布的提交并匹配子模块。源码和子模块必须干净。Windows 源码副本中的临时修改也需要先审查处理；发布命令不会提交、丢弃、重置或自动切换你的工作区。

从原生 PowerShell 执行：

```powershell
pwsh.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File E:\TBuild\tdesktop\tools\release-windows.ps1 `
  -Repo E:\TBuild\tdesktop `
  -SourceSha <完整提交SHA>
```

默认使用 `E:\TBuild\fork-release` 暂存、缓存与验证输出；Qt 默认 `5.15.19`，MSVC 默认匹配 `14.44` 系列并验证配置树中的确切工具版本。可通过 `-QtVersion`、`-MsvcVersion` 指定实际配置。所有构建和发布输出留在 E 盘。命令遇到正在运行的该路径 `AyuGram.exe` 会停止，先自行退出再重试。

命令按顺序检查源码、初始化 MSVC、重新配置禁用更新器和 fork 下载入口、执行 Debug 增量构建、运行实际布局回归、校验构建证明、打包、上传及公开 Pre-release。**它会在全部门禁通过后直接公开测试版**，不再等待你点击发布。完整应用实测仍由维护者自行安排，发布说明不会声称这些场景已自动验证。

每个原生命令的日志保存在 `OutputRoot\logs`，错误会给出日志路径；API 配置行与 API hash 在写入前脱敏。发布包不收集这些日志。

版本自动选取下一个可用序号，例如 `v7.2.9-jinmo.1`。也可传 `-Tag v7.2.9-jinmo.1` 明确指定，序号必须为正且基础版本匹配源码。公开版本、已有冲突 tag 和身份不符的草稿均拒绝覆盖。

## 上传失败与重试

上传使用草稿暂存，所有资产 SHA-256 与远端一致后才公开。失败时记录草稿版本，使用原版本重试：

```powershell
pwsh.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File E:\TBuild\tdesktop\tools\release-windows.ps1 `
  -Repo E:\TBuild\tdesktop -SourceSha <同一完整提交SHA> `
  -Tag v7.2.9-jinmo.1 -Resume
```

`-Resume` 仅跳过构建，不跳过源码、配置、产物哈希和构建证明检查。证明有效期 24 小时，要求与当前 MSVC、Qt 和 E 盘环境一致。相同源码、产物与配置生成确定的包内容，已上传且哈希相符的资产可以复用。不匹配的草稿、额外资产或 API 错误均停止，不自动删除远端资产。超时或构建产物变动后，应使用新的发行序号重新构建发布，并人工处理旧草稿。

进程异常中断可能留下 Git 元数据目录中的 `fork-release-wrapper.lock` 或 `fork-release-publish.lock`。确认没有发布命令正在运行后，只删除对应的锁文件；不要删除构建或便携账号目录。

## 发布包与验证范围

公开资产为便携 ZIP、`SHA256SUMS` 和 `build-info.json`。ZIP 只包含 `AyuGram.exe`、许可证、说明，以及带 `.keep` 的 `TelegramForcePortable`（`.keep` 让空目录在任何解压器下都保留，Scoop 的便携数据持久化依赖该目录存在）。不包含账号 `tdata`、缓存、日志或 PDB。对应源码链接、SHA、Debug 配置、验证范围和工具版本写入发布信息。Debug 的大小与启动依赖仍需在目标机器验证；已检查当前配置使用静态 Debug CRT，未用干净 Windows 机器证明全部第三方运行依赖。

程序内保留 AyuGram 名称和图标。此发布配置禁用内置更新器，关于页的版本链接与“下载测试版”入口指向本 fork 的 Releases。每次升级前退出程序，保留自己的便携数据，仅替换程序文件。

## 开发验证

```bash
npm ci --ignore-scripts
AGENT_NOTE_ARCHIVE_BASE_REF=HEAD npm run verify-notes
python3 -m unittest discover -s tools -p 'test_fork_*.py'
# actionlint 1.7.7、g++ C++20 和 Node 22.19+ 已在 PATH 时：
AGENT_NOTE_ARCHIVE_BASE_REF=HEAD bash tools/check-fork-sync.sh
```

自动化测试使用临时真实 Git 仓库和模拟 GitHub 边界，不会发布真实 Release。仓库的既有 Canary/签名/Telegram 发布链保持独立。
