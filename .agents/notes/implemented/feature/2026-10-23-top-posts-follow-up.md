# Agent Note: 频道榜单二轮改造（留驻、记忆、续扫、表情质量、缓存生命周期）

Status: implemented

## Problem

首版榜单（见 `implemented/feature/2026-10-19-channel-top-posts.md`）上线后，实测暴露五类问题，逐条核对代码后的结论：

1. **榜单被顶掉**：卡片点击走 `controller()->showPeerHistory(_peer, Window::SectionShow::Way::Forward, msgId)`（`top_posts_section.cpp:424-427`），Forward 会把榜单 section 从栈里推走；想连续看第 2 条必须重新点菜单。自带的搜索结果列表是常驻的，可连续点第 1..5 条，这是体验落差。
2. **排序与筛选不记忆**：`SectionMemento` 保存了 sort/filter（`top_posts_section.cpp:480`、恢复在 `:490-491`），但菜单入口每次只构造带 peer 的 memento（`context_menu.cpp:441-446`），所以每次进面板都回到默认 Reactions/All；全仓没有任何 Settings 持久化。
3. **重扫从零开始**：`Scanner::start()` 每次把 `_offsetId = 0`（`top_posts_scanner.cpp:149`），表里也没有"已扫到哪"的水位列（`entities.h:147-166` 的 `ChannelTopPost` 只有逐帖字段）。选 1000 后再选 2000，等于把前 1000 条重抓一遍，既慢又重复消耗服务端配额。
4. **表情不分优劣**：`ExtractPostMetrics` 把每个 reaction 的 `count` 等权累加进 `reactionsCount`（`top_posts_scanner.cpp:59-79`），媒体组聚合也等权相加（`top_posts_aggregator.cpp:44`）；`CONTEXT.md` 还把这种口径定成了领域术语 `Reaction Engagement`。结果是🤡、👎一类表情堆出的恶意资源可以和高赞干货并列甚至压在上面。
5. **文案写死英文**：`ayu_TopPosts*` 键全英文且七个语言包都没有 `ayu_` 键（`lang.strings:9195-9215`），面板内部的 `[Photo]/[Video]/[Document]/[Album / Media Set]/[Message]`（`top_posts_inner.cpp:76-89`）与日期格式 `"MMM d, yyyy"`（`:270`）更是硬编码。

顺带核出的两个缓存事实（解释"隔一两天就失效"）：

- ~~`pruneExpiredChannelPosts`（`ayu_database.cpp:814-825`，默认 30 天）全仓零调用点~~ —— **该判断有误，实现时更正**：`initialize()` 末尾（锁作用域之外）本来就有 `pruneExpiredChannelPosts(); evictLruChannels();`。它真正的缺陷是判据用 `max(scanTime)`，和 LRU 的 30 频道上限叠加后，用户看到的还是"隔天失效"。
- 真正删数据的是 `evictLruChannelsLocked(30)`（`ayu_database.cpp:664-708`）：每次 `saveChannelPosts` 末尾执行，按 `max(scanTime)` 当作"最近访问"排序，只留 30 个频道。打开面板读缓存不写库，不算访问——所以扫过的频道一多，老频道的榜单会整批消失，不管它多常被查看。

## Decision

一轮改完，七个改动点，各自可独立验收：

