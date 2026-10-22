# Agent Note: 频道热门资源与多维热度榜单（按点赞/评论排序）

Status: implemented

## Problem

在 Telegram 广播频道（尤其是软件、影视、音乐、电子书等资源分享类频道）中，帖子随时间线性累积，往往包含成千上万条历史消息。在实际使用场景中，用户寻找高质量资源的核心痛点是：
1. **时间倒序检索效率极低**：官方客户端（包括 Shared Media 媒体概览）仅支持按 Message ID / 时间倒序浏览，或仅支持纯文本关键词搜索。用户想找该频道历史上最受好评、最精华的高赞资源时，必须手动滑屏翻阅数万条历史消息。
2. **缺乏多维热度筛选**：资源贴的“受欢迎程度”通常强烈反映在**表情点赞量（Reactions Count）**和**评论讨论量（Comments/Replies Count）**上（高赞代表资源质量高，高评论代表讨论热烈、反馈集中）。目前客户端完全缺失该能力。
3. **服务端无排序支持与 API 频控限制**：MTProto 协议服务端的 `messages.search` 与 `messages.getHistory` 不支持任何按 Reactions 或 Comments 排序的参数。若在客户端无限制全量抓取，将极易触发 Telegram 服务端的 `FLOOD_WAIT` 严重风控。

因此，需要在客户端提供一套安全、有界扫描、带本地缓存淘汰机制的多维热度榜单功能。

## Decision

在 AyuGram 频道聊天界面的顶部右上角菜单以及 AyuGram 菜单中，新增“🏆 热门资源榜”（`tr::ayu_TopPostsMenuText`）入口，通过轻量级异步扫描与本地数据库持久化，提供基于表情点赞量、评论数与浏览量排序的独立滑出面板。

### 1. 扫描引擎与有界安全策略 (`TopPosts::Scanner`)
- **异步分批请求**：使用 `MTPmessages_Search`（配合 `MessagesFilter`）或 `MTPmessages_GetHistory`，单批次请求 100 条。
- **请求节流保护**：每次网络请求之间强制加入 150ms ~ 250ms 的安全缓冲间隔，监听 `FLOOD_WAIT` 错误并提供倒计时提示，支持用户随时暂停/取消。
- **自定义扫描深度**：
  - 预设选项：500 条（极速，~2s）、1000 条（默认推荐，~4s）、2000 条（深度，~8s）。
  - 自定义上限：允许用户输入数值，但设立硬性安全上限（默认最大 5000 条），防止误操作导致封号。
- **媒体分类定向扫描**：
  - 支持“全部消息”、“仅文件/软件 (Documents)”、“仅图片与视频 (Photo & Video)”。利用服务端过滤器剔除纯文本水贴，大幅提高单次扫描的有效资源密度。

### 2. 热度提取与多维排序
从消息体 `HistoryItem` 中一次性提取三项指标（零额外网络请求）：
- **表情互动量 (Reaction Engagement)**：累加 `MessageReactions` 中所有表情的 `count`（所有表情不论正负均具有收集与热度参考价值，不作倾向性权重过滤）。**注：后半句已被 `implemented/feature/2026-10-23-top-posts-follow-up.md` 取代**——总数口径与排序保留，但不再是唯一口径，新增了按优质 / 劣质表情的独立排序。
- **评论讨论数 (Comments)**：读取 `repliesCount()`；若频道未关联讨论组（`repliesAreComments()` 为假），顶部“按评论量”排序按钮保持显示但置灰，鼠标悬停展示明确提示。
- **浏览量 (Views)**：读取 `viewsCount()`。
- **媒体合集聚合 (Media Group)**：对具备相同 `grouped_id` 的连续消息（如相册或分卷附件）聚合成单张卡片，汇总展示文件总量，点击卡片定位至合集首条消息。
面板支持按“🔥 按点赞量”、“💬 按评论量”、“👁️ 按浏览量”无缝切换降序排列。

