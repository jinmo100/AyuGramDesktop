# Navigation stack reference

Facts gathered on the 2026-10-25 `dev` tree while diagnosing "the ranking cannot be returned to after visiting comments". Line numbers drift — search the symbols before quoting them.

## The three ways

`SectionShow::Way` (declared in `window/window_session_controller.h`, default `Way::Forward`) decides what a section opening does to `MainWidget::_stack`:

| Way | Effect |
| --- | --- |
| `Forward` | the section being left is pushed onto `_stack`; the new one takes the column |
| `Backward` | the top entry is popped first, then shown |
| `ClearStack` | every entry is dropped; the new section becomes the floor |

State survives a round trip only if it lives in something pushed onto that stack, because what is pushed is a **memento** (`StackItemHistory` / `StackItemSection`), not a widget. `Window::SectionWidget::createMemento()` is the contract that makes the state restorable; a widget that keeps its state only in its own members loses it when the stack pops.

## Push and pop

- `MainWidget::showNewSection(memento, params)` — `auto saveInStack = (params.way == SectionShow::Way::Forward);` and, for a third-column section, `if (newThirdSection) { saveInStack = false; }`. A `Forward` section that lands in the third column therefore **never reaches the stack**.
- `MainWidget::showBackFromStack(params)` — empty stack means "there is nothing to return to": it calls `_controller->clearSectionStack(params)` and focuses the dialogs. Otherwise it pops one item and replays it, dispatching by type: `HistoryStackItem` → `showPeerHistory(peer->id, params.withWay(Backward), ShowAtUnreadMsgId)`, `SectionStackItem` → `showNewSection(memento, params.withWay(Backward))`.
- The back arrow is drawn only while `!content()->stackIsEmpty()`, so a vanished arrow means an empty stack, not a hidden one.

## The trap that cost this session twice

Inside `MainWidget::showHistory`, before the section is shown:

```cpp
if (!params.allowDuplicateInStack) {
    for (auto i = 0, s = int(_stack.size()); i < s; ++i) {
        if (_stack.at(i)->type() == HistoryStackItem
            && _stack.at(i)->peer()->id == peerId) {
            foundInStack = true;
            while (int(_stack.size()) > i + 1) {
                ClearBotStartToken(_stack.back()->peer());
                _stack.pop_back();
            }
            _stack.pop_back();
            if (!back) {
                back = true;
            }
            break;
        }
    }
}
```

Showing a chat that is **already on the stack** truncates everything above it and turns the show into a `Backward` replay. A feature that hangs its state under a chat section therefore dies the moment anything re-shows that chat. Either pass `allowDuplicateInStack`, or give the feature a home outside the stack.

The same function then coerces a plain `Forward` onto the active chat: `if (way == Way::Forward && peer->id == peerId) { way = _mainSection ? Way::Backward : Way::ClearStack; }`.

## Diagnostic recipe

Walk the reported flow and write one line per click: *which section opened, with which `Way`, and what did `_stack` look like after*.

1. Enter the feature — confirm a `SectionStackItem` exists (`Forward`, no third column).
2. Open the post from inside it — the feature's item must still be below.
3. Open the comments — `showRepliesForMessage` builds a `ChatMemento::Comments()` and calls `showSection(memento, params)`; check the `Way` it was given.
4. Leave the comments, then press back twice.
5. If the arrow disappears at step 4, the stack was emptied in step 3: grep the chat-showing path for the dedup loop above, then confirm which caller re-showed that peer.

| Symptom | Most likely mechanism |
| --- | --- |
| back arrow gone after leaving a nested view | stack emptied, usually by a re-show of a chat already on it |
| back returns to the wrong place | a memento whose `createWidget` rebuilt a different state (or `nullptr` for that column) |
| panel content resets on return | state lived in the widget, not the memento |
| a `Forward` section never returns | it opened into the third column (`saveInStack = false`) |

## Panel pattern that avoided the stack

The ranking became a left-column panel instead of a stack section:

- `ayu/ui/top_posts/top_posts_column.{h,cpp}` owns one long-lived `Window::SectionWidget` per window, slides a `Dialogs::Widget`-hosted child, and closes on `Esc`; the section widget is kept alive between showings, so channel, ranking mode, filter and scroll position survive posts and comments by construction.
- `Dialogs::Widget::showTopPosts/hideTopPosts` host that child; `MainWidget::dialogsWidget()` exposes the dialogs widget to AyuGram code.
- `TopPosts::SectionWidget::createWidget` must accept `Window::Column::First` (it previously answered `nullptr` there) and take a back handler so the panel-level back button closes the panel rather than popping the stack.
- The menu entry falls back to the old `showSection` path when the window is one-column, where there is no left column to host a panel.

The general shape: a view that must outlive navigation keeps its own identity and state, and the window only decides where to place it. Anything on `MainWidget`'s stack is by definition transient.
