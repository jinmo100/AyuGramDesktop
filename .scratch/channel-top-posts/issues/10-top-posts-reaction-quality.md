# 10: 表情质量分档与两个新排序项（总数 / 优质 / 劣质）

Status: implemented（代码已改，运行未实测）


Blocked by: None

## Summary
保留"按总表情数"排序，新增"按优质表情""按劣质表情"两个排序项；两个分量在查询期从 `reactionsJson` 现算，分类名单可在设置里改。

## Acceptance Criteria
- [x] 不新增计数列：优质/劣质在查询期从 `reactionsJson` 现算（`CountReactionQuality`），存量缓存免重扫即可排序——比落列更省一次迁移，且改名单立即对全部缓存生效。
- [x] 三个口径同时成立：总数沿用 `reactionsCount` 语义不变；优质/劣质只按名单求和，未分类与 ⭐ 只进总数。
- [x] 三个排序项都能选，并列次键沿用 reactions→comments→views→date。
- [ ] 三个排序项在同一批真实帖子上给出不同的前 5 名。（待实测）
- [x] 设置里能改两份名单（AyuGram → 存储 → Reaction Quality），改动后重开面板即生效。
- [x] 存量缓存不重扫也能算出两个分量（查询期现算即为此处的"惰性补齐"）。

## Implementation Details
- 提取点 `top_posts_scanner.cpp:59-79`：每个 reaction 已有 `id.emoji()` / `id.custom()` / `id.paid()`，直接在循环里按名单分流。
- 聚合点 `top_posts_aggregator.cpp:44` 三个数一起加。
- 排序枚举 `ayu_database.h:60-64` 与 SQL 分支 `ayu_database.cpp:930-967` 各加两支。
- 固定栏第二行左半当前是 3 格（`top_posts_section.cpp:319-338`），5 项会挤，需重排或以合并按钮承载；以"任何宽度不裁字"验收。

## 二轮返工（2026-10-23 二次验收）

默认正名单补 🤣 😂 😁 🙂 👌；卡片按帖显示 `👍N` / `👎N`，让质量排序的依据可自证。负名单仍只收嘲讽/否定类。