1. **留驻**：卡片点击仍是 Forward，但榜单 section 在导航后保留在栈里；返回时保持排序、筛选与滚动位置。把面板做成聊天区浮层（真正像搜索那样并排常驻）成本高，不在本轮。
2. **记忆**：按频道持久化排序与筛选（`AyuSettings`），新频道用全局默认（排序 Reactions、筛选 All、扫描深度 1000）。
3. **续扫水位**：`ChannelTopPost` 之外新增频道级扫描状态（最旧已扫 messageId、最后扫描时间、上次扫描深度、上次扫描口径）。重选 2000 只补 1000→2000 的缺口；水位以"最旧已扫 messageId"为界。
4. **扫描与筛选解耦**：扫描永远按全量窗口抓取，筛选只作用于查询层；切换排序/筛选零网络请求。定向扫描（只抓 Documents/Media）取消。
5. **表情质量分档**：`reactionsCount` 保持现有语义（全表情等权求和）并保留"按总数"排序；新增两个排序项"按优质表情""按劣质表情"。~~每帖额外记两个计数（优质、劣质）。~~ 实现时改成**查询期现算**：按可配置名单把 `reactionsJson` 求和，不新增计数列——名单是用户可以改的，落列会随名单变更失真。分类用可配置名单（设置项），未分类的表情（自定义 emoji、付费 ⭐）只进总数，不进两个分量。媒体组的分量随总数一起相加。本条取代 `implemented/feature/2026-10-19-channel-top-posts.md` 里"所有表情不论正负均具有热度参考价值，不作倾向性权重过滤"的结论——总数语义与排序保留，但不再是唯一口径。
6. **缓存生命周期**：删掉死掉的 TTL 调用面或改成"水位 30 天过期"；LRU 改用真正的访问时间（打开面板即更新），频道上限 30→100。
7. **术语与语言**：`CONTEXT.md` 把 `Reaction Engagement` 拆成"总数 + 两个分量"三个词；面板内部硬编码文案进 `lang.strings`，日期按 locale 格式化。447 个 `ayu_` 键的中文包不在本轮。

## Consequences

行为后果：榜单不再被点卡顶掉，返回即回原位；每个频道各自记住排序与筛选；扫描深度只补缺口，不重发已扫过的窗口；切排序 / 切筛选零网络；表情质量成为独立维度而不是替换总数；缓存按"真正被打开过"的时间淘汰，上限放宽到 100 个频道。

代价与遗留：多了一张 `ChannelTopPostState` 表；按质量排序要在内存里解析该频道的 `reactionsJson`（最多 300 行）；排序多了一次下拉点击；七个语言包的 `ayu_` 键翻译仍未补齐。

### 验收标准

下面的复选框按代码路径勾选；运行时行为见下文 Verification status。

- [ ] 榜单里连续点开第 1、2、3 张卡后返回，仍停在原排序、原筛选、原滚动位置，无需重新点菜单。（待实测）
- [x] 在频道 A 选"按评论 + 仅文档"后退出再进入，仍是"按评论 + 仅文档"；频道 B 不受影响。
- [x] 已扫 1000 条的频道改选 2000，只发约 10 批新请求，不重扫前 1000 条；扫描进度条从 1000 起算。
- [x] 切换排序或筛选不产生任何网络请求；Documents/Media 筛选下榜单来自同一份全量缓存。
- [ ] 三个排序项都在：总数、优质、劣质；三者在同一批帖上给出不同的前 5 名（用真实频道验证）。（待实测）
- [x] 未分类表情与付费 ⭐ 只影响"总数"排序，不影响优质/劣质排序。
- [ ] 打开面板会刷新该频道"最近访问"时间；扫过 40 个频道后，最常打开的那个仍在缓存里。（待实测）
- [x] 面板标签优先复用核心 `lng_*` 键（Reactions/Comments/Views/All/Files/Media），随应用语言变化；AyuGram 专有词（Top Posts、Positive、Negative、菜单项、横幅）仍是 `ayu_` 英文——本仓库没有 `ayu_` 键的译文通道（见下）。

## Implementation

### 1. 留驻（票 06）

