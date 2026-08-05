# AGENTS.md — Vela

## Architecture Overview

This is a **Qt6/C++23 desktop GUI** (Linux) that wraps the `protonvpn` CLI tool. The app has **no direct VPN or network logic** — it drives everything through `QProcess` calls to the `protonvpn` command-line binary.

```
Vela (QML UI, module URI Vela)
  ├── qml/views/    (LoadingView, LoginView, NotInstalledView, MainView, SettingsView, AccountView)
  ├── qml/components/ (P* primitives: PButton, PSwitch, PTextField, PToast, ...)
  ├── app/          (QML app layer: VpnFacade, serverListModel, trayController, main.cpp)
  └── core/         (vpncore static lib: VpnService, StateMachine, CliClient, StatusPoller)
        └── drives `protonvpn <subcommand>` via QProcess
```

**Key data flows (QML):**
- QML → `VpnFacade` → `VpnService` → `protonvpn <cmd>` subprocess → signals back through the facade to QML
- `StatusPoller` keeps a long-lived subprocess running `bash -c 'while true; ... sleep 15'` polling `protonvpn status`; steady cadence is 15 s (transition cadence faster while state changes)
- `CliParsers` parse `Key: Value` lines from `protonvpn status` into `QMap<QString,QString>` and command output into typed structs
- `VpnService::applyStatusFields()` (via snapshot) translates the map into `VpnState` enum changes and emits `stateChanged(VpnState, QString)`

**Layering rule:** `core/` links **no** GUI modules (only `Qt6::Core`) so it stays testable. `app/` + `qml/` are the QML-only UI layer.

## Build & Run

Requirements: **CMake ≥ 3.31**, **Qt ≥ 6.8** (needed for `qt_standard_project_setup(REQUIRES 6.8)` and the QML module build), C++23. Debian trixie / Ubuntu 26.04 / Fedora 42 or newer.

```bash
# Configure (from src/)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
# Build
cmake --build build
# Run
./build/vela
# Run all tests
cd build && ctest --output-on-failure
```

Pre-built configs exist at `src/cmake-build-debug/` and `src/cmake-build-release/`.

## Flatpak Sandboxing

All `QProcess` spawning **must** go through `buildHostCommand()` (`core/hostCommand.h`):

```cpp
auto [prog, args] = buildHostCommand("protonvpn", {"connect", country});
process->start(prog, args);
```

The `protonvpn` CLI is **bundled inside the sandbox** and on `PATH`, so `buildHostCommand()` returns the program unchanged (no `flatpak-spawn --host` delegation; that was removed). It still exists as the single QProcess funnel. Never call `QProcess::start("protonvpn", ...)` directly.

The CLI's NetworkManager backend reaches the **host** NetworkManager daemon over the shared system bus via the `--system-talk-name=org.freedesktop.NetworkManager` finish-arg (same permission as the official `com.protonvpn.www`). The sandbox ships a source-built `libnm` + `NetworkManager-1.0.typelib`; the in-sandbox NM daemon binary is never launched.

## Key Files

| File | Purpose |
|---|---|
| `core/vpnService.h/cpp` | Central service in `vpncore`; all CLI calls, auth, state, list fetches; emits typed signals |
| `core/vpnStateMachine.h/cpp` | `VpnState` enum state machine (Connected, Connecting, Disconnected, ...) |
| `core/cliClient.h/cpp` | QProcess spawning + per-command parsing of `protonvpn` output |
| `core/cliParsers.h/cpp` | Pure parsers for status fields, server info, command output |
| `core/statusPoller.h/cpp` | Background `protonvpn status` polling subprocess via `processRunner` |
| `core/processRunner.h/cpp` | Thin QProcess wrapper (start, output capture, cancellation) |
| `core/hostCommand.h` | `buildHostCommand()` subprocess funnel (CLI is bundled in-sandbox; returns program unchanged) |
| `core/natPmpService.h/cpp` | Port-forwarding (NAT-PMP) support |
| `app/vpnFacade.h/cpp` | QML-exposed facade bridging `VpnService` and QML views |
| `app/serverListModel.h/cpp` | `QAbstractListModel` feeding country/city lists to QML |
| `app/trayController.h/cpp` | Tray icon + notifications + single-instance raise |
| `app/main.cpp` | QML engine, D-Bus registration, single-instance lock, version from `version.json` |
| `qml/Main.qml` | Root window / view stack of the QML UI |
| `qml/theme/Theme.qml` | Singleton color/palette properties for the QML UI |
| `appConfig.h/cpp` | App preferences → `~/.config/Vela/app.json` |
| `connectionHistory.h/cpp` | Recent connections → `$XDG_DATA_HOME/Vela/history.json` |
| `favoritesManager.h/cpp` | Favorites (starred cities/servers) |
| `geoUtils.h/cpp` | Country/continent/coordinate helpers for the map |
| `i18n/vela_en.ts` | Translation catalog (QML `qsTr` + C++ `tr` strings) |
| `version.json` | Single source of truth for `app_version` and tested CLI range |

