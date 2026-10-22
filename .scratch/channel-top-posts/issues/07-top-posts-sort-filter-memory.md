# 07: 排序与筛选按频道记忆

Status: implemented（代码已改，运行未实测）


Blocked by: None

## Summary
每个频道各记一份"排序 + 筛选"，退出再进入时恢复；新频道用全局默认（Reactions / All / 深度 1000）。

## Acceptance Criteria
- [ ] 频道 A 选"按评论 + 仅文档"后退出再进，仍是"按评论 + 仅文档"。（代码侧：`ChannelTopPostState.sortOrder/filter` 落盘并在 ctor 读回；待实测）
- [ ] 频道 B 首次进入是默认 Reactions/All，不受 A 影响。（代码侧：按 channelId 分行，缺行即默认；待实测）
- [x] 频道没有讨论组时"按评论"回落到 Reactions 并同步落盘（ctor 里回落后再 `saveViewState()`）。
- [x] 记忆跟着频道走：`clearChannelPosts` 只清水位不动 sort/filter，`clearAllChannelPosts` 同样保留排序记忆。

## Implementation Details
- `AyuSettings`（`Telegram/SourceFiles/ayu/ayu_settings.h`，nlohmann json + `rpl::variable`）目前没有 TopPosts 段；持久化以 channelId 为键的 map。
- 读取点在 `top_posts_section.cpp:490-491`（memento 恢复），写入点在 `:397-410`（`sortChanged`/`filterChanged`）与退出时。
- `top_posts_section.cpp:178-200` 的自动回落逻辑（无讨论组回落到 Reactions、有讨论组回落或切到 Comments）也要同步写入记忆。
