# 06: 榜单留驻与返回（连续查看第 2、3 条不再重开面板）

Status: implemented（代码已改，运行未实测）


Blocked by: None

## Summary
点开一张卡不再把榜单 section 顶出栈：卡片仍走 Forward 跳转，但返回后榜单仍在、且保持原排序/筛选/滚动位置，可直接看第 2、3 条。

## Acceptance Criteria
- [ ] 点开第 1 张卡后返回，面板仍在栈顶，排序/筛选/滚动位置与离开时一致。（代码侧：卡片仍走 `Way::Forward`，榜单 memento 记 sort/filter/scrollTop；待实测）
- [ ] 连续点开第 1、2、3 张卡，每次返回都回到同一位置，无需重新点菜单。（待实测）
- [x] 面板顶栏返回与系统返回同源：两者都落到 `showBackFromStack()`（`top_posts_section.cpp:423`）。
- [x] 跳转后高亮闪一下目标消息（沿用原有 Forward + msgId 行为，未改动）。

## Implementation Details
- `Telegram/SourceFiles/ayu/ui/top_posts/top_posts_section.cpp:424-427` 目前直接 `showPeerHistory(..., SectionShow::Way::Forward, msgId)`；`Window::SectionShow::Way` 只有 Forward/Backward/ClearStack（`Telegram/SourceFiles/window/window_session_controller.h:164-168`），没有"留驻"选项，需要在 section 层自己决定栈行为。
- 滚动位置与列表内容来自 `InnerWidget`（`top_posts_inner.cpp`），返回时不要重建 `InnerWidget`、不要重跑 `loadPosts()`。
- 回归入口：`.scratch/channel-top-posts/test_ui_and_cards_seams.cpp`、`.scratch/channel-top-posts/test_layout_regression.py`。

## 二轮返工（2026-10-23 二次验收）

点卡时改用 `allowDuplicateInStack = true` + `origin = OriginMessage{FullMsgId}`（上游 `history_item_helpers.cpp:876-884` 的写法），榜单 memento 才会真的留在 section 栈里，聊天顶栏出现返回键。
