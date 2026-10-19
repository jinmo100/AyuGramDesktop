# 05: Channel Context Menu Integration & Chat Timeline Jump

Blocked by: 04

## Summary
Add the "Top Posts" entry into the broadcast channel top-right context menu, hook deletion events to invalidate cached entries, and implement smooth scroll-and-highlight navigation to target posts on card click.

## Acceptance Criteria
- [x] Context menu in broadcast channels displays "🏆 Top Posts" (localized via `tr::` system).
- [x] Clicking the menu item launches `TopPosts::SectionMemento` on the active window.
- [x] Clicking any card in the Top Posts list triggers `sessionController->historyView_showAt(peer, msgId)` to smoothly scroll the active chat timeline to that post and pulse the message highlight.
- [x] Global settings (under Ayu settings -> Storage) includes a button to "Clear All Top Posts Caches".
- [x] Incoming channel message revocation events trigger `AyuDatabase::removePost(channelId, messageId)` so deleted posts disappear from the view.

## Implementation Details
- Update `Telegram/SourceFiles/ayu/ui/context_menu/context_menu.cpp` to register the action for channels.
- Add localization strings in `Telegram/Resources/langs/lang.strings` or `Telegram/SourceFiles/ayu/ayu_lang.cpp`.
- Connect message deletion observer in session data controller.
- Wire navigation callback in `TopPosts::SectionWidget`.
