# Agent Note: 榜单左栏面板的退出入口

Status: implemented

## Problem

左栏榜单面板（`ayu/ui/top_posts/top_posts_column.cpp` 把 `TopPosts::SectionWidget` 装进对话列表栏）上线后，用户报告"打开榜单后没有退出键，只能浏览这一个频道"（消息记录 m01326）。

实测截图（m01324）里左栏顶行只有标题"Top Posts"，右侧空白——返回箭头和右上角三点菜单都不见了。逐行核对代码后的结论：

1. **两个头部按钮从来没有尺寸**。`FixedBar` 里返回按钮用的是 `Profile::BackButton`（`top_posts_section.cpp` 原来的 `:170`），该控件靠 `moveToLeft/moveToRight` 定位，尺寸由它自己的 `resizeGetHeight` 决定；但整个 `FixedBar` 从未给它 `setGeometry`，`width()` 恒为 0。三点菜单 `_menuToggle` 的 `resizeGetHeight` 里只有一次 `moveToRight(...)`，同样没有尺寸，`Ui::RpWidget` 的默认尺寸是 0×0，于是"存在、已 show、但面积为 0"，画不出来也点不到。像素证据吻合：标题用 `_backButton->width() + st::topPostsTabMargin` 定位，实测标题起点距面板左缘约 6 个逻辑像素，正好等于 `topPostsTabMargin`。
2. **Esc 也没用**。`Column::keyPressEvent` 里虽然接了 `Qt::Key_Escape`，但 `Column` 只有在**自己**获得焦点时才会收到按键；面板里全是 `RippleButton`、滚动区这类自带焦点的子控件，按键永远到不了 `Column`。

首版榜单是右侧整栏 section，头部用的是主窗口通用的返回键与三点菜单，天然有尺寸；改成左栏自绘面板后，这部分必须自己负责，于是漏了。

## Decision

补上可见的退出入口，三条路都能走：

1. **返回箭头**：新增样式项 `topPostsBackButton`（`ayu_styles.style:157`，`IconButton(topBarSearch)` 派生，`title_back` 图标），`_backButton` 改用它（`top_posts_section.cpp:172`），并在 `resizeGetHeight` 里显式 `setGeometry(0, 0, st::topPostsBackButton.width, st::topBarHeight)`（`:342`）。点它 `_backRequests.fire({})`，`Column` 收到后 `hideColumn()`，对话列表滑回来。
2. **右上角三点菜单**：同样在 `resizeGetHeight` 里显式给尺寸（`:347`），并在菜单末尾加一条分隔线和"关闭"项（`tr::lng_close`，`:334`）——菜单本身早已有刷新/重扫/清缓存，这条让"退出"在菜单里也标着名字。
3. **Esc**：`Column` 构造时创建一个 `Qt::WidgetWithChildrenShortcut` 的 `QShortcut`（`top_posts_column.cpp:39`），子控件有焦点时也生效；原先的 `Column::keyPressEvent` 保留（`:138`）。

顺带把频道名画进顶行（`paintEvent`，`:379`）：标题右边、`_menuToggle->x()` 之前，用 `st::normalFont` + `st::windowSubTextFg`，`elidedText` 截断。左栏替换掉了对话列表，"现在看的是哪个频道"在旧版是靠 section 顶栏说的，自绘面板必须自己说。

## Consequences

用户侧：面板顶行变成"← Top Posts 频道名 ⋯"，点箭头、按 Esc、菜单里选"关闭"都能回聊天列表；榜单本身（排序、筛选、滚动位置）在 `Column` 里留驻，再打开还是原样。

代价与遗留：多了一个样式项；标题的起点现在依赖 `st::topPostsBackButton.width`，改按钮宽度会连带挪标题（这比依赖 `Profile::BackButton` 的内部 margin 更可控）；频道名过长时靠省略号收尾，没有 tooltip。

## Alternatives considered

- **继续用 `Profile::BackButton`，只补 `setGeometry`**：它的绘制依赖调用方给出的矩形与内部 margin 组合，`moveToLeft` 的语义是"贴左并自己算边距"；直接 `setGeometry` 会和它自己的布局互相打架，而且标题定位要读取它算完之后的几何。换成一个纯 `IconButton` + 显式矩形，把"顶行三个元素的位置"集中在一个函数里。
- **只加菜单项，不加箭头**：菜单里已经有一堆命令，"退出"藏在里面不算恢复了用户预期的返回入口；用户的原话就是"没有退出键"，缺的是可见的控件。
- **Esc 改用窗口级快捷键**：主窗口自己处理 Esc（取消搜索、退出当前 section），挂全局面板级快捷键容易和这些路径抢同一次按键；`WidgetWithChildrenShortcut` 只在面板可见且在其内部有焦点时生效，范围刚好。

## Implementation

- `Telegram/SourceFiles/ayu/ui/ayu_styles.style:157` — 新增 `topPostsBackButton`。
- `Telegram/SourceFiles/ayu/ui/top_posts/top_posts_section.cpp` — `:172` 换控件；`:342` 与 `:347` 显式几何；`:334` 菜单"关闭"项；`:379` `paintEvent` 画标题与频道名。
- `Telegram/SourceFiles/ayu/ui/top_posts/top_posts_column.cpp` — `:39` Esc 快捷键；`:138` 保留 `keyPressEvent` 兜底。
