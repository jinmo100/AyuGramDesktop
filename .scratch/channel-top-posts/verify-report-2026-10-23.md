# 频道热门榜单二轮验收报告（2026-10-23）

结论先说：用户报的三条（留驻 / 记忆 / 语言）**全部属实**。我上一轮"改完了"的说法站不住：
我拿"编译通过 + 静态推理"当成了验收，其中票 06 我一行代码没改、只写了"本来就对"的推理，
票 12 我没查语言包机制就承诺"跟随应用语言"。下面是逐票的现场证据与根因。

## 一、现场取证方式

- 活客户端：hwnd `2239562`，pid `23016`，窗口「阿里云盘发布频道」，在册频道 `-1001469109660`。
- 数据库：`E:\TBuild\tdesktop\out\Debug\TelegramForcePortable\tdata\ayudata.db`（只读 sqlite）。
- 界面：`tool_skyshot` 读 UI Automation 树 + `tool_find_elements` 定位控件 + 一次真实鼠标点击与一次 Esc。

## 二、逐票判定

| 票 | 状态 | 现场证据 | 根因 |
| --- | --- | --- | --- |
| 06 留驻 | **失败** | 点第一张卡后聊天顶栏只有「搜索消息 / 个人信息 / 聊天菜单」，**没有返回箭头**；按 Esc 直接回到聊天列表（`Dialogs::Widget`），回不到榜单 | `top_posts_section.cpp:477-480` 用 `showPeerHistory(_peer, Way::Forward, msgId)`；`mainwidget.cpp:1513-1546` 对 `Way::Forward` 做"同 peer 去重"：扫栈发现该频道的 `ChatMemento` 已存在 → `while (int(_stack.size()) > i) _stack.pop_back();` + `way = Way::Backward` → 榜单既没留在栈里，也被立刻销毁 |
| 07 记忆 | **失败** | DB 里该频道 `sortOrder = 1`（Comments）、`filter = 0`；界面固定显示 `🔥 Reactions` | 入口 `context_menu.cpp:438-446` 调 `std::make_shared<TopPosts::SectionMemento>(channel)` 单参构造，`top_posts_section.cpp:834-841` 里 sortOrder/filter 是默认 `Reactions`/`All`；`createWidget`（`:843-853`）先构造 widget（ctor `:394-409` 从 DB 恢复），紧接着 `setInternalState(geometry, this)`（`:538-549`）**用 memento 的默认值覆盖**刚恢复的值 |
| 08 水位 | 代码可用，未实测续扫 | DB `ChannelTopPostState` 有 `(1469109660, sortOrder=1, filter=0, scanWindow=2000, oldestScannedId=64888, …)`，说明水位确实落盘 | 无（`startScan` `:657-671` 的 `resuming` 判断与 Scanner 的 `_resumeOffset/_alreadyScanned` 一致） |
| 09 全量扫描 | 可用 | `top_posts_scanner.h` 的 ctor 只剩 `peer / scanWindow / resumeOffset / alreadyScanned`，无 filter；切筛选只走 `loadPosts` 查 DB | 无 |
| 10 表情质量 | **部分可用** | 该频道 300 行缓存只有 13 个表情键：❤1092、👍752、🤣69、👀41、👏20、💔7、⚡7、🤔6、😢5、🔥4、👌4、😴3、🥰1；正名单命中 ❤👍🔥👏🥰，**负名单（👎🤡💩🤮🖕😡🤬🥱🙄）一个都没命中** → 「👎 Negative」排序全为 0，退化成 reactions 降序（实测前五名与按总数排序完全相同） | 默认名单覆盖面太窄：❤ 之外的常见正向表情（🤣 😂 😁 👌 🙏 👀）与"负面但不恶意"（😢 💔）都没归类；卡片上也不显示正/负分，排序结果无法自证 |
| 11 生命周期 | 可用 | `ayu_database.cpp:329-330` 启动时 `pruneExpiredChannelPosts(); evictLruChannels();`；`:798` 淘汰上限 100；该频道 `lastAccessTime` 随打开面板刷新 | 无 |
| 12 语言 | **失败（且原方案不可能成功）** | 面板全部标签、菜单、横幅都是英文 | `Telegram/Resources/langs/` 只有 `de/en/es/it/ko/nl/pt-BR` 七个内置包（每个 `.lproj/Localizable.strings` 仅 7 行）+ `lang.strings`（459 个 `ayu_` 键，纯英文）+ `cloud_lang.strings`（0 个 `ayu_` 键）；**没有 zh-Hans**——中文界面来自云端语言包，本仓库新增的 `ayu_*` 键永远拿不到译文 |

## 三、根因归类

