# 发布恢复参考

只在发布门禁失败时读取对应分支。先保存证据和备份，再做精确修复；所有命令显式检查退出状态。

## E 盘源码对齐

首次发布曾遇到 E 盘 HEAD 落后、修复文件已复制但未提交的情况。逐项比较文件与已推送提交，可对照历史已提交版本；不能把所有差异都推定为旧修复。确认归属后保存 `git stash push --include-untracked`，核验 stash 创建成功并记录对象，再执行快进。备份保留至成功验证，不自动恢复覆盖新源码。

原生 Windows 副本使用原生 Git；WSL 源码编辑保持 LF、无 BOM。避免两个 Git 进程或不同平台工具同时操作一个 index。执行 fetch/merge/stash 前分别验证上一条操作成功。

## 递归子模块不齐

读取 `git submodule status --recursive` 的完整结果。`-` 表示未初始化，`+` 表示 HEAD 不匹配，`U` 表示冲突。对未初始化的精确路径从其直接父仓库执行 `git submodule update --init --recursive --depth=1 -- <path>`，然后再次检查全部递归状态。

首次恢复中出现过 libcbor 的 doxygen-theme、range-v3 的 doc/gh-pages、cmake 的 cppgir，以及 cppgir 内的 expected-lite。它们是历史例子，不是固定修复清单；不要只查一层便宣布完成。已经填充或修改的子模块先查归属，不能用 reset 清掉。

## Windows 无法读取 WSL 符号链接

`Function not implemented` 或 `cannot hash` 可能来自 WSL 创建的链接与 Windows Git `core.symlinks=false` 不兼容。检查精确子模块的索引模式 `120000`、HEAD 链接目标、当前文件种类和链接内容，区分真实修改。

只有确认每个待转换链接与 HEAD 完全相同，并已在源码外保留可恢复链接的归档后，才将这些精确链接移除，再让原生 Windows Git 按索引恢复为链接目标文本。恢复后核验字节和原生 Git 干净状态；保留备份路径。首次涉及 fcitx5-qt、hime、hunspell、nimf，不应按这份名称列表直接批量重置。正常 Windows checkout 无需做此转换。

## 残留锁

通过 `git rev-parse --git-path <name>` 定位真实锁路径，兼容 worktree；发布锁为 `fork-release-wrapper.lock`、`fork-release-publish.lock`。记录本次启动器与其子进程，检查相关 Python/Git/MSBuild/发布进程是否仍存活。发布锁中的 PID 仅是线索，需核对进程身份和启动时间。

只有确认没有对应活进程后，才精确移除陈旧锁，再顺序重试。Git 的 `index.lock` 也按相同原则检查，必要时先移到备份名。PID 不明或可能仍运行时保持锁并查证；不要全局停止 Python/Git 或批量删除 `.git` 锁文件。

## 编译与工具初始化失败

先读本次唯一日志。加载 `../../build-windows-ayugram/SKILL.md` 处理依赖/编译问题。既有发布包装器直接导入 `Microsoft.VisualStudio.DevShell.dll` 并调用 `Enter-VsDevShell`；不要假定 `Launch-VsDevShell.ps1` 接受相同参数。核对配置树里的完整 MSVC 版本与实际环境。

保留现有 Qt 和 Debug 构建结果，修复具体失败再继续。PowerShell 7 的实际兼容性以本次运行证据为准，不能用此前 5.1 成功代替；需要回退时记录具体错误和原因。单纯提炼 skill 不执行构建、发版或修改远端。

## 上传失败

先查远端：若已公开，转只读核验；若是脚本拥有的同一草稿，按主 skill 的 `-Resume` 路径继续。出现草稿内容、源码、资产 digest 不符时保持远端原样并报告。缺凭据时先检查原生 gh 登录和仓库权限，避免打印 token、PEM、CMake 中的 API 凭据或账号数据。
