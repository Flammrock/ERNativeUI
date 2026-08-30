# Localized Greeting

This deliberately small client demonstrates the complete localization flow:

1. call `erui::query_game_language()` before registration;
2. select translated, compile-time-owned UTF-16 text;
3. use English for unavailable and unknown languages;
4. localize the provider name, row label, help text, and native alert.

`LanguageInfo::identifier` also retains Steam's exact UTF-8 token. A mod may
recognize a community locale there even when `known` is
`GameLanguage::unknown`.