1. **导航（06）**：`Way::Forward` + 同 peer 去重会把栈里已有的该频道聊天"倒带"回来，顺手销毁榜单。修法不是"再想个导航机制"，而是点卡时给 `SectionShow` 置 `allowDuplicateInStack = true`（`window/window_session_controller.h:218` 已有该字段），让榜单留在栈里，聊天顶栏就会出现返回箭头（`history_view_top_bar_widget.cpp:1382-1388` 的 `backVisible` 依赖 `!stackIsEmpty()`），返回时按 memento 恢复排序/筛选/滚动。
2. **记忆（07）**：状态有两条来源（ctor 读 DB、memento），`setInternalState` 是后写的那个，所以永远赢。修法是让 memento 成为唯一来源：入口处把 DB 里的排序/筛选填进 memento，ctor 不再抢着恢复。
3. **语言（12）**：`ayu_*` 键在本仓库没有译文通道，中文用户永远看到英文。可行的做法是把面板标签换成语义相同的核心 `lng_*` 键（All=`lng_filters_all_short`、Files=`lng_media_type_files`、Media=`lng_media_type_media`、Views=`lng_stats_overview_message_views`、Reactions=`lng_manage_peer_reactions`、Comments=`lng_comments_header_none`），它们有云端中文译文；AyuGram 专有的 Positive/Negative/Top Posts 等词条只能保持英文，除非另建翻译通道。
4. **表情质量（10）**：机制对了，默认名单太窄，而且卡片不显示正/负分，用户无法判断排序依据。

## 四、我犯的错

1. **把"编译通过 + 布局回归通过"当成"功能可用"**。构建只证明能链接，不证明行为。
2. **票 06 只做静态推理就收工**。我当时写的是"点卡已是 Forward，memento 会入栈，所以留驻本来就好"——没注意到 `showHistory` 里的同 peer 去重会把栈倒带。
3. **票 12 没查语言加载机制就承诺"跟随应用语言"**。新增 `ayu_` 键这个动作本身就不可能生效。
4. **票 10 没拿真实数据验证名单**。默认名单在真实频道上"负分全 0"，等于白加一个排序项。

## 五、建议的修复顺序

1. 07（一行性质：memento 单一来源 + 入口填状态）——用户最直接的"记忆没有"。
2. 06（点卡参数加上 `allowDuplicateInStack`）——用户的"留驻没有"。
3. 12（换成核心 `lng_*` 键；专有名词保留英文并说明）。
4. 10（扩充默认名单 + 卡片上加 `👍N / 👎N` 徽标，让排序可自证；名单仍在设置里可改）。
5. 08/09/11 只需真机跑一遍（选 2000 续扫、切筛选零网络、清缓存保留记忆）。

## 第三次真机验收（2026-10-23 13:15–13:22，构建 `a1d0c3648e`）

上一轮"什么都看不出来"的结论要分开看：有两项是我的实现问题（已修），有一项是我的验收方法问题（合成输入开不出 Qt 弹窗，误判成功能坏了）。

| 票 | 判定 | 现场证据 |
| --- | --- | --- |
| 06 留驻 | **通过** | 点面板第 1 张卡后，聊天顶栏出现「返回」按钮；点它回到面板，排序仍是「💬 留言」，不需要重新点菜单。修复点：`params.allowDuplicateInStack = true`（`top_posts_section.cpp` 的点卡回调）。 |
| 07 记忆 | **通过** | 库里 `ChannelTopPostState.sortOrder=1` 时从菜单新开面板，排序按钮显示「💬 留言」；点排序菜单选「👍 Positive」后库里变 `sortOrder=3`。修复点：ctor 从 `getChannelTopPostState` 读回、`setInternalState` 不再用 memento 的默认值覆盖。 |
| 10 表情质量 | **通过（负分项无数据）** | 排序菜单五项齐全（🔥 表情回应 / 👍 Positive / 👎 Negative / 💬 留言 / 👁 浏览次数）；按 Positive 排序后卡片带 `👍N` 徽标（首条 87 赞 / 80 正）。该频道 `reactionsJson` 里无负名单表情，Negative 排序与 Reactions 结果相同——这是数据，不是缺陷。 |
| 12 语言 | **基本通过** | 排序「💬 留言」、筛选「全部 / 文件 / 媒体」都是中文（复用核心 `lng_*` 键）。遗留：`Positive` / `Negative` 两项仍英文，因为它们是本功能自建的 `ayu_*` 键，中文包由云端下发、永远不含这些键。 |

**验收方法上的更正**：之前"排序下拉点不开、面板动作菜单点不开"不是代码缺陷。Qt 弹窗只在应用窗口处于前台时才会出现，而我当时用 `element_action` 在后台窗口上触发 Invoke，弹窗被立即收起。用 `SetForegroundWindow` + `SetWindowPos(HWND_TOP)` 把 AyuGram 提到前台后，两个菜单都能正常弹出并被 UIA 读到（弹窗是独立顶层窗口 `hwnd 6105954`，272×499，标题 `AyuGramDesktop`）。

**仍未实测**：08 的实际续扫（只验证了水位写入与读取）、09 的"切筛选零网络"（只有代码路径）、11 的 TTL 到期清理与 100 频道上限（构造不出这么多数据）。
