# Localized Greeting

This deliberately small client demonstrates the complete localization flow:

1. establish one explicit API 1.1 connection with `erui::connect()`;
2. call `Connection::game_language()` before registration;
3. select translated, compile-time-owned UTF-16 text;
4. use English for unavailable and unknown languages;
5. localize the provider name, row label, help text, and native alert;
6. register through that same connection without rediscovering the host.

`LanguageInfo::identifier` also retains Steam's exact UTF-8 token. A mod may
recognize a community locale there even when `known` is
`GameLanguage::unknown`.
