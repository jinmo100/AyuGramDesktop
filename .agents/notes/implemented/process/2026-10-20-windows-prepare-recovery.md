# Agent Note: Windows 依赖准备恢复与下载复用

Status: implemented

## Problem

E 盘原生 Windows Debug 构建在网络下载、Qt 重建、MSYS2 包选择及 MSVC 兼容处遇到中断。此前为完成构建保留的 `prepare.py` 改动需要纳入版本管理，避免后续发布的源码干净度门禁长期被阻塞。

## Decision

依赖准备缓存已下载的 MSYS2 安装包，优先使用清华 MSYS2 镜像，选用 MSYS 的 diffutils。固定版本的 OpenSSL、FFmpeg 和 ada 使用浅克隆，减少首次准备成本。Windows 的 libjxl 编译器条件补丁覆盖 `_MSC_VER`。

Windows Qt 5 准备保留已有源码目录并浅更新所需子模块，重新清理 Qt 自己的 qtbase 工作树后应用补丁。清理仅作用于这一依赖工作树，重新准备前不应在其中保留人工补丁或独立工作。补丁路径带引号，允许空白差异但任何应用失败立即中止。Qt 6 等仍采用原有清空重建路径，不能因全局保留目录而使无条件 clone 失败。Qt 5 显式指定 nmake 与 Debug OpenSSL 链接库，避免错误的 make 工具或库路径。

过期缓存自动重建路径使用赋值设置 `Rebuild`，修复原比较表达式未改变状态的问题。此提交属于构建流程修复，不改变应用功能。

## Alternatives considered

每次强制清理所有依赖的优点是源码状态简单；已有 Qt 构建代价较高且存在网络中断，选择仅对具备复用命令的 Windows Qt 5 保留源码。其他依赖不扩大复用范围。

忽略 Qt 补丁失败的优点是恢复操作步骤更少；它会继续构建部分打补丁的依赖，因此保留失败即停的原有约束。

## Consequences

这些变更来自已完成的原生 Windows 构建恢复，历史构建证据见[榜单笔记](../feature/2026-10-19-channel-top-posts.md)。本次提交只做 Python 语法、复用范围与补丁错误路径检查，不重新下载、清理或构建依赖。镜像可用性、浅克隆分支和后续 Qt 重建仍受外部环境影响。
