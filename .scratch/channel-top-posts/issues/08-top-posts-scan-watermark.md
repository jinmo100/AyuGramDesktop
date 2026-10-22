# 08: 扫描水位与增量续扫（选 2000 只补 1000→2000 的缺口）

Status: implemented（代码已改，运行未实测）


Blocked by: None

## Summary
为每个频道记录"已经扫到哪"，重选更大的深度时只补缺口，不再从最新消息重扫一遍。

## Acceptance Criteria
- [x] 新增频道级扫描状态 `ChannelTopPostState`（oldestScannedId / lastScanTime / scanWindow / sortOrder+filter），随频道一起被淘汰或按 TTL 清理。
- [ ] 已扫 1000 条的频道改选 2000：只发约 10 批新请求，进度从 1000 起算。（代码侧：`startScan()` 读水位传 `resumeOffset`/`alreadyScanned`；待实测）
- [x] 改选更小的深度（2000→1000）不删已缓存数据（`scanWindow` 只取 max，只是不再往前）。
- [x] 扫描中途取消保留水位：`finish(Cancelled)` 与 Completed 走同一段写水位逻辑。
- [x] 没有水位数据的老库按"无水位"处理（`getChannelTopPostState` 缺行返回默认值）。

## Implementation Details
- `Telegram/SourceFiles/ayu/features/top_posts/top_posts_scanner.cpp:149` 的 `_offsetId = 0` 改成从水位恢复；`:348-360` 已有 `oldestId` 推进逻辑，把它接到水位上。
- 水位落盘位置二选一：`ChannelTopPost` 旁边新建一张频道状态表，或在 `AyuSettings` 存。优先前者（跟着 `clearChannelPosts`/`evictLruChannels` 一起走）。
- `truncateChannelPosts(channelId, 300)` 只截榜单行，不能截水位，否则续扫会重头再来。
