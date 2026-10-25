---
name: verify-ayugram-on-device
description: "Drive the locally built AyuGram Desktop Debug executable on the Windows desktop through the win32 UI-automation tools to reproduce, verify and diagnose UI defects: post/comment/back navigation, left-column panels, ranking and search views. Use when the task is to test a UI flow on the real app, to reproduce a reported 'state lost after going back' bug, or to prove that a rebuilt feature actually works."
---

# Verify AyuGram Desktop on the device

Reproduce first, then read code, then prove the fix on a rebuilt binary. The claim you report must be backed by the exact build you ran, the click targets you used, and a screenshot or element dump of the state after each step.

## Completion criteria

Do not close the loop until all four hold:

- the executable that answered was linked **after** the last source edit, and you can name its mtime and the process start time that came later;
- the reported flow was walked end to end on that process — the same order the user reported it, not a shortcut;
- every claimed UI state has a saved screenshot path or an element dump showing it;
- what was **not** verified is written down (scan progress, sync, another account, narrow layout).

A successful link, a visible toolbar, or a widget that "should" behave that way establishes nothing.

## Steps

### 1. Pin the build

- [ ] Read `out\Debug\AyuGram.exe` mtime and the running process start time. If the exe is newer than the process, the live window is stale code — stop that instance and launch again.
- [ ] Reuse the build path from [build-windows-ayugram](../build-windows-ayugram/SKILL.md): WSL edits, `E:\TBuild` build tree, `E:\TBuild\compile_debug.bat`.
- [ ] Launch through a script that records the pid, then confirm the window appears with `tool_list_windows`.
- [ ] One debug instance at a time. The user often has their own client open — find both and pick the one whose pid matches `AyuGram.exe` under `E:\TBuild`.

### 2. Address the window, not the screen

- [ ] `tool_find_window` / `tool_list_windows` gives hwnd, pid and geometry. Client area may start at `(0,0)` while the frame is at `(-11,-11)`.
- [ ] Before any hit-test or physical click, raise the target: `SetForegroundWindow` alone fails silently when another process owns the foreground.
- [ ] Sanity check every UIA hit-test: the returned pid must be the app's. A `Chrome_WidgetWin_*` or a foreign pid means another window covers that point.

Raise recipe (WSL side, no quoting hazards — PowerShell reads `/tmp` of the WSL box only through `-EncodedCommand`, never `-File`):

```bash
cat > /tmp/psrun.sh <<'EOF'
#!/usr/bin/env bash
# usage: psrun.sh <ps-file>
enc=$(iconv -f UTF-8 -t UTF-16LE "$1" | base64 -w0)
/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe \
  -NoProfile -ExecutionPolicy Bypass -EncodedCommand "$enc"
EOF
```

Inside the `.ps1`, `ShowWindow(h, 3)` then `AttachThreadInput(GetCurrentThreadId(), GetWindowThreadProcessId(GetForegroundWindow()), true)` around `SetWindowPos(h, 0,0,0,0,0, SWP_NOSIZE|SWP_NOMOVE|SWP_SHOWWINDOW)` + `SetForegroundWindow` + `BringWindowToTop`, and print `GetForegroundWindow() -eq h` so the call is verified, not assumed.

### 3. Interact element-first

- [ ] `tool_find_elements(hwnd, name_contains="聊天菜单", role="button")` → `tool_element_action(ref, "press")`. The UIA path delivers a pattern: no cursor, no focus change, immune to z-order.
- [ ] `list_item` elements expose `invoke` and reach entries the viewport does not show, which is how a channel buried thousands of pixels down the dialog list is opened without scrolling.
- [ ] An icon button may report a `0×0` rect while the window is not foregrounded. Re-query after raising instead of concluding the control is gone.
- [ ] Coordinates are the last resort. When they are unavoidable, take them from a capture of the same window (step 4) and cross-check with `tool_element_at_point`.

### 4. Screenshots: measure, never guess

- [ ] `tool_capture_window(hwnd, save_path=...)` returns client-only pixels plus `bounds`, `scale_x`, `scale_y`.
- [ ] `read_image` reports its own downscale factor — multiply by that before quoting a coordinate, then confirm by cropping a narrow strip around the suspected boundary.
- [ ] The app runs at 150% interface scale on a 3840-wide screen: a 1600-wide preview maps by ~2.4, and the dialog column ends near x≈956 physical. Those numbers are measurements of this machine, re-measure after any layout change.
- [ ] Save every state you intend to cite to disk; do not describe a state you only saw once.

### 5. Yield to the human

- [ ] `raw_event` paths refuse with `user-active` when someone is at the keyboard. That is not an error to retry — switch to the element path, or wait and say so.
- [ ] Never drive the window the user is actively working in. Announce before any physical click.

### 6. Reproduce the reported flow, in the reported order

- [ ] Walk the exact path: enter the panel → open the post → open its comments → leave the comments → press back → back again.
- [ ] After each step record which pane owns what. A pane quietly dropping its content between two steps is the bug, and it is only visible when the steps are walked in order.
- [ ] When the report says "state is lost", capture both ends: the state before entering, and the same place after returning.

### 7. Diagnose against the navigation model

Read [reference/navigation-stack.md](reference/navigation-stack.md) before blaming a widget: this app pushes sections onto `MainWidget`'s stack, and "back" is that stack popping, not a widget hiding itself. Most "cannot go back" reports are the stack, not the view.

- [ ] Trace the run: for each click, name what was pushed or popped, and which `SectionShow::Way` was used.
- [ ] Confirm with symbols (`showNewSection`, `showBackFromStack`, `showRepliesForMessage`, `SectionMemento::createWidget`) rather than trusting recorded line numbers.
- [ ] Only then decide: fix the stack semantics, or give the feature its own home outside the stack.

## Traps met on this machine

| Symptom | Cause | Fix |
| --- | --- | --- |
| `-File` cannot find the script | the `.ps1` lives in WSL `/tmp`, invisible to Windows PowerShell | pipe it via `-EncodedCommand` (step 2 helper) |
| element hit-test returns a foreign pid | another maximized window (the DSH shell, a browser) covers that point although the app is foreground | raise the target, re-hit-test, or stay on the element path |
| an icon button reports `0×0` | top bar not laid out for a non-foreground window | raise, then re-query |
| the injected click lands in the wrong pane | coordinates converted from a downscaled preview by hand | use `scale_x`/`scale_y` from the capture, verify by cropping |
| `user-active` refusal | a person is typing | stop injecting, use element actions, or ask |
| a rebuilt feature still looks old | the exe was rebuilt while the old process kept running | compare mtime with process start, relaunch |
| the panel is there but nothing paints | only a layout pass was triggered | see the layout regression note in [build-windows-ayugram](../build-windows-ayugram/SKILL.md) |

## Report shape

State, in this order: what was rebuilt (mtime, pid), the flow walked (click targets), what passed with the screenshot paths, what failed with the same evidence, and what remains untested. Separate "verified on the device" from "read from code".
