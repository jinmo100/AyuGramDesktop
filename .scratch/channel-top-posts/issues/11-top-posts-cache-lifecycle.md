# 11: 缓存生命周期修复（TTL 与 LRU）

Status: implemented（代码已改，运行未实测）


Blocked by: None

## Summary
修掉"缓存一两天就没了"：TTL 要么真生效要么删掉，LRU 改用真正的访问时间并把频道上限从 30 提到 100。

## Acceptance Criteria
- [x] `pruneExpiredChannelPosts` 有明确调用点（`initialize()` 锁外 `ayu_database.cpp` 原有调用），不是死代码。
- [x] 打开面板即更新"最近访问"：`touchChannelTopPostAccess` 写 `lastAccessTime`，`evictLruChannelsLocked`/`pruneExpiredChannelPosts` 都以 `max(lastAccessTime, maxScanTime)` 为准。
- [ ] 扫过 40 个频道后最常打开的那个仍在缓存。（待实测）
- [x] 频道上限 30→100，`evictLruChannels` 默认值单点定义，加锁包装与内部实现口径一致。
- [x] 每频道 300 条截断保持现状。

## Implementation Details
- `ayu_database.cpp:664-708` `evictLruChannelsLocked(30)` 按 `max(scanTime)` 排序 → 改按访问时间列；`:826` `evictLruChannels` 同步。
- 访问时间的写入点：`top_posts_section.cpp` 打开面板/`loadPosts()` 时。
- 需要新增一列并迁移老库（默认值取 `max(scanTime)`）。