### 3. 数据持久化与缓存删除策略 (`AyuDatabase`)
在 `./tdata/ayudata.db` 中新增 `ChannelTopPost` 实体表：
- 字段包括：`userId`, `channelId`, `messageId`, `date`, `reactionsCount`, `commentsCount`, `viewsCount`, `textSnippet`, `mediaType`, `reactionsJson`, `scanTime`。
- **三层缓存淘汰与清理策略**：
  1. **单频道配额截断**：扫描入库后自动执行截断，每个频道仅保留前 300 条精华记录，低热度数据即时剔除，避免数据库膨胀。
  2. **TTL 与全局 LRU 淘汰**：缓存保存期 30 天，超过 30 天未重新访问的频道缓存自动失效；全局最多保留 30 个频道的榜单数据，超出时按最近访问时间淘汰。
  3. **用户主动清理与撤回同步**：面板提供“清空本频道缓存”按钮，Ayu 设置的 Storage 页面提供“清空所有榜单缓存”选项；监听到频道消息被管理员撤回时，同步从缓存中剔除并自动刷新视图。
- **智能增量同步 (Incremental Sync)**：
  - 点击刷新时，既拉取从上次最新 `messageId` 到当前的新消息，同时通过 `messages.getMessages` 批量带回本地已缓存 Top 300 消息的最新 Reaction/Replies 数据（仅需额外 1~2 次请求），兼顾高响应速度与老帖点赞增长更新。

### 4. UI 架构与交互设计 (`TopPosts::SectionWidget`)
- **独立滑出面板**：继承 `Window::SectionWidget`（参考现有的 `MessageHistory::Widget`），从主窗口右侧滑出，保持原生体验与架构解耦。
- **卡片式列表**：展示缩略图/文件图标、高亮反应数与评论数徽章、文件名/摘要前两行、发布日期。
- **双向联动跳转**：点击卡片通过 `sessionController->historyView_showAt(peer, msgId)` 直接在主聊天区域平滑滚动定位并高亮闪烁，便于下载和查看讨论。

## Consequences

### 收益与影响
1. 用户在大型资源广播频道中可秒级发现历史上最受欢迎的高质量资源，彻底摆脱数万条历史消息逐屏翻找的困境。
2. 架构上严格遵循有界扫描和分批防抖，请求之间保持安全间隔，完全避免了触发 Telegram 服务端风控的风险。
3. 本地 SQLite 缓存与 300 条上限配额、30 天 TTL、30 频道 LRU 淘汰机制确保了磁盘占用极小，同时支持频道离线秒开与低频宽增量刷新。
4. 消息撤回和全局设置存储清理打通了数据生命周期闭环，避免陈旧或已删资源驻留客户端。

### 约束与维护要点
1. 增量刷新仅更新最近新消息及已入榜 Top 300 项，若早期冷门帖发生剧烈反应爆发，需通过全量重新扫描获取更新。
2. 仅在广播频道（Channel）右上角菜单和 AyuGram 菜单中展示该入口，普通群聊与私聊不开放此功能。

### Windows / Qt 5 编译兼容修复

原生 Windows Debug 编译前检查发现：构造函数缺少函数体起始括号、数据库枚举与全局 ID 的命名空间误用、外层异步 lambda 未捕获内层回调所需的 `this`，以及 Qt 5 不接受 `std::string` 的 `QString::fromUtf8` 重载。修复采用显式命名空间、补齐捕获与 `QString::fromStdString`；主线程回调继续由弱引用保护，数据库后台写入不访问对象成员。使用 E 盘的 MSVC / Qt 5.15.19 原生 Debug 编译验证，依赖、缓存与临时目录统一位于 `E:\TBuild`。静态 seam 检查无法代替真实编译；最终编译结果以构建日志与退出码为准。

语言生成器还拒绝非复数键中的保留占位符 `{count}`。扫描与同步进度改用 `{current}`，数字与等待秒数显式转换为 `QString`；真正的项目数量文案保留 `#one` / `#other` 与 `{count}`。这样保持进度文案语义，也符合生成的普通占位符接口。

MSVC 实际编译继续暴露接口不匹配：离开事件应覆盖 `leaveEventHook`；设置页基类、翻译投影和样式需要显式包含声明；数据库清理事件的常量捕获使用 `fire_copy`；LRU 内部函数移到调用之前的匿名命名空间。频道缓存统一经 `getBareID` 获取不含类型标记的正数 ID，避免直接改用 `PeerId::value` 导致缓存键变化。项目没有 `defaultFlatButton` 实例，因此新增继承 `historyComposeButton` 的榜单按钮样式，并将高度与文字偏移保留在 `.style` 中。标题改用现有 `semiboldFont` / `windowFg`，通过字体度量垂直居中，去掉未缩放的像素补偿。