- **一轮的判断是错的**：`Way::Forward` 并不足以让榜单留在栈里。`MainWidget::showHistory`（`Telegram/SourceFiles/mainwidget.cpp:1507-1546`）对 Forward 做"同 peer 去重"：扫栈发现该频道的 `ChatMemento` 已存在就 `pop_back` 到该位置并把 way 改成 Backward，榜单因此被销毁、栈被清空、聊天顶栏也就不出现返回键。
- 二轮修法照抄上游同类跳转（`Telegram/SourceFiles/history/history_item_helpers.cpp:876-884` 的"跳到某条消息并保留来源"）：点卡时构造 `params.allowDuplicateInStack = true`（字段定义在 `Telegram/SourceFiles/window/window_session_controller.h:218`）并设 `params.origin = OriginMessage{ FullMsgId(_peer->id, msgId) }`，再 `showPeerHistory`。这样榜单 memento 会真的压进栈，聊天顶栏出现返回键，返回走 `showBackFromStack()` 回原位。
- 回到榜单时 `SectionWidget::showInternal` / `setInternalState` 用 memento 恢复排序、筛选、滚动位置，并触发一次纯本地的 `loadPosts()`（读 SQLite，无网络、不重扫）。
- 没有另造导航机制：`Window::SectionShow::Way` 只有 Forward/Backward/ClearStack，section 栈本身就是"返回原位"的机制。

### 2. 排序/筛选记忆（票 07）

- 新表 `ChannelTopPostState`（`Telegram/SourceFiles/ayu/data/entities.h` 定义，建表在 `ayu_database.cpp` 的 `make_storage`）：`channelId` 主键 + `sortOrder` / `filter` / `scanWindow` / `oldestScannedId` / `lastScanTime` / `lastAccessTime`。
- 面板构造与 `setInternalState` 都先 `AyuDatabase::getChannelTopPostState` 读回并 `_fixedBar->setState(...)`；改排序 / 改筛选时 `SectionWidget::saveViewState()` 落盘。
- 新频道在构造里写一次默认值（Reactions / All）；"无讨论组却存着 Comments"的频道回落到 Reactions 后再落盘。

### 3. 续扫水位（票 08）

- `Scanner` 构造多收 `MsgId resumeOffset` 与 `int alreadyScanned`；`start()` 里 `_scannedCount = _alreadyScanned; _offsetId = _resumeOffset;`。
- `SectionWidget::startScan` 读 state：`resuming = (oldestScannedId > 0 && target > state.scanWindow)`，命中即传水位与已扫条数，进度条从已扫条数起算。
- `Scanner::finish()` 的 Completed / Cancelled 两支都写回 `state.scanWindow = depth`（**实际**扫到的条数，不是目标深度）、`oldestScannedId = _offsetId.bare`、`lastScanTime = now`；取消后重选更大窗口也从真实缺口接上。
- `truncateChannelPosts(channelId, 300)` 只截榜单行，不动 state 行。
- 扫过但一条都没命中的频道不再每次打开都自动重扫（`loadPosts()` 多了 `oldestScannedId > 0` 的判断）。

### 4. 扫描与筛选解耦（票 09）

- `Scanner` 去掉 filter 成员，`requestNextBatch()` 只发 `MTPmessages_GetHistory`，不再走 `MTPmessages_Search`；`IncrementalSyncController` 同样去掉 filter。
- 筛选只在 `AyuDatabase::getChannelTopPosts` 的 where 子句生效，切排序 / 切筛选零网络。
- 一次性作废旧口径缓存：`AyuMigrations::migrateToV2` 清空 `ChannelTopPost` 与 `ChannelTopPostState`，`kLatestVersion = 2`。v1 的定向扫描只存了当时筛选那一类，留着会让"切到仅文档立刻有内容"看起来像坏数据。

### 5. 表情质量（票 10）

- `TopPostsSortOrder` 加 `Positive`、`Negative`；固定栏原来的三个排序按钮换成一个下拉按钮 + 5 个菜单项（🔥 Reactions / 👍 Positive / 👎 Negative / 💬 Comments / 👁 Views），无讨论组时 Comments 项禁用。
- 打分函数 `CountReactionQuality(const std::string &json, const std::vector<QString> &keys)`（`ayu_database.cpp` 匿名命名空间）：解析 `reactionsJson` 按名单求和，坏 JSON 返回 0。
- `getChannelTopPosts` 的 Positive / Negative 分支在内存里做：全取该频道行 → 逐条打分 → 按 (质量, reactionsCount, commentsCount, viewsCount, date) 降序 → 再切 `[offset, offset + limit)`；Reactions / Comments / Views 仍走原来的 SQL 排序。
- 名单存在 `AyuSettings::topPostsSettings()`（`positiveReactions` / `negativeReactions`，空白分隔 emoji；默认正 ❤ 👍 🔥 🎉 👏 💯 😍 🥰 🤩 ✨ ✅ 🙏、负 👎 🤡 💩 🤮 🖕 😡 🤬 🥱 🙄）；`Telegram/SourceFiles/ayu/ui/settings/settings_storage.cpp` 新增 "Reaction Quality" 分区，两项都是可编辑的 `EditMarkBox`（带"恢复默认"）。
- 未分类表情（自定义 emoji、付费 ⭐）只进总数。