## Conventions

- **C++23**, strict conformance (`-extensions OFF`), `#pragma once` everywhere
- **No `.ui` files** — widget layout is built programmatically; the QML UI needs none
- **Singletons** via `static T& instance()`: `AppConfig`, `ConnectionHistory`, `FavoritesManager`
- **Logging**: use `DBG_APP(msg)`, `DBG_CLI(msg)`, `DBG_SETTINGS(msg)` macros (stdout, tagged+timestamped). Never use `qDebug()`.
- **Versioning**: single source of truth is `src/version.json` (keys: `app_version`, `cli_version_tested_min`, `cli_version_tested_max`, optional `prerelease`); read at runtime via embedded resource `:/version.json`
- **Palette**: dark Proton-branded theme (bg `#1a1a2e`, accent purple `#6d4aff`); QML reads it from the `Theme.qml` singleton
- **Translations**: Qt Linguist, source file `i18n/vela_en.ts`; QML strings use `qsTr()` (context = file basename), C++ uses `tr()`. Regenerate with `lupdate6` over `app/` and `qml/`, compiled by `qt_add_translations` into `:/i18n/vela_en.qm`.
- **Language**: American English only — variable names, comments, and default/fallback text strings (e.g. `color` not `colour`, `canceled` not `cancelled`, `initialize` not `initialise`)

### Code Style

- **No if-init syntax**: do not use `if (init; condition)` — declare the variable on a separate line before the `if`:
  ```cpp
  // OK
  QHBoxLayout* hl = qobject_cast<QHBoxLayout*>(layout());
  if (hl != nullptr) { ... }

  // Not OK
  if (auto* hl = qobject_cast<QHBoxLayout*>(layout()); hl != nullptr) { ... }
  ```
- **Constant placement**: declare `constexpr` constants above the function or class that uses them, not inside function bodies. For `.cpp` files use an anonymous namespace; for class-scope constants use `static constexpr` members. For header-only free functions where an anonymous namespace is inappropriate (Clang-Tidy warns), use a named inner namespace (e.g. `namespace Detail`) or promote them to class-scope `static constexpr` members if a class is nearby.
- **Magic numbers**: never use numeric literals inline — define named constants using `constexpr` (or `static constexpr` at class scope) with `UPPER_SNAKE_CASE` names:
  ```cpp
  // OK
  constexpr int SIDEBAR_WIDTH = 64;
  constexpr int NAV_ICON_SIZE = 24;
  m_sidebar->setFixedWidth(SIDEBAR_WIDTH);

  // Not OK
  m_sidebar->setFixedWidth(64);
  ```
  String and boolean literals are exempt. Enumerators (which already have names) are also exempt. The literal `0` is also generally exempt when used as a neutral zero (e.g. empty margins, start indices, zero spacing) — only name it when `0` carries domain-specific meaning (e.g. "feature disabled" sentinel).