2026-10-20 原生 Windows x64 Debug 构建完成，`telegram-debug.exit` 为 `0`，日志确认 `Telegram.vcxproj` 输出 `E:\TBuild\tdesktop\out\Debug\AyuGram.exe`（并非 `Telegram.exe`）。产物版本 `7.2.9.0`，大小 520677376 字节，SHA-256 为 `8DC00EDB09B36934F932431753AA9A42BF07E3913725EFE3F791EED375773113`。便携标记目录已存在；此次验证覆盖配置、语言与样式生成、MSVC 编译和链接，未运行登录后的频道榜单交互。

Windows 构建技能同步记录本次恢复边界：优先复用已完成依赖与 Qt 安装产物，只恢复经证实为空的子模块；成功以本轮退出码、链接日志和新产物共同确认。重跑完整准备流程的优势是统一重建依赖，但会增加耗时并可能清理已修复的 Qt 源码，因此已有完整依赖时采用增量构建，缺项时只恢复对应阶段。技能中的版本与耗时属于历史快照，后续必须检查现场环境。

实机出现“工具栏可见、内容全空”的回归后，布局验证直接抽取生产 `SectionWidget::updateLayout`、`InnerWidget::setPosts` 与 `resizeGetHeight` 方法体，在原生 Qt Widgets / QScrollArea 中执行。旧版六项几何断言全部失败：滚动区未给内容设置宽度，`setPosts` 直接调用只计算高度的 `resizeGetHeight`，返回值被丢弃，因此空文案和卡片都没有可绘制区域。修复显式调用 `resizeToWidth` 应用计算结果，并在页面变化时同步内容宽度；该测试使用最小无边距 RpWidget 适配器，覆盖布局契约，不代表登录后网络扫描已验证。

修复后六项布局断言全部通过，Windows Debug 再次编译链接成功。原生 Windows UI Automation 在同一频道实测显示真实帖子；点赞排序首条为 87，浏览量排序首条为 376.8K，评论排序首条为 12，Files 筛选显示无文件提示。用户随后接手剩余测试，未继续验证全量重扫、频控恢复与长期同步。回归入口为 `.scratch/channel-top-posts/test_layout_regression.py`，配套 CMake 仅复用已有 E 盘依赖，避免把生产逻辑重新写一遍当作测试；技能同步记录了几何契约与实机证据边界。

## Alternatives considered

### Why not client-side unbounded full scan (客户端全量无界扫描)?
- **最强论据**：全量遍历频道历史的所有消息（几十万条），能保证绝对完整、毫无遗漏的全局历史排行榜。
- **否掉理由**：单次最多拉取 100 条消息，几十万条消息意味着数千次连续高频 API 请求。Telegram 服务端具有极其严苛的调用频控，这必然会频繁触发小时级的 `FLOOD_WAIT`，甚至导致用户账号被官方风控封禁；同时耗时数十分钟，实用性完全破产。

### Why not only sort currently loaded viewport messages (仅排当前内存中已拉取的消息)?
- **最强论据**：实现最简单（如浏览器端 userscript），完全不需要发起额外网络请求，零被风控风险。
- **否掉理由**：Telegram Desktop 本地内存仅缓存当前屏幕及附近几十条消息，在海量历史资源的广播频道中，仅排序视口内这几十条消息没有任何意义，根本无法满足用户“挖掘历史高赞/高分资源”的核心诉求。

### Why not in-memory ephemeral session (纯内存临时会话，不落盘)?
- **最强论据**：完全不修改 SQLite 数据库 schema，不需要考虑数据库迁移和数据膨胀清理策略，架构极简。
- **否掉理由**：用户每次切换或重新打开面板都需要等待数秒重新向 Telegram 服务器抓取，浪费流量和 API 额度，且完全无法支持增量刷新和离线秒开体验。

### Why not external third-party scraper bot/service (依赖第三方中心化爬虫或索引服务)?
- **最强论据**：将高并发抓取和索引压力转移至云端服务器，客户端只需请求后端 API 即可毫秒级获取全量排序。
- **否掉理由**：彻底违背了 AyuGram 作为去中心化开源本地客户端的隐私承诺与安全底线；需要架设和维护长期中心化服务器，成本高且引入第三方单点依赖。
