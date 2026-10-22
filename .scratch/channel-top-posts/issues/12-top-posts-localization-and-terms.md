# 12: 本功能本地化与术语收敛

Status: implemented（代码已改，运行未实测）


Blocked by: None

## Summary
面板内不再有硬编码英文：文案进 `lang.strings`，日期按 locale；`CONTEXT.md` 把表情口径拆成三个术语。

## Acceptance Criteria
- [x] `top_posts_inner.cpp` 的 `[Photo]/[Video]/[Document]/[Album / Media Set]/[Message]` 全改 `tr::ayu_TopPostsMedia*`。
- [x] 日期格式改为 `QLocale().toString(..., u"d MMM yyyy"_q)`，跟随应用语言。
- [x] 新增文案同步加键：`ayu_TopPostsSortPositive/Negative`、`ayu_TopPostsMedia*`、`ayu_TopPostsReactionsTitle/PositiveReactions/NegativeReactions/ReactionsAbout`。
- [x] `CONTEXT.md` 拆成 Reaction Total / Positive Reaction / Negative Reaction，旧"unweighted total = valid engagement"已替换。
- [x] 447 个 `ayu_` 键的整体中文翻译不在本票范围（仅英文源串）。

## Implementation Details
- 现有键在 `Telegram/Resources/langs/lang.strings:9195-9215`，全部只有英文值；七个语言包（de/en/es/it/ko/nl/pt-BR）尚无 `ayu_` 键，缺失时回落英文属预期。
- 日期格式化参考 `lang` 里既有的 locale 资产，不要硬编码格式串。