### 6. 缓存生命周期（票 11）

- `pruneExpiredChannelPosts` 的判据从 `max(scanTime)` 改成 `max(lastAccessTime, scanTime)`（无 state 行时退回 scanTime），过期时同批删 posts 行与 state 行。
- `evictLruChannelsLocked` 上限 30 → 100，排序同样用 `max(lastAccessTime, maxScanTime)`；`touchChannelTopPostAccess(channelId)` 在构造与 `setInternalState` 时调用，只对真正有缓存行的频道建 state 行，避免空频道挤掉缓存频道。
- 淘汰频道时连同 `ChannelTopPostState` 一起删——否则"水位还在、数据没了"会让该频道永不自动重扫。
- `clearChannelPosts` 与 `clearAllChannelPosts` 都保留排序 / 筛选记忆，只把 `scanWindow` / `oldestScannedId` / `lastScanTime` 归零——清缓存不该顺手把用户偏好也清掉。

### 7. 语言（票 12）

- `top_posts_inner.cpp` 的 `[Photo]` / `[Video]` / `[Document]` / `[Album / Media Set]` / `[Album / Media Set ({count})]` / `[Message]` 改为 `ayu_TopPostsMedia*` 键；日期从 `"MMM d, yyyy"` 改为 `QLocale().toString(dt, u"d MMM yyyy"_q)`。
- 新增键：`ayu_TopPostsSortPositive` / `ayu_TopPostsSortNegative`、`ayu_TopPostsMedia{Photo,Video,Document,Album,AlbumCount,Message}`、`ayu_TopPostsReactionsTitle` / `ayu_TopPostsReactionsAbout`、`ayu_TopPostsPositiveReactions` / `ayu_TopPostsNegativeReactions`。
- 七个语言包里的 447 个 `ayu_` 键翻译仍在范围外。

## Verification status

上面每一条都是**代码路径已实现，但没有构建、没有跑起来**：仓库 AGENTS.md 明确"不要构建"，本轮也没有真机界面。

所以 Acceptance criteria 里的运行时断言（真实频道上三个排序给出不同前 5 名、40 个频道后常开的仍在缓存、连续点 1/2/3 张卡回到原位置）都还**未实测**，构建后要按 `.scratch/channel-top-posts/test_layout_regression.py` 再补一次面板实测。落盘格式部分只经过 `sync_schema(true)` 自动建表路径推断，同样未经运行验证。

## Risks

- **落盘格式变更**：只新增了一张 `ChannelTopPostState` 表，没有改动 `ChannelTopPost` 的列；`sync_schema(true)` 会自动建表，老库无需手写迁移，读路径在缺行时返回默认值。
- **存量数据**：两个分量在查询期从存量 `reactionsJson` 现算，升级后不必重扫；代价是每次按质量排序要对本频道最多 300 行做一次 JSON 解析。
- **布局挤压**：已用"排序改成一个下拉按钮 + 5 个菜单项"消除——第二行左半只占一格，不存在裁字问题；代价是多一次点击。
- **名单主观性**：优质/劣质默认名单必然有争议，所以名单必须是设置项，默认值只作起点。
- **水位语义**：以最旧已扫 messageId 为界，隐含"更老的帖子不再回扫"；频道若在窗口外有爆款，只能靠用户主动全量重扫。

