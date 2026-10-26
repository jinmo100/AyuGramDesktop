# Agent Note: 榜单扫描覆盖与自定义时间范围

Status: implemented

## Problem

热度榜单的「扩窗」和「自定义范围」共用一个坏掉的水位：`ChannelTopPostState::scanWindow` 既是用户选的目标条数，又是已扫条数，还是续扫判断依据。

1. 扩窗不幂等：`Telegram/SourceFiles/ayu/ui/top_posts/top_posts_section.cpp:780` 用 `target > state.scanWindow` 判定是否续扫，而扫描完成时 `Telegram/SourceFiles/ayu/features/top_posts/top_posts_scanner.cpp:452` 把 `scanWindow = 本次扫描条数` 写回。结果选过 1000 再选 5000 会从 0 重扫，选过 5000 再选 1000 反而把水位降到 1000，之后再扩窗又重扫一遍。用户看到的「缓存时灵时不灵、又从零开始加载」就是这个。
2. 整行覆盖写竞争：`saveViewState()`（section.cpp:1043 附近）和扫描器回写都用 `storage.replace(state)`。扫描进行中切换排序会读旧行、写回整行，把扫描侧刚写入的水位覆盖掉。
3. 没有自定义范围：扫描只能从最新往前数 N 条，5000 条对消息量大的群只覆盖一两个月，拿不到去年某几个月的高质量帖。

## Decision

- 覆盖改为多区间：新增 `ChannelTopPostScanRange`（`Telegram/SourceFiles/ayu/data/entities.h:216`，一比一对应仓库惯例的 schema 实体），一条记录代表一次扫描过的消息区间，带 `fromDate`/`toDate`/`fromId`/`toId`/`messagesScanned`。
- 计数的覆盖只增不减：`extendChannelTopPostCoverage`（`Telegram/SourceFiles/ayu/data/ayu_database.cpp:1292`）只在这次扫到的 `oldestScannedId` 更旧时才更新水位。表里 2000 行的 `scanWindow` 从此只表示「已覆盖的最近条数上限」，不再是目标值。
- 定向写入拆开争用：`saveChannelTopPostView`（ayu_database.cpp:1270）只写 `sortOrder`/`filter`，`extendChannelTopPostCoverage` 只写 `scanWindow`/`oldestScannedId`/`lastScanTime`，`touchChannelTopPostAccess` 只写 `lastAccessTime`。三条路径都不再整行 replace。
- 时间范围扫描是绝对区间：`CommandLine`/扫描器新增 `rangeFrom`/`rangeTo`，`MTPmessages_getHistory` 的 `offset_date` 用来直接跳到 `rangeTo`，之后按批次自然往前，批次里最旧消息早于 `rangeFrom` 就停（scanner.cpp 里 `reachedRangeStart`）。
- 范围扫描可续：先查同名 `[fromDate, toDate]` 的范围记录，`toId` 当续扫起点，`messagesScanned` 当已有预算，这次再追加 20000 条（`kMaxRangeScanMessages`，总上限 200000 = `kMaxRangeScanTotal`）。重复点同一区间是幂等的，不会从头再扫。
- 范围视图是叠加而非另开界面：`setDateRange()/clearDateRange()` 只改 `getChannelTopPosts` 的日期上下界，排序和内容筛选照常生效；顶部固定栏出现一枚可点击的「起 – 止 ✕」chip（`FixedBar::setDateRange`）。
- 显示上限维持 300（`AyuDatabase::kTopPostsDisplayLimit`），存储上限 3000（`kTopPostsStorageLimit`）。截断按互动量保留（ayu_database.cpp:824 的排序），早年的帖只要进了前 3000 就留得住。

## Consequences

- 新表靠 `storage.sync_schema(true)` 自动建，不需要手写迁移；老库的 `ChannelTopPostState` 不动，水位只增不减会自愈。
- 「Clear Channel Cache」顺带清空该频道的范围记录和日期视图，是唯一的「从头来」入口。
- 增量刷新（delta 同步）拿不到新消息条数，明确不计入覆盖；铺开大面积时间范围只能靠范围扫描一次一次铺。
- 扫描期间再点扩窗或范围只提示「正在扫描」，不排队。
- 范围扫描期间进度文案只有已扫条数，没有总数（区间里有多少消息事先不可知）。
- 决定与取舍的完整记录见 `docs/adr/0001-top-posts-coverage-and-scan-ranges.md`。

## Alternatives considered

- 留着 `scanWindow` 同时当目标值和已扫值，只在写入时做 max：否决。目标值和已完成值语义不同，混在一起导致「扩窗」和「重复扫描」永远分不清，加自定义范围后更乱。
- 给覆盖加一个 JSON 列存区间列表：否决。按区间查询和续扫要反序列化全表，且与仓库其它表的一比一实体惯例不一致。
- 范围扫描直接复用增量同步的 delta 通道：否决。delta 只能取比缓存新的消息，天生无法回看历史区间。
- 范围视图单独一个 section（进栈）：否决。会跟已有排序/筛选 tab 分叉成两套 UI，且用户明确要求榜单留在左侧、进出帖子不掉状态。
- 范围扫描不设总量上限，扫到区间尽头为止：否决。几十万条消息的区间会长时间占着网络和 flood 配额，200000 条足够覆盖任何一次「看去年几个月」的需求。
