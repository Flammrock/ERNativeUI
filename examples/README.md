# ERNativeUI examples

These C++17 client DLLs use only the header-only ERNativeUI SDK. They do not
link an import library. Build all examples from this directory, or open an
individual example when you want a smaller project.

| Example | Use it when you want to... | Load in an ordinary setup? |
|---|---|---|
| [Localized Greeting](localized_greeting/README.md) | Learn the connection, language fallback, one row, and one native dialog | Yes, as a small demonstration |
| [Tarnished UI Showcase](tarnished_ui_showcase/README.md) | Exercise the broad stable menu surface plus API 1.4 presentation, availability, and visibility regressions | Yes, for integration testing |
| [Showcase Screenshot Helper](showcase_screenshot_helper/README.md) | Select one isolated English control, custom tab, title-screen layout, or focused API 1.4 row-update scene from an INI and capture deterministic documentation images | Only while preparing documentation |
| [Client-mod template](template/README.md) | Start a real mod from extensively commented source | No; copy and rename it first |
| [Custom Tabs Stress](custom_tabs_stress/README.md) | Exercise a fixed 50-tab API 1.3 workload, transparent custom RGBA8 icons, independent state, and pagination | Only as an isolated stress/integration test |
| [Title-Screen Text](title_screen_text/README.md) | Register API 1.3 status lines in all four title-screen corners with inherited and explicit colors | Yes, as a focused API 1.3 demonstration |

In every case, Mod Engine 2 must load the shared `ERNativeUI.dll` before the
client DLL. See the [SDK setup guide](../docs/getting-started/setup.md) for
standalone builds and the [first mod walkthrough](../docs/getting-started/first-mod.md)
for a complete beginner-friendly path.
