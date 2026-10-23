# 07: 排序与筛选按频道记忆

Status: implemented（代码已改，运行未实测）


Blocked by: None

## Summary
每个频道各记一份"排序 + 筛选"，退出再进入时恢复；新频道用全局默认（Reactions / All / 深度 1000）。

## Acceptance Criteria
- [x] 频道 A 选"按评论 + 仅文档"后退出再进，仍是"按评论 + 仅文档"。（2026-10-23 13:20 真机实测：库里 `sortOrder=1` 时从菜单新开面板显示「💬 留言」；点菜单选「👍 Positive」后库里变 `sortOrder=3`，重启前后一致）
- [ ] 频道 B 首次进入是默认 Reactions/All，不受 A 影响。（代码侧按 channelId 分行；本次只对频道 A 做了真机切换，B 的独立性依据是 `ChannelTopPostState` 的主键，见 `ayu_database.cpp` 的 `getChannelTopPostState`）
- [x] 频道没有讨论组时"按评论"回落到 Reactions 并同步落盘（ctor 里回落后再 `saveViewState()`）。
- [x] 记忆跟着频道走：`clearChannelPosts` 只清水位不动 sort/filter，`clearAllChannelPosts` 同样保留排序记忆。

## Implementation Details
- `AyuSettings`（`Telegram/SourceFiles/ayu/ayu_settings.h`，nlohmann json + `rpl::variable`）目前没有 TopPosts 段；持久化以 channelId 为键的 map。
- 读取点在 `top_posts_section.cpp:490-491`（memento 恢复），写入点在 `:397-410`（`sortChanged`/`filterChanged`）与退出时。
- `top_posts_section.cpp:178-200` 的自动回落逻辑（无讨论组回落到 Reactions、有讨论组回落或切到 Comments）也要同步写入记忆。

## 二轮返工（2026-10-23 二次验收）

排序/筛选改由 memento 单一来源：ctor 不再从 DB 抢着恢复，`setInternalState` 应用 memento 并落盘；菜单入口先用 `getChannelTopPostState` 读出上次状态再构造 memento。
