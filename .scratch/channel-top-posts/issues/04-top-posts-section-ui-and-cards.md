# 04: Slide-Out Top Posts Section Panel & Card Layout

Blocked by: 02, 03

## Summary
Create the `TopPosts::SectionWidget` (subclassed from `Window::SectionWidget`) featuring a top control bar (Sort: Reactions / Comments / Views; Filter: All / Documents / Media), a live scan progress banner, and a scrollable card view with smooth thumbnail/icon rendering.

## Acceptance Criteria
- [x] Section widget slides out smoothly from the right side of the main window using `SectionWidget` / `SectionMemento`.
- [x] Top bar provides a segmented tab or dropdown switching between Reactions, Comments, and Views sort modes.
- [x] When the target channel has no discussion group, the Comments sort button is rendered disabled with a descriptive tooltip.
- [x] Media filter selector toggles between "All", "Documents", and "Photos/Videos".
- [x] Progress banner renders active scan progress (e.g. "Scanning 600/1000 messages...") with an inline Cancel button.
- [x] Card layout displays thumbnail/file icon, total reaction badge (e.g. `🔥 245`), comment count, text snippet or file name, and sent date.
- [x] Empty state renders friendly message when no top posts match the current filter or before first scan.
- [x] Provides a "Clear Channel Cache" option in the panel's action menu.

## Implementation Details
- Create `Telegram/SourceFiles/ayu/ui/top_posts/top_posts_section.h` and `.cpp`.
- Create `Telegram/SourceFiles/ayu/ui/top_posts/top_posts_inner.h` and `.cpp` (inheriting from `Ui::RpWidget` for double-buffered paint).
- Style metrics defined in `.style` files following project guidelines (never hardcoding pixel sizes in cpp).