- **Brace style**: GNU/Allman — opening brace on its own line for functions, classes, and control structures
- **`auto`**: avoid for simple/obvious types; use explicit types (e.g. `int count = 0;`, `QString name = ...`). `auto` is acceptable where the type is verbose or deduced from a template (e.g. range-for over complex containers, structured bindings)
- **Loop bodies**: ALL loops (`for`, `while`) must use curly braces — no single-line unbraced loops, no exceptions
- **No `do`/`while` loops**: use a `while` loop instead
- **`switch` case bodies**: `case` labels are indented one level inside the `switch` block; the body always starts on the next line after the label:
  ```cpp
  // OK
  switch (state)
  {
      case VpnState::Connected:
          handleConnected();
          break;

      case VpnState::Error:
          handleError();
          break;

      default:
          break;
  }

  // Not OK
  case VpnState::Connected: handleConnected(); break;
  ```
- **Boolean negation**: use `== false` instead of `!` in conditions — `if (ok == false)` not `if (!ok)`. Likewise prefer `== true` when it improves clarity over a bare identifier.
- **Pointer null checks**: always use `== nullptr` or `!= nullptr` explicitly — never rely on implicit pointer-to-bool conversion (`if (ptr)` or `if (!ptr)`).
- **Condition bodies**: `if`/`else` bodies must use curly braces **unless** the body is a bare `return;` (void), `return true;`/`return false;` (boolean), `break;`, or `continue;`, in which case the body may appear on the same line as the condition without braces. Everything else — including assignments, function calls, and any other return expression — must use curly braces:
  ```cpp
  // OK — bare void/boolean return, break, or continue, same line
  if (ok == false) return;
  if (m_value == value) return;
  if (found == false) return false;
  if (done) break;
  if (skip) continue;

  // Not OK — must use braces
  if (x) doSomething();                  // function call
  if (x) return m_value;                 // non-boolean return expression
  ```

## Testing

Tests live in `src/tests/` and use **Qt Test** (`QtTest/QtTest`). Each test file maps to one logical unit — the naming convention is `tst_<unit>.cpp`.

**When to write tests:** write a test for any class/function that has pure or near-pure logic — parsers, data models, config helpers, utility functions. Do **not** try to test `QWidget`/QML views, `VpnFacade`, or `VpnService` (subprocess-dependent); those are integration-level and are not tested here.

**How to register a new test:**

1. Create `src/tests/tst_<unit>.cpp`.
2. Add it to `src/tests/CMakeLists.txt` using the existing `add_qt_test` macro, listing all the source files the unit depends on directly (no libraries beyond what `add_qt_test` provides by default):
   ```cmake
   add_qt_test(tst_myunit
       tst_myunit.cpp
       ../myunit.h
       ../myunit.cpp
   )
   ```
3. If the unit needs extra Qt modules (e.g. `Qt6::Gui`, `Qt6::Qml`), add them with a separate `target_link_libraries` call after `add_qt_test`.

**Test structure** — one `QObject` subclass per file, test slots in `private slots:`, `QTEST_MAIN` + `.moc` include at the bottom:

```cpp
#include <QtTest/QtTest>
#include "myunit.h"

class TstMyUnit : public QObject
{
    Q_OBJECT

private slots:
    void methodName_condition_expectedResult()
    {
        QCOMPARE(MyUnit::doThing("input"), QStringLiteral("expected"));
    }
};

QTEST_MAIN(TstMyUnit)
#include "tst_myunit.moc"
```

**Naming:** `methodName_condition_expectedResult` (e.g. `parseStatusFields_emptyInput_returnsEmptyMap`).

**Singletons in tests** (`AppConfig`, `ConnectionHistory`, `FavoritesManager`): call `QStandardPaths::setTestModeEnabled(true)` in `initTestCase()` and restore it in `cleanupTestCase()` to keep tests isolated from the real user config directory. Restore any values mutated during a slot at the end of that slot.

## Page Navigation (QML)

`Main.qml` hosts the view stack. The flow is: `LoadingView → NotInstalledView` (if CLI missing) or `LoginView` (if not authenticated) or `MainView` (main screen), with `SettingsView` and `AccountView` reachable from the main screen.

## Signals Pattern

`VpnService` emits typed signals; `VpnFacade` re-exposes them as QML properties/signals, and views bind to them. Views **never** call `protonvpn` directly — all actions go through `VpnFacade` → `VpnService`.