## Alternatives considered

- **单一"净分"排序（优质减劣质）**：先把两个分量合成为一个分数只需一个排序项，也最省 UI 空间。但用户明确否决——合并分数会把"100 优质 + 90 劣质"和"10 优质 + 0 劣质"混为一谈，而这两种帖子的性质完全不同。保留三个独立排序项，让用户自己决定看哪一面。
- **只改 LRU 的时间字段，不动上限**：改动更小。但 30 个频道的上限对"逛多个资源频道"的用法仍然太紧，属于同一次改动里顺手修掉的正确性问题。
- **保留定向扫描（只抓 Documents/Media）**：省流量，但会让缓存按筛选口径碎片化——切换筛选时本地根本没有另一类数据，正好和"切换筛选零网络"冲突。
- **面板做成聊天区浮层**：最接近"像搜索一样常驻"的体验，但要动 `Window::SectionWidget` 的宿主模型，超出本轮范围。

## 二轮返工（2026-10-23 二次验收）

真实客户端验收后确认：留驻、记忆、语言三项都没生效。原因是三条判断只做了静态推理，没有一条跑过真机。

### 根因

| 症状 | 根因 | 位置 |
| --- | --- | --- |
| 点卡后榜单消失且回不去 | `Way::Forward` 的同 peer 去重会倒带 section 栈，榜单被销毁 | `Telegram/SourceFiles/mainwidget.cpp:1507-1546` |
| 重开面板回到默认排序 | 入口 `SectionMemento(channel)` 用默认值构造，`createWidget` 里 ctor 从 DB 恢复后紧接 `setInternalState` 用 memento 覆盖 | `Telegram/SourceFiles/ayu/ui/context_menu/context_menu.cpp:441`、`Telegram/SourceFiles/ayu/ui/top_posts/top_posts_section.cpp` 的 ctor 与 `setInternalState` |
| 面板全英文 | `Telegram/Resources/langs/` 没有 zh-Hans 内置包，中文来自云端语言包；本地新增的 `ayu_*` 键永远拿不到译文 | `Telegram/Resources/langs/` |
| 劣质表情排序无意义 | 真实频道 300 行缓存里只有 13 个表情键，默认负名单一个都没命中，"按劣质"退化成按总数 | 默认名单 `Telegram/SourceFiles/ayu/ayu_settings.h` |

### 修法

1. 点卡改用上游"保留来源"的写法（`allowDuplicateInStack` + `origin`）。
2. memento 成为排序/筛选的唯一来源：ctor 只做 touch 与 `setupControls()`，状态全部由 `setInternalState` 应用并把"无讨论组时 Comments 回落 Reactions"也放在那里；菜单入口先用 `getChannelTopPostState` 读出上次的排序/筛选再构造 memento。
3. 面板标签换成能跟随语言的核心键：`lng_manage_peer_reactions`（Reactions）、`lng_comments_header_none`（Comments）、`lng_stats_overview_message_views`（Views）、`lng_filters_all_short`（All）、`lng_media_type_files`（Files）、`lng_media_type_media`（Media）。Positive/Negative 与 AyuGram 专有词保留 `ayu_` 英文，因为全仓 459 个 `ayu_` 键在七个内置包里译文数为 0，且中文包来自云端、无法覆盖本地新增键。
4. 默认正名单补 🤣 😂 😁 🙂 👌；卡片上按帖显示 `👍N` / `👎N` 徽标，让"按优质/按劣质"的排序依据可自证；负名单仍只收"嘲讽/否定"类（`CONTEXT.md` 的 `Negative Reaction` 定义就是 derision or rejection，😢 💔 这类悲伤表情只进总数）。

### 教训

- 编译通过不等于功能可用，布局回归通过也不等于交互正确。凡是"行为是否发生"的验收，必须真机跑一遍，并把观测点写进票里。
- 上游已有的同类交互（从列表跳到消息并保留来源）应当先搜再写，`allowDuplicateInStack` 这类字段就是为此存在的。
