# 09: 扫描抓全量窗口，筛选只在查询层

Status: implemented（代码已改，运行未实测）


Blocked by: 08

## Summary
扫描不再按筛选口径定向抓取；Documents/Media 筛选只作用于本地查询，切换排序/筛选零网络。

## Acceptance Criteria
- [x] `Scanner` 与 `IncrementalSyncController` 都不再接收筛选参数，抓取窗口与筛选无关。
- [x] 切换排序或筛选不产生网络请求：`loadPosts()` 只查库，`getChannelTopPosts` 在查询层做 mediaType 过滤。
- [x] 首扫用全量口径完成后切"仅文档"，榜单立刻有内容（同一份缓存按查询过滤）。
- [x] 一次性作废旧口径缓存：`AyuMigrations::migrateToV2` 清空 `ChannelTopPost` 与状态表，`kLatestVersion = 2`。

## Implementation Details
- 构造点 `top_posts_section.cpp:590`、`:661` 传入的 `_filter` 去掉；`getChannelTopPosts`（`ayu_database.cpp:912-975`）的 mediaType 过滤保持现状，它本来就是查询层。
- `top_posts_sync.cpp:58` 的排序沿用例保持。
