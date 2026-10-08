# Plasma Mail — Plan (Milestone 1)

- **Goal**: A Plasma-native mail client (C++20, Qt 6, Kirigami, KDE PIM libraries) that can
  set up a Yahoo account with an app password, list its folders, show the newest 50 messages
  of a folder, and display a message safely (no remote content, no JavaScript), marking it read.
  Optionally it stores mail locally, limited by an import start date, a keep-for time limit
  and a size limit in MB/GB.
  Code is split Geary-style into an **engine** (no UI) and the **app** (QML UI).
- **Out of scope** (this milestone): Gmail/Outlook OAuth2, sending mail (SMTP), compose/reply,
  full offline mode (composing offline, offline flag changes), search, multiple accounts, Proton Mail (skipped entirely),
  notifications, IMAP IDLE, attachment opening/saving, .desktop file and packaging.
  Do not commit credentials anywhere; passwords live only in the system keychain or in
  environment variables for manual tests.
- **Global verify**:
  `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
- **Open questions**:
  - **Q1** (answered 2026-10-08: yes, ready; the user types it in themselves. **Corrected 2026-10-08:** in the first GUI run the user entered their web login password, which Yahoo refuses over IMAP; an app password still has to be generated before T24): Does the user have a Yahoo app password ready (Yahoo → Account security →
    2-step verification on → "Generate app password")? Needed only by T6 and T24, which are
    manual checks against the real server. Ask before starting those tasks.
  - **Q2** (answered: all except Trash and Junk): Which folders are stored locally? Proposal: all folders except Trash and Junk.
    Needed by T37.
  - **Q3** (answered: whole message): Are attachments stored too (whole message, counted in the size limit), or only
    the text, with attachments fetched from the server when needed? Proposal: whole message.
    Needed by T28 and T35.

## Later milestones (not planned yet, do not start)
2. Gmail via OAuth2 (Qt NetworkAuth, loopback + PKCE, XOAUTH2 SASL plugin `libkdexoauth2` is
   installed), Gmail extensions (X-GM-THRID, X-GM-LABELS, All Mail de-duplication).
3. Outlook/Microsoft via OAuth2 (reuses milestone 2 code).
4. Compose / reply / forward over SMTP (KPim6::SMTP is installed).
5. IDLE, notifications, multiple accounts, background sync timer.

## Build plan

### Toolchain (installed on this machine, CachyOS)
| Tool | Version | Used for |
|---|---|---|
| CMake | 4.4 | Build system (min. 3.22 in `CMakeLists.txt`) |
| Ninja | system | Generator (`-G Ninja`) |
| GCC | 16.2 | Default compiler, C++20 |
| Clang | 23.1 | Optional second compiler; `clang-format` for T44 |
| extra-cmake-modules (ECM) | 6.30 | KDE CMake settings, `ecm_add_test`, sanitizers, clang-format target |
| qt6-tools / `qmllint` | 6.11 | QML linting (`/usr/bin/qmllint`) |
| gdb | 18.1 | Debugging |
| clazy, valgrind | **not installed** | Optional. Use ASan/UBSan (T43 preset) instead of valgrind. `sudo pacman -S clazy` if wanted. |

### Libraries
| Package (Arch) | Version | CMake package → target | What we use it for | First task |
|---|---|---|---|---|
| qt6-base | 6.11 | `Qt6` → `Qt6::Core`, `Gui`, `Network`, `Sql`, `Test` | Core types, TLS sockets, SQLite index, unit tests | T1 |
| qt6-declarative | 6.11 | `Qt6::Qml`, `Quick`, `QuickControls2` | QML UI, `qt_add_qml_module` | T16 |
| qt6-webengine | 6.11 | `Qt6::WebEngineQuick` | Sandboxed HTML message view | T19/T23 |
| kirigami | 6.30 | `KF6::Kirigami` (QML import `org.kde.kirigami`) | Plasma-native window, pages, drawer, forms | T19 |
| kirigami-addons | 1.15 | QML import `org.kde.kirigamiaddons.formcard` (optional) | Date picker for "Import mail from" (T41), if plain controls are too clumsy | T41 |
| qqc2-desktop-style | 6.30 | runtime only | Native Breeze look for QtQuick Controls | T19 |
| ki18n | 6.30 | `KF6::I18n`, `KF6::I18nQml` | Translatable strings in C++ and QML | T10 |
| kcoreaddons | 6.30 | `KF6::CoreAddons` | `KAboutData`, `KJob` (base of KIMAP jobs), `KFormat` byte sizes | T16 |
| kconfig | 6.30 | `KF6::ConfigCore` | `plasmamailrc` account + storage settings | T1 |
| kmime | 6.30 | `KF6::Mime` | Parse messages, headers, body parts, attachments | T5 |
| kimap | 26.08 | `KPim6IMAP` → `KPim6::IMAP` | IMAP session, login, LIST, SELECT, SEARCH, FETCH, STORE | T5 |
| libsasl (cyrus) | 2.1.28 | runtime, via KIMAP | Login mechanisms (plain, login) | T5 |
| qtkeychain-qt6 | 0.17 | `Qt6Keychain` → `Qt6Keychain::Qt6Keychain` | Store the app password in KWallet / Secret Service | T14 |
| sqlite | 3.53 | runtime, via Qt `QSQLITE` driver | Local message index | T28 |
| ksmtp, kmailtransport | 26.08 | `KPim6SMTP` | **Not in milestone 1** (sending, milestone 4) | — |
| qt6-networkauth, libkgapi (`libkdexoauth2` SASL plugin) | — | `Qt6::NetworkAuth` | **Not in milestone 1** (Gmail/Outlook OAuth2, milestones 2–3) | — |

Not used on purpose: Akonadi (too heavy for a simple client), KMail's MessageViewer/MessageComposer
(widget-based, pulls in Akonadi), Gmail REST API (IMAP covers all providers with one code path).

### Architecture
```
 ┌───────────────────────── plasma-mail (executable) ─────────────────────────┐
 │  QML (src/qml): Main · AccountSetupPage · MessageListPage · MessagePage ·   │
 │                 StorageSettingsPage            ── Kirigami, WebEngineView    │
 │        ▲ properties / signals          │ Q_INVOKABLE calls                   │
 │  MailController (src/app, QML singleton) ── the only class QML talks to     │
 └────────┬───────────────────────────────┬────────────────────────────────────┘
          │ owns                          │ owns
 ┌────────▼──────── plasmamail_models ────┴───┐
 │ FolderModel · MessageListModel (QtCore)    │
 └────────┬───────────────────────────────────┘
          │ uses value types (FolderInfo, MessageSummary)
 ┌────────▼──────────────── plasmamail_engine (no UI) ──────────────────────────┐
 │ Account · StorageSettings  ── KConfig (plasmamailrc)                          │
 │ SecretStore                ── QtKeychain (KWallet / Secret Service)           │
 │ ImapClient                 ── KIMAP Session + jobs over TLS ── IMAP server    │
 │ folderutils · messagerenderer (KMime → safe HTML, attachment names)           │
 │ MessageStore               ── QtSql SQLite index + .eml files                 │
 │ retention · SyncManager    ── ImapClient + MessageStore + StorageSettings     │
 └───────────────────────────────────────────────────────────────────────────────┘
```
Rules: the engine never includes QML/Quick headers; models never talk to the network;
QML never calls the engine directly. Everything is asynchronous (signals), nothing blocks the
GUI thread except small SQLite queries.

### Main data flows
- **Startup**: `Account::load()` → `SecretStore::readPassword` → `ImapClient::setAccount` →
  `listFolders` → `FolderModel` → auto-select Inbox → `fetchHeaders(Inbox, 50)` →
  `MessageListModel`. If storage is on: list from `MessageStore` first, then `SyncManager::syncAll`.
- **Open message**: local `.eml` from `MessageStore` if present, else `ImapClient::fetchMessage`
  → `messageBodyHtml` + `attachmentNames` → controller properties → `MessagePage`
  (`WebEngineView.loadHtml`). If unread: `markSeen` on server, model and store.
- **Sync** (per folder): SELECT (check UIDVALIDITY) → `SEARCH SINCE <cutoff>` → skip stored UIDs
  → fetch newest first in batches of 20 within the size budget → `MessageStore::addMessage`
  → `applyRetention`.

### Directory layout (end of milestone 1)
```
CMakeLists.txt  CMakePresets.json  .clang-format  PLAN.md
src/CMakeLists.txt
src/engine/   account · storagesettings · secretstore · folderutils · imapclient
              messagerenderer · messagestore · retention · syncmanager
src/models/   foldermodel · messagelistmodel
src/app/      main.cpp · mailcontroller
src/qml/      Main · AccountSetupPage · MessageListPage · MessagePage · StorageSettingsPage
autotests/    one <name>test.cpp per engine/model unit
tools/        imapprobe.cpp (manual IMAP checks, not installed)
```

### Build, run, test
```fish
# configure
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build                         # everything
cmake --build build --target plasmamail_engine
ctest --test-dir build --output-on-failure  # all tests; -R <name> for one
build/bin/plasma-mail                       # run the app
qmllint -I build/src src/qml/*.qml          # lint QML (after T19; T19 notes the exact import path)

# run against a throw-away profile (doesn't touch your real config/mail)
env XDG_CONFIG_HOME=(mktemp -d) XDG_DATA_HOME=(mktemp -d) build/bin/plasma-mail

# dev: save a screenshot of the app window (not the screen) after 6 s and quit (T48);
# PLASMAMAIL_SCREENSHOT_OPEN=1 opens the newest message first
env PLASMAMAIL_SCREENSHOT=/tmp/shot.png build/bin/plasma-mail

# protocol debugging (prints IMAP traffic; never paste its output publicly: it has your mail)
env QT_LOGGING_RULES="org.kde.pim.kimap*=true" build/bin/plasma-mail

# sanitizer build (after T43)
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
```

### Where data lives at runtime
| What | Where |
|---|---|
| Account + storage settings | `~/.config/plasmamailrc` |
| App password | system keychain, service `plasma-mail`, key = email address |
| Local mail index | `~/.local/share/plasma-mail/mail.sqlite` |
| Stored messages | `~/.local/share/plasma-mail/messages/…/*.eml` |
| Test runs | Qt test-mode paths (`~/.qttest/…`) — never the real ones |

### Security rules (apply to every task)
- Only encrypted connections: implicit TLS (993/465) or STARTTLS. Never `Unencrypted`.
  Certificate errors fail the connection (no "ignore" button in milestone 1).
- Passwords: never logged, never written to config, never in command-line arguments.
- HTML mail: JavaScript off, remote images off, CSP meta tag, links open in the external
  browser, `loadHtml` with base URL `about:blank`.
- The app never deletes or moves mail on the server in milestone 1 (only `\Seen` is set).

### Execution order (waves)
Tasks in the same wave can run in parallel once the earlier waves are done.
| Wave | Tasks |
|---|---|
| 0 | T1 ✔, T2 ✔ |
| 1 | T3, T7, T14, T26, T43, T44 |
| 2 | T4, T5, T8, T10, T15, T27 |
| 3 | T6, T9, T11, T12, T16, T28, T34 |
| 4 | T13, T17, T19, T29, T35 |
| 5 | T18, T20, T21, T22, T30 |
| 6 | T23, T31 |
| 7 | T24 (manual), T32 |
| 8 | T33, T36 |
| 9 | T37 |
| 10 | T38 (manual), T39 |
| 11 | T40, T41 |
| 12 | T42 (manual) |
| 13 | T25 (global verify) |

GUI-first reordering (2026-10-08, user's call): T26 → T27 → T39a → T41 run before the rest of
the storage work, so the storage page exists early with usage/cleanup shown but disabled.
T39 and T46 later make it fully functional.

## Conventions (read before any task)
- Build dir is `build/` (git-ignored). Configure once with
  `cmake -S . -B build -G Ninja`; later builds just `cmake --build build`.
- `KDECompilerSettings` is strict: `QT_NO_CAST_FROM_ASCII`, `QT_NO_KEYWORDS`. Use
  `using namespace Qt::StringLiterals;` with `u"..."_s` / `"..."_L1`, and
  `Q_SIGNALS` / `Q_EMIT` / `Q_SLOTS`.
- Three targets: `plasmamail_engine` (static lib, `src/engine/`), `plasmamail_models`
  (static lib, `src/models/`, QtCore only, no QML macros), `plasma-mail` (executable,
  `src/app/` + `src/qml/`, QML module URI `PlasmaMail`).
- Adding a new source file to the matching `CMakeLists.txt` does not count toward a task's
  1–2 file limit, and a `.h`/`.cpp` pair counts as one file.
- Every source, CMake and QML file starts with an SPDX header (`//` comments in C++/QML,
  `#` in CMake):
  `SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>` and
  `SPDX-License-Identifier: GPL-2.0-or-later`. The licence text is in
  `LICENSES/GPL-2.0-or-later.txt`.
- Build only in `build/` (or `build-*/` presets). Never run `cmake .` in the source tree.
- Include project headers by file name (`"folderutils.h"`), not `"engine/…"`.
- Tests live in `autotests/`, use QTest, are added with ECM's `ecm_add_test`, and must call
  `QStandardPaths::setTestModeEnabled(true)` before touching config.
- Installed library facts (verified on this machine, Qt 6.11, KF 6.30, KPim 26.08):
  - KMime is `KF6::Mime`, headers `<KMime/Message>`; messages are `std::shared_ptr<KMime::Message>`;
    header getters take `KMime::CreatePolicy` (`DontCreate`/`Create`); const getters return
    `nullptr` when absent. Useful: `mainBodyPart("text/html")`, `decodedText()`,
    `Content::attachments()`, `contentDisposition()->filename()`, `contentType()->name()`.
  - KIMAP is `KPim6::IMAP`, headers `<KIMAP/LoginJob>` etc. Jobs are KJobs queued per
    `KIMAP::Session`. List folders with `ListJob::IncludeUnsubscribed` (plain LIST, which
    carries RFC 6154 flags like `\Sent`); never `IncludeFolderRoleFlags`: it sends Gmail-only
    `XLIST`, which Yahoo rejects (see T6).
    `FetchJob::FetchScope::Headers` fetches flags + UID + common headers;
    `FetchScope::Full` fetches the whole message.
  - QtKeychain: `find_package(Qt6Keychain)`, target `Qt6Keychain::Qt6Keychain`,
    header `<qt6keychain/keychain.h>`.
  - QML i18n: `KLocalization::setupLocalizedContext(&engine)` from `<KLocalizedQmlContext>`,
    then `KI18n.i18nc("@context", "text")` in QML. Every QML file using it needs
    `import org.kde.ki18n` (without it `KI18n` is undefined at runtime; qmllint doesn't catch
    it). Needs `TRANSLATION_DOMAIN` defined (done in root CMake).
  - QtWebEngine in QML needs `QtWebEngineQuick::initialize()` before the `QGuiApplication`
    is constructed.
  - Qt logs to the systemd journal, not stderr, when stderr isn't a terminal. Offscreen
    "no QML warnings" checks must set `QT_FORCE_STDERR_LOGGING=1` or they see nothing.
  - Kirigami prints "QML <Page>: Created graphical object was not placed in the graphics scene"
    for every page pushed as a `Component` (PageRow creates it with a non-Item parent, then
    reparents it). Harmless; ignore it in the offscreen checks.
- Commit after each finished task: `T<n>: <title>`.
- Local storage decisions:
  - Off by default. Settings per account, stored in `plasmamailrc` group `Storage`.
  - Settings: enabled; import start date (or "all mail"); keep-for = number + unit
    (days/weeks/months/years) or "forever"; size limit = number + unit (MB/GB) or "unlimited".
    1 MB = 1024×1024 bytes, 1 GB = 1024 MB.
  - Defaults when turned on: start date = 1 year ago, keep for 1 year, limit 1 GB.
  - Effective cutoff = the later of (start date, now − keep-for), compared with the message's
    `Date` header (falls back to IMAP INTERNALDATE).
  - Limits only remove **local copies**. Nothing is ever deleted on the server.
  - When over the size limit, the oldest messages are removed first. Sync downloads newest
    first and stops when the next message would exceed the limit.
  - Storage layout: SQLite index (QtSql, `QSQLITE`) at
    `QStandardPaths::AppDataLocation/mail.sqlite` + one raw `.eml` file per message under
    `AppDataLocation/messages/`. A folder's local copies are dropped if its UIDVALIDITY changes.
  - Messages older than the cutoff stay readable online; they're just not stored.
- Decision: when a message has both HTML and plain text, show HTML (like Geary). Remote
  content is blocked by a Content-Security-Policy `<meta>` plus WebEngine settings.

---

## Tasks

### T1 — CMake skeleton that builds the engine library with `Account`
- status: done
- depends on: none
- parallel-safe with: none
- files: CMakeLists.txt, src/CMakeLists.txt
- inputs: drafts already exist in both files and in src/engine/account.{h,cpp}. The draft
  src/CMakeLists.txt references files that don't exist yet — reduce it to only
  `plasmamail_engine` built from `engine/account.cpp`, linking Qt6::Core, KF6::ConfigCore.
  Root CMake: keep find_package calls; add `include(ECMAddTests)` and
  `if(BUILD_TESTING) add_subdirectory(autotests) endif()` guarded by
  `if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/autotests/CMakeLists.txt)` so it configures before T2.
- output: `plasmamail_engine` builds.
- verify: `cmake -S . -B build -G Ninja && cmake --build build --target plasmamail_engine`
  succeeds with no warnings from our sources.
- notes: Done. src/CMakeLists.txt now builds only `plasmamail_engine` (account.cpp, Qt6::Core,
  KF6::ConfigCore); later tasks add their sources and link libs (KF6::Mime, KPim6::IMAP,
  Qt6::Network, Qt6Keychain, Qt6::Sql) there. Root CMake also finds Qt6 Sql and Test, includes
  ECMAddTests and adds `autotests/` only if its CMakeLists.txt exists. Configure prints a
  harmless "No such remote 'origin'" from ECM's git checks (no remote set). Drafts in
  src/engine/imapclient.* and src/app/*model.* are still untracked and unbuilt.

### T2 — Test harness and `Account` unit test
- status: done
- depends on: T1
- parallel-safe with: T3, T7, T14
- files: autotests/CMakeLists.txt, autotests/accounttest.cpp
- inputs: src/engine/account.h (presets: yahoo/gmail/outlook; `isValid`; `load/save/remove`
  using `plasmamailrc` group `Account`).
- output: `accounttest` checks: each preset fills correct hosts/ports, unknown provider returns
  false, `isValid` rules, save→load round trip, `remove()` clears it. Uses test mode paths.
- verify: `cmake --build build && ctest --test-dir build -R accounttest --output-on-failure`
- notes: Committed in c6da4f4 by another session. Re-verified 2026-10-08 after the file had been
  deleted from the working tree (restored with `git restore`). `applyPreset` now clears host/port
  fields first, so an unknown provider (incl. "custom") leaves them empty: callers must set custom
  hosts *after* calling it.

### T3 — Folder helpers: `FolderInfo`, role detection, sorting
- status: done
- depends on: T1
- parallel-safe with: T2, T7, T14
- files: src/engine/folderutils.h, src/engine/folderutils.cpp
- inputs: move `FolderInfo` struct and the static `roleFromFlags` / `roleOrder` logic out of
  the draft src/engine/imapclient.{h,cpp} (do not edit imapclient files here; T5 does).
- output: `struct FolderInfo {path, name, role, depth}`;
  `QString folderRoleFromFlags(const QString &path, const QList<QByteArray> &flags)`
  (INBOX → "inbox", special-use flags case-insensitive → sent/drafts/trash/junk/archive/all/flagged);
  `void sortFolders(QList<FolderInfo> &)` (role order inbox, flagged, drafts, sent, archive,
  all, junk, trash, then others by path case-insensitively).
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Committed in d745178 (that commit also contained the IMAP client, see T5). The
  `folderutils.cpp` file was missing from src/CMakeLists.txt in that commit; added during the
  2026-10-08 cleanup. Verified with `cmake --build build --target plasmamail_engine`.

### T4 — Unit test for folder helpers
- status: done
- depends on: T2, T3
- parallel-safe with: T5, T8, T10, T15
- files: autotests/folderutilstest.cpp
- inputs: src/engine/folderutils.h
- output: data-driven tests for role detection (incl. `inbox` lowercase, `\Sent` vs `\SENT`,
  no flags) and sort order.
- verify: `ctest --test-dir build -R folderutilstest --output-on-failure`
- notes: Done 2026-10-08. Covers every special-use role, case-insensitive flags
  (`\Sent`, `\SENT`, `\DRAFTS`), lowercase "inbox" path, unknown/no flags, and sort order incl.
  case-insensitive names (apple before Banana). The test file defines operator== for
  FolderInfo because QCOMPARE on QList<FolderInfo> needs it.

### T5 — `ImapClient` compiles against folder helpers
- status: done
- depends on: T3
- parallel-safe with: T4, T8, T10, T15
- files: src/engine/imapclient.h, src/engine/imapclient.cpp
- inputs: drafts exist. Replace local `FolderInfo`/role helpers with `folderutils.h`.
  Keep `MessageSummary {uid, subject, from, date, unread}` in imapclient.h. Add to engine lib;
  link KF6::Mime, KPim6::IMAP, Qt6::Network.
- output: `ImapClient` with `setAccount(Account, password)`, `listFolders()`,
  `fetchHeaders(folder, limit)`, `fetchMessage(folder, uid)`, `markSeen(folder, uid)`,
  `disconnectFromServer()` and signals `foldersListed`, `headersFetched`, `messageFetched`,
  `errorOccurred`. Review the draft against the KIMAP facts above; don't assume it is correct.
- verify: `cmake --build build --target plasmamail_engine` succeeds with no warnings from our
  sources.
- notes: Code arrived in the T3 commit (d745178); wired into src/CMakeLists.txt with KF6::Mime,
  KPim6::IMAP, Qt6::Network (Qt6Keychain and Qt6::Sql are linked early for T14/T28). Builds with
  no warnings. Known issue split out as T45 (one error per queued job after a failed login).

### T6 — `imap-probe` command-line tool for manual engine checks
- status: done
- depends on: T5, Q1
- parallel-safe with: everything after T5 except T24
- files: tools/imapprobe.cpp, tools/CMakeLists.txt (+ `add_subdirectory(tools)` in root CMake)
- inputs: `ImapClient`, `Account::applyPreset`. Reads `PLASMAMAIL_PROVIDER` (default yahoo),
  `PLASMAMAIL_EMAIL`, `PLASMAMAIL_PASSWORD` from the environment; never prints the password.
- output: prints the folder list, the newest 10 INBOX headers, then the subject and size of the
  newest message; exits 0 on success, 1 on any `errorOccurred`, with a 60 s timeout.
- verify: (manual, needs Q1) `PLASMAMAIL_EMAIL=… PLASMAMAIL_PASSWORD=… build/bin/imap-probe`
  prints folders incl. Inbox/Sent and exits 0; with a wrong password it prints a login error
  and exits 1.
- notes: Done 2026-10-08, verified against the user's Yahoo account (app password):
  6 folders with correct roles (INBOX, Draft, Sent, Archive, Bulk=junk, Trash), newest 10 INBOX
  headers, newest message fetched (18 KB), exit 0; the fetched unread message stays unread
  (BODY.PEEK). Wrong password → one `Invalid credentials [AUTHENTICATIONFAILED]` line, exit 1.
  If `PLASMAMAIL_PASSWORD` is unset the tool asks on the terminal without echo (keeps the
  password out of shell history). **Engine bug found and fixed here:** `ListJob::
  IncludeFolderRoleFlags` makes KIMAP send `XLIST` (Gmail-only); Yahoo answers `BAD … CLIENTBUG`.
  `ImapClient::listFolders` now uses `IncludeUnsubscribed` (plain `LIST`); Yahoo returns the
  RFC 6154 flags there even without advertising SPECIAL-USE. Yahoo has no `SPECIAL-USE`/`XLIST`
  capability. Protocol debugging: `KIMAP_LOGFILE=<path>` writes the raw session — it contains
  the LOGIN line with the password, delete it afterwards.

### T7 — Message body to safe HTML
- status: done
- depends on: T1
- parallel-safe with: T2, T3, T14
- files: src/engine/messagerenderer.h, src/engine/messagerenderer.cpp
- inputs: KMime facts above.
- output: `QString messageBodyHtml(const KMime::Message &msg, const QColor &text, const QColor &background)`:
  uses `text/html` part if present, else `text/plain` escaped with `toHtmlEscaped()`, wrapped
  in `white-space: pre-wrap`, with http(s) URLs turned into links; always prepends
  `<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; img-src data: cid:">`.
  Colors apply only to the plain-text wrapper. Add file to engine lib (link Qt6::Gui for QColor).
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Done 2026-10-08. `messageBodyHtml()` in src/engine/messagerenderer.{h,cpp}; the CSP
  string is the public constant `messageContentSecurityPolicy`. HTML part: CSP meta + the
  decoded HTML as-is. Plain text: full HTML document with the CSP meta, colors on `body`,
  `pre-wrap` div, escaped text, URLs linked (a trailing `.,;:!?)]` is not part of the link; the
  link stops at escaped quotes/brackets). A missing Content-Type is treated as text/plain;
  other cases fall back to `Message::textContent()`. Engine now links Qt6::Gui.
  **Gotcha for T8/T28/T35:** KMime only splits headers from body on LF line endings. Build
  test messages with `\n`, and convert raw CRLF data with `KMime::CRLFtoLF()` before
  `setContent()` (KIMAP already does this for fetched messages). A smoke run (not committed)
  confirmed: script escaping, URL linking, multipart/alternative → HTML, QP + ISO-8859-1 decoding.

### T8 — Unit test for `messageBodyHtml`
- status: done
- depends on: T2, T7
- parallel-safe with: T4, T5, T10, T15
- files: autotests/messagerenderertest.cpp
- inputs: src/engine/messagerenderer.h; build test messages from raw RFC 822 strings via
  `KMime::Message::setContent()` + `parse()`.
- output: tests: plain-text only (escapes `<script>`, links a URL), multipart/alternative
  picks HTML, CSP meta always present, quoted-printable + non-UTF-8 charset decoded.
- verify: `ctest --test-dir build -R messagerenderertest --output-on-failure`
- notes: Done 2026-10-08. 27 checks: CSP present and before the body for plain, HTML,
  no Content-Type, empty body and attachment-only mails; policy has `default-src 'none'` and no
  script/http sources; markup escaped in plain text; data-driven URL linking (query `&`,
  trailing dot, parentheses, quotes, angle brackets, two links, no link for `www.` or
  `javascript:`); colors; missing Content-Type = plain; multipart/alternative and
  mixed+alternative pick HTML; an attached .html file does not replace the plain body;
  QP ISO-8859-1 incl. soft line break, base64 windows-1252 HTML, UTF-8 Cyrillic.

### T9 — Attachment names from a message
- status: done
- depends on: T8
- parallel-safe with: T10–T15
- files: src/engine/messagerenderer.{h,cpp}, autotests/messagerenderertest.cpp
- inputs: `Content::attachments()`; name from `contentDisposition()->filename()`, falling back
  to `contentType()->name()`, then a translated "Unnamed attachment".
- output: `QStringList attachmentNames(KMime::Message &msg)` plus a test with one named and one
  unnamed attachment.
- verify: `ctest --test-dir build -R messagerenderertest --output-on-failure`
- notes: Done 2026-10-08. Signature takes `const KMime::Message &` (KMime has a const
  `attachments()`), which also accepts non-const messages. Engine library now links KF6::I18n
  for the "Unnamed attachment" text. Tests (9 rows in messagerenderertest): no attachments,
  disposition filename, Content-Type name only, filename wins over name, unnamed, named +
  unnamed in order, RFC 2231 `filename*=` (UTF-8 Cyrillic), RFC 2047 encoded name, and
  multipart/alternative bodies not counted as attachments.

### T10 — `FolderModel` in the models library
- status: done
- depends on: T3
- parallel-safe with: T4, T5, T8, T14, T15
- files: src/models/foldermodel.h, src/models/foldermodel.cpp
- inputs: draft at src/app/foldermodel.{h,cpp} — move it (git mv not needed, nothing is
  committed), drop `QML_ELEMENT`/`QML_UNCREATABLE` and `<QQmlEngine>`, include
  `folderutils.h` instead of imapclient.h. Create `plasmamail_models` static lib in
  src/CMakeLists.txt linking `plasmamail_engine` and KF6::I18n.
- output: list model with roles `path, name, folderRole, depth, iconName`;
  `setFolders()`, `displayName(path)`, `inboxPath()`.
- verify: `cmake --build build --target plasmamail_models` succeeds.
- notes: Done 2026-10-08 after review. Fixed: translated "Inbox" name (also for
  `Qt::DisplayRole`), `displayName()` falls back to the path, Breeze icon names that exist
  (inbox `mail-folder-inbox`, sent `mail-folder-sent`, drafts `document-edit`, trash
  `user-trash`, junk `mail-mark-junk`, archive/all `mail-folder-outbox`, flagged
  `mail-mark-important`, other `folder-mail`; also served for `Qt::DecorationRole`), include
  `"folderutils.h"` by name, no `Q_INVOKABLE`s. The models library contains only FolderModel
  until T12 adds MessageListModel; `messagelistmodeltest` therefore doesn't link until T12/T13.

### T11 — Unit test for `FolderModel`
- status: done
- depends on: T2, T10
- parallel-safe with: T12–T15
- files: autotests/foldermodeltest.cpp
- inputs: src/models/foldermodel.h
- output: test runs `QAbstractItemModelTester` while calling `setFolders`, checks role values,
  `inboxPath()` (inbox role, fallback to first, empty model → empty string), `displayName()`.
- verify: `ctest --test-dir build -R foldermodeltest --output-on-failure`
- notes: Done 2026-10-08. Rewritten from the earlier draft (it used the forbidden
  `slots` keyword and skipped `QAbstractItemModelTester`). Covers consistency via the tester,
  all roles and role names, every icon name (data-driven), translated "Inbox", displayName
  fallback, inboxPath fallbacks. Uses `QTEST_GUILESS_MAIN`. The `messagelistmodeltest`
  registration was taken out of autotests/CMakeLists.txt until T13.

### T12 — `MessageListModel` in the models library
- status: done
- depends on: T5, T10
- parallel-safe with: T11, T14, T15
- files: src/models/messagelistmodel.h, src/models/messagelistmodel.cpp
- inputs: draft at src/app/messagelistmodel.{h,cpp} — move, drop QML macros. Uses
  `MessageSummary` from imapclient.h.
- output: roles `uid, subject, from, date, dateText, unread`; `setMessages`, `clear`,
  `setUnread(uid, bool)` (emits `dataChanged` for that row only), `message(uid)`.
  Remove the leftover draft files in src/app/.
- verify: `cmake --build build --target plasmamail_models` succeeds.
- notes: Done 2026-10-08 after review. Fixed: dates are converted to local time before the
  "today" check (sender time zones), formatted with `QLocale` short formats, "(No subject)" is
  translatable, `Qt::DisplayRole` = subject, no `Q_INVOKABLE`s, include `"imapclient.h"` by name.
  Added `static QString dateText(const QDateTime &, const QDate &today)` so tests can pin "today".
  `setUnread` emits `dataChanged` only when the value actually changes. src/app/ drafts were
  already gone.

### T13 — Unit test for `MessageListModel`
- status: done
- depends on: T2, T12
- parallel-safe with: T14–T16
- files: autotests/messagelistmodeltest.cpp
- inputs: src/models/messagelistmodel.h
- output: `QAbstractItemModelTester`; empty subject → "(No subject)"; `setUnread` emits one
  `dataChanged` with `UnreadRole`; `message(unknownUid)` returns uid 0.
- verify: `ctest --test-dir build -R messagelistmodeltest --output-on-failure`
- notes: Done 2026-10-08. Rewritten from the earlier draft (forbidden `slots` keyword, and a
  date check that only passed on 1 October). Covers the model tester, all roles, "(No subject)",
  `dateText` for today/other days/invalid with a pinned "today", local-time conversion across
  time zones, exactly one `dataChanged` with `UnreadRole` (none for unchanged/unknown uid),
  lookup and clear. Expected strings use `QLocale`, so the test passes in any locale.

### T14 — `SecretStore`: async keychain wrapper
- status: done
- depends on: T1
- parallel-safe with: T2, T3, T7
- files: src/engine/secretstore.h, src/engine/secretstore.cpp
- inputs: QtKeychain facts above; service name `plasma-mail` (`AccountSecrets::service` in
  account.h), key = account email.
- output: `SecretStore` QObject with `writePassword(key, password)`, `readPassword(key)`,
  `deletePassword(key)` and signals `passwordWritten(key, error)`, `passwordRead(key, password, error)`,
  `passwordDeleted(key, error)` (empty error = success). Link Qt6Keychain into engine lib.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Done 2026-10-08. `SecretStore(const QString &service, QObject *parent)`: the service
  name is a constructor argument so T15 can use a separate test service; the app passes
  `QString::fromLatin1(AccountSecrets::service)`. Every request emits exactly one signal; empty
  error = success, and a failure always has non-empty text. `deletePassword` treats
  `EntryNotFound` as success (removing an account twice is fine); `readPassword` of a missing
  key reports an error and an empty password. Insecure plain-file fallback is explicitly off.
  Qt6Keychain was already linked into the engine.

### T15 — Test for `SecretStore` against the session keychain
- status: done
- depends on: T2, T14
- parallel-safe with: T4, T5, T8, T10–T13
- files: autotests/secretstoretest.cpp
- inputs: src/engine/secretstore.h
- output: write → read → delete → read-fails round trip with key `plasma-mail-autotest`,
  using `QSignalSpy::wait`. `QSKIP` if the keychain reports "no backend available".
- verify: `ctest --test-dir build -R secretstoretest --output-on-failure` (run inside the
  Plasma session so KWallet/Secret Service is reachable).
- notes: Done 2026-10-08. Uses service *and* key `plasma-mail-autotest`, so it never touches
  the app's real entries; `cleanupTestCase` deletes the test entry even after a failure. Covers
  round trip with a non-ASCII/emoji/quoted password, read-after-delete error, overwrite, and
  deleting a missing key twice. Skips (QSKIP) when `QKeychain::isAvailable()` is false.
  Note: ctest reports a skipped test as "Passed"; run `build/bin/secretstoretest` to see
  per-test PASS/SKIP. Verified in the Plasma session (kwalletd6 + ksecretd): 6 passed,
  0 skipped, ~30 ms, no permission prompt. 60 s wait per request covers a first-time prompt.

### T16 — `MailController`: account setup and folder listing
- status: done
- depends on: T5, T10, T14
- parallel-safe with: T11, T13, T15
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: `Account`, `SecretStore`, `ImapClient`, `FolderModel`.
- output: `QML_ELEMENT QML_SINGLETON` class; properties `hasAccount`, `accountEmail`, `busy`,
  `folderModel` (type `QAbstractListModel*`, CONSTANT); signal `errorOccurred(QString)`;
  invokables `setupAccount(provider, email, displayName, password, imapHost, imapPort, smtpHost, smtpPort)`
  (custom uses host args; saves config, writes password, then connects) and `removeAccount()`.
  On construction: load account; if valid, read password and connect. Connecting =
  `ImapClient::setAccount` + `listFolders`. `busy` counts in-flight requests.
  Create the `plasma-mail` app target here only far enough to compile this file
  (`qt_add_qml_module` with this source, no QML files yet, a temporary trivial
  `src/app/main.cpp` returning 0 is allowed and replaced in T19).
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Done 2026-10-08. **Deviation (better UX, like Geary):** `setupAccount` first logs in
  and lists folders with the new credentials; only on success does it write the password to
  the keychain, save the config and emit `accountChanged`. A wrong password saves nothing and
  emits one `errorOccurred`; an earlier account is reconnected. Setting up a different email
  deletes the old one's keychain entry. Startup: `connectSavedAccount()` reads the password and
  connects. `busy` is a request counter, reset on fatal errors. User-facing texts use i18n;
  engine error texts are passed through untranslated. App target: `qt_add_qml_module`
  (URI PlasmaMail, QTP0001 NEW), `src/app` is on the include path because the generated type
  registration includes `<mailcontroller.h>` by name; `src/app/main.cpp` is a stub returning 0
  until T19. Verified beyond the build check with a throwaway harness against Yahoo (test-mode
  config, real keychain): wrong → right password, restart reconnects from the keychain with
  6 folders, `removeAccount` clears config and keychain.

### T17 — `MailController`: folder selection and message list
- status: done
- depends on: T12, T16
- parallel-safe with: T13, T15
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: `MessageListModel`, `ImapClient::fetchHeaders`.
- output: properties `messageModel` (`QAbstractListModel*`), `currentFolder`,
  `currentFolderName`; invokables `selectFolder(path)` (clears list, fetches newest 50) and
  `refresh()`. After the first folder list arrives, auto-select `inboxPath()`. Ignore
  `headersFetched` for a folder that is no longer current.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Done 2026-10-08. `MailController::messageLimit = 50`. `selectFolder` clears the list
  only when the folder changes (re-selecting the same folder keeps it while reloading).
  `refresh()` reloads the folder list and the current folder. After every folder list: if the
  current folder no longer exists (or none yet), the inbox is selected; otherwise
  `currentFolderChanged` is re-emitted so the translated name updates. `headersFetched` for a
  non-current folder only ends the busy request. `removeAccount` clears folder and messages.
  Verified with the scratch harness on Yahoo: inbox auto-selected (50 rows, name "Inbox"),
  Sent 41 rows, Trash→INBOX quick switch ends on INBOX, refresh keeps 6 folders/50 rows.

### T18 — `MailController`: open a message
- status: done
- depends on: T9, T17
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: `ImapClient::fetchMessage/markSeen`, `messageBodyHtml`, `attachmentNames`.
- output: properties `currentUid`, `messageSubject`, `messageFrom`, `messageTo`, `messageDate`,
  `messageHtml`, `messageAttachments` (one `currentMessageChanged` signal); invokable
  `openMessage(uid)`: fetch, render with `QGuiApplication::palette()` colors, ignore results
  for a uid that is no longer current, and if the message was unread call `markSeen` and
  `MessageListModel::setUnread(uid, false)`.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Done 2026-10-08. Opening a message clears the previous one's properties at once, so
  the view never shows a stale body; changing folder or removing the account closes it.

### T19 — App entry point and main window
- status: done
- depends on: T16
- parallel-safe with: T17, T18
- files: src/app/main.cpp, src/qml/Main.qml
- inputs: conventions above (WebEngine init, localized context, `KAboutData` with
  component name `plasma-mail`, `QQuickStyle` fallback to `org.kde.desktop`).
- output: `Kirigami.ApplicationWindow` titled "Plasma Mail"; shows an empty placeholder page
  for now; `MailController.errorOccurred` shows `showPassiveNotification`.
- verify: `cmake --build build && QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=offscreen timeout 5 build/bin/plasma-mail; test $status -eq 124`
  (still running after 5 s = no startup crash) and no `qrc:` / `QML` warnings in the output.
- notes: Done 2026-10-08. Load with `engine.loadFromModule("PlasmaMail", "Main")` (resource
  path is `qrc:/qt/qml/PlasmaMail/qml/Main.qml`); exits 1 if the QML fails to load. QML strings
  use `KI18n.i18nc` (links `KF6::I18nQml`). `QTP0004 NEW` set because QML lives in `qml/`.
  `qmllint -I build/src src/qml/*.qml` works as written in the commands section.

### T20 — Account setup page
- status: done
- depends on: T19
- parallel-safe with: T21, T22
- files: src/qml/AccountSetupPage.qml, src/qml/Main.qml
- inputs: `MailController.setupAccount`, `hasAccount`.
- output: `Kirigami.ScrollablePage` with `Kirigami.FormLayout`: provider combo (Yahoo,
  Custom IMAP; Gmail/Outlook shown disabled with "coming in a later version"), email,
  display name, password (echo mode password, hint linking to Yahoo app-password help for
  Yahoo), host/port fields only for Custom, "Connect" button. Main.qml shows this page when
  `!MailController.hasAccount`.
- verify: offscreen launch as in T19 with a clean config (`XDG_CONFIG_HOME` set to a fresh
  temp dir) prints no QML warnings; manual: the page renders and the button is disabled until
  email and password are filled.
- notes: Done 2026-10-08. Main.qml clears the page stack and pushes the setup page or a
  placeholder mail page on start and on every `accountChanged` (T21/T22 replace the placeholder).
  Connect also needs a host for Custom and is disabled while `busy`. Port SpinBox has no
  thousands separator. Yahoo hint links to https://help.yahoo.com/kb/SLN15241.html.

### T21 — Folder list in the global drawer
- status: done
- depends on: T17, T19
- parallel-safe with: T20, T22
- files: src/qml/Main.qml
- inputs: `MailController.folderModel` roles, `selectFolder`, `currentFolder`, `removeAccount`.
- output: `Kirigami.GlobalDrawer` (modal on narrow windows) listing folders with icon,
  indentation by `depth`, highlight of the current folder; footer action "Remove account".
- verify: offscreen launch as in T19 prints no QML warnings.
- notes: Done 2026-10-08. Drawer is disabled without an account, docked open on wide windows
  and closes after picking a folder when modal. "Remove Account" asks first (PromptDialog).
  Verified offscreen twice: clean config, and a fake custom account (`127.0.0.1:1`, run with
  `DBUS_SESSION_BUS_ADDRESS=disabled:` so the real wallet is never touched) so the drawer loads.

### T22 — Message list page
- status: done
- depends on: T17, T19
- parallel-safe with: T20, T21
- files: src/qml/MessageListPage.qml, src/qml/Main.qml
- inputs: `MailController.messageModel` roles, `currentFolderName`, `busy`, `refresh`,
  `openMessage`.
- output: `Kirigami.ScrollablePage` titled with the folder name; delegates show sender, subject,
  date, bold when unread; refresh action (F5); busy indicator; placeholder message when empty.
  Clicking a row calls `openMessage(uid)` and pushes the message page (T23; until then, no push).
- verify: offscreen launch as in T19 prints no QML warnings.
- notes: Done 2026-10-08. Busy shows as an indeterminate progress bar in the page header; the
  refresh action is disabled meanwhile. The open message (`currentUid`) is highlighted. Main.qml
  now pushes this page when an account exists. Empty subject shows "(no subject)".

### T23 — Message page with sandboxed HTML view
- status: done
- depends on: T18, T22
- parallel-safe with: T20, T21
- files: src/qml/MessagePage.qml, src/qml/MessageListPage.qml
- inputs: `MailController` message properties.
- output: page with subject, from, to, date, attachment names (chips, not clickable), and a
  `WebEngineView` loading `messageHtml` via `loadHtml(html, "about:blank")` with
  `settings.javascriptEnabled: false`, `settings.autoLoadImages: false`,
  `settings.localContentCanAccessRemoteUrls: false`; clicked links open with
  `Qt.openUrlExternally` and the navigation is rejected. MessageListPage pushes it on click.
- verify: offscreen launch as in T19 prints no QML warnings.
- notes: Done 2026-10-08. Navigations other than our own `about:`/`data:` load are rejected;
  link clicks and new-window requests open in the browser. Also sets
  `localContentCanAccessFileUrls: false`. Clicking another message reuses the open message
  page; changing folder pops it. Verified offscreen by temporarily pushing MessagePage at
  startup: no QML warnings (only Chromium's Vulkan/GPUInfo lines, from the offscreen platform).
  Link handling and image blocking are not checked offscreen: covered by T24.

### T24 — Manual end-to-end check with Yahoo
- status: todo
- depends on: T20, T21, T22, T23, Q1
- parallel-safe with: none
- files: none (record results in notes; file new tasks for any bug found)
- inputs: the user's Yahoo address and app password, entered in the app by the user.
- output: confirmed behaviour list in notes.
- verify: (manual, with the user) add account → Inbox opens automatically with newest messages
  → other folders open from the drawer → an HTML newsletter shows without remote images →
  an opened unread message becomes non-bold and stays read after restart → restart reconnects
  without asking for the password → "Remove account" returns to the setup page.
- notes: Partial 2026-10-08: with a new app password, add account → Inbox opens automatically
  with newest messages (folders: Inbox, Draft, Sent, Archive, Bulk, Trash). Rest still to check.

### T26 — `StorageSettings`: values, load/save, cutoff and byte limit
- status: done
- depends on: T1
- parallel-safe with: T2, T3, T7, T14
- files: src/engine/storagesettings.h, src/engine/storagesettings.cpp
- inputs: "Local storage decisions" in Conventions.
- output: struct with `enabled`, `QDate importSince` (null = all mail), `int keepValue`,
  `enum KeepUnit {Days, Weeks, Months, Years, Forever}`, `int sizeValue`,
  `enum SizeUnit {MB, GB, Unlimited}`; `static StorageSettings load()`, `save()`,
  `static StorageSettings defaults(QDate today)`;
  `QDateTime cutoff(const QDateTime &now) const` (null = no cutoff);
  `qint64 maxBytes() const` (0 = unlimited).
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Done 2026-10-08. Units are stored by name ("days"…"forever", "MB"/"GB"/"unlimited");
  `ImportSince` is ISO date, empty = all mail, missing key = default. Values < 1 load as 1.
  The import date counts from the start of that day in `now`'s time zone.

### T27 — Unit test for `StorageSettings`
- status: done
- depends on: T2, T26
- parallel-safe with: T4, T5, T8, T28
- files: autotests/storagesettingstest.cpp
- inputs: src/engine/storagesettings.h
- output: tests: MB/GB/Unlimited byte math; cutoff for each keep unit (incl. month-end, e.g.
  Mar 31 − 1 month); cutoff takes the later of start date and keep-for; Forever + null start
  → null cutoff; save→load round trip; defaults.
- verify: `ctest --test-dir build -R storagesettingstest --output-on-failure`
- notes: Done 2026-10-08. 10 cases incl. Mar 31 − 1 month in a normal and a leap year, 5 GB
  without int overflow, and "all mail" surviving save→load (not reverting to the default date).

### T28 — `MessageStore`: open database, add and look up messages
- status: todo
- depends on: T5, Q3
- parallel-safe with: T26, T27
- files: src/engine/messagestore.h, src/engine/messagestore.cpp
- inputs: `MessageSummary` (imapclient.h); storage layout in Conventions. Link Qt6::Sql.
- output: `MessageStore(const QString &baseDir)` (tests pass a temp dir; app passes
  AppDataLocation); `bool open()` creates schema
  `messages(folder, uidvalidity, uid, subject, sender, date, unread, size, path, PRIMARY KEY(folder, uid))`;
  `bool addMessage(folder, uidValidity, const MessageSummary&, const QByteArray &raw)`
  writes the `.eml` then the row; `bool contains(folder, uid) const`.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes:

### T29 — Unit test for `MessageStore` add/contains
- status: todo
- depends on: T2, T28
- parallel-safe with: T26, T27
- files: autotests/messagestoretest.cpp
- inputs: src/engine/messagestore.h; `QTemporaryDir`.
- output: open creates the DB; add → contains → `.eml` file exists with the same bytes;
  re-adding the same uid replaces it without a duplicate row.
- verify: `ctest --test-dir build -R messagestoretest --output-on-failure`
- notes:

### T30 — `MessageStore`: list, read and mark read
- status: todo
- depends on: T29
- parallel-safe with: T26, T27
- files: src/engine/messagestore.{h,cpp}, autotests/messagestoretest.cpp
- inputs: T28 schema.
- output: `QList<MessageSummary> messages(folder, int limit) const` (newest date first),
  `QByteArray rawMessage(folder, uid) const`, `void setUnread(folder, uid, bool)`, plus test
  cases for each.
- verify: `ctest --test-dir build -R messagestoretest --output-on-failure`
- notes:

### T31 — `MessageStore`: size accounting and removal
- status: todo
- depends on: T30
- parallel-safe with: T26, T27
- files: src/engine/messagestore.{h,cpp}, autotests/messagestoretest.cpp
- inputs: T28 schema.
- output: `qint64 totalSize() const`; `int removeOlderThan(const QDateTime&)`;
  `int removeOldestUntil(qint64 maxBytes)`; `int removeFolder(folder)` (for UIDVALIDITY
  change); `qint64 storedUidValidity(folder) const`; `void clear()`. Each deletes rows and
  `.eml` files together. Plus test cases for each.
- verify: `ctest --test-dir build -R messagestoretest --output-on-failure`
- notes:

### T32 — Retention: apply settings to the store
- status: todo
- depends on: T26, T31
- parallel-safe with: T33, T34
- files: src/engine/retention.h, src/engine/retention.cpp
- inputs: `StorageSettings::cutoff/maxBytes`, `MessageStore::removeOlderThan/removeOldestUntil`.
- output: `RetentionResult applyRetention(MessageStore&, const StorageSettings&, const QDateTime &now)`
  returning removed count and remaining bytes. Disabled settings → `clear()`.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes:

### T33 — Unit test for retention
- status: todo
- depends on: T2, T32
- parallel-safe with: T34
- files: autotests/retentiontest.cpp
- inputs: src/engine/retention.h, `MessageStore` in a `QTemporaryDir`.
- output: tests: time cutoff removes only older messages; size limit removes oldest first and
  stops at or under the limit; both together; disabled clears everything; unlimited/forever
  removes nothing.
- verify: `ctest --test-dir build -R retentiontest --output-on-failure`
- notes:

### T34 — `ImapClient::searchSince`
- status: todo
- depends on: T5
- parallel-safe with: T26–T33
- files: src/engine/imapclient.{h,cpp}
- inputs: `KIMAP::SearchJob` (check its header for the SINCE term and UID mode),
  `SelectJob::uidValidity()`.
- output: `void searchSince(folder, const QDate &since)` (null date = all messages) emitting
  `searchFinished(folder, qint64 uidValidity, QList<qint64> uids)`.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes:

### T35 — `ImapClient::fetchRawMessages`
- status: todo
- depends on: T34, Q3
- parallel-safe with: T26–T33
- files: src/engine/imapclient.{h,cpp}
- inputs: `FetchJob` with `FetchScope::Full`, UID based; `Message::encodedContent()` for raw
  bytes.
- output: `void fetchRawMessages(folder, const QList<qint64> &uids)` emitting
  `rawMessageFetched(folder, MessageSummary, QByteArray raw)` per message and
  `rawFetchFinished(folder)` at the end.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes:

### T36 — `SyncManager`: download one folder within limits
- status: todo
- depends on: T32, T35
- parallel-safe with: none
- files: src/engine/syncmanager.h, src/engine/syncmanager.cpp
- inputs: `ImapClient::searchSince/fetchRawMessages`, `MessageStore`, `StorageSettings`,
  `applyRetention`.
- output: `void syncFolder(folder)`: drop local copies if UIDVALIDITY changed; search since the
  cutoff date; skip uids already stored; fetch newest first in batches of 20; stop when the
  next message would pass `maxBytes`; then `applyRetention`. Signals `progress(folder, done, total)`,
  `folderSynced(folder)`, `errorOccurred(QString)`.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes:

### T37 — `SyncManager`: sync all chosen folders
- status: todo
- depends on: T36, Q2
- parallel-safe with: none
- files: src/engine/syncmanager.{h,cpp}
- inputs: `FolderInfo` list (role field), answer to Q2.
- output: `void syncAll(const QList<FolderInfo>&)` runs `syncFolder` one folder at a time for
  the chosen folders; signal `syncFinished()`.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes:

### T38 — `imap-probe --sync` for a manual storage check
- status: todo
- depends on: T6, T37, Q1
- parallel-safe with: T39–T41
- files: tools/imapprobe.cpp
- inputs: env vars from T6 plus `PLASMAMAIL_SINCE` (yyyy-MM-dd) and `PLASMAMAIL_MAX_MB`; uses a
  store directory given by `PLASMAMAIL_STORE_DIR`.
- output: `--sync` syncs all chosen folders and prints messages stored per folder and total MB.
- verify: (manual, needs Q1) run twice with `PLASMAMAIL_MAX_MB=5`: total stays ≤ 5 MB, the
  second run downloads nothing new; then with a later `PLASMAMAIL_SINCE` older mail is removed.
- notes:

### T39 — `MailController`: storage usage, cleanup and sync
- status: todo
- depends on: T37, T39a
- parallel-safe with: T38
- files: src/app/mailcontroller.{h,cpp}
- inputs: `MessageStore`, `SyncManager`, `applyRetention`.
- output: properties `storageUsedBytes`, `syncing`, `storageReady` (true once usage/cleanup
  work); `saveStorageSettings(...)` now also applies retention and starts a sync; invokable
  `cleanUpNow()`. Sync runs after connecting when enabled.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Split 2026-10-08: the settings properties moved to T39a.

### T39a — `MailController`: storage settings properties
- status: done
- depends on: T18, T26
- parallel-safe with: T27
- files: src/app/mailcontroller.{h,cpp}
- inputs: `StorageSettings`.
- output: properties `storageEnabled`, `importSince` (ISO date string, empty = all mail), `keepValue`,
  `keepUnit`, `sizeValue`, `sizeUnit` (one `storageSettingsChanged` signal; units exposed as
  ints matching the enums); invokable `saveStorageSettings(enabled, importSince, keepValue,
  keepUnit, sizeValue, sizeUnit)` that only saves (no store yet); `defaultStorageSettings()`
  returning a map with the defaults for "today", for the page's first switch-on.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Done 2026-10-08. **Deviation:** `importSince` is an ISO `"yyyy-MM-dd"` string, not a
  `QDate`: a QDate becomes a JS `Date` at midnight and time-zone conversion can shift it a day.
  `saveStorageSettings` clamps values to ≥ 1 and units to the enum range.

### T40 — `MailController`: read from local storage first
- status: todo
- depends on: T39
- parallel-safe with: T38
- files: src/app/mailcontroller.{h,cpp}
- inputs: `MessageStore::messages/rawMessage/setUnread`.
- output: when storage is enabled, `selectFolder` shows stored messages immediately and then
  refreshes from the server; `openMessage` parses the local `.eml` when present instead of
  fetching; marking read also updates the store.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes:

### T41 — Storage settings page
- status: done
- depends on: T21, T39a
- parallel-safe with: T27
- files: src/qml/StorageSettingsPage.qml, src/qml/Main.qml
- inputs: T39a properties.
- output: `Kirigami.ScrollablePage` + `FormLayout`: switch "Store mail on this computer";
  "Import mail from" with a date field and an "All mail" option; "Keep mail for" spin box +
  combo (days/weeks/months/years/forever); "Size limit" spin box + combo (MB/GB/unlimited);
  "Currently used: X" (formatted with `KFormat`/`Format.formatByteSize`); note that only
  local copies are removed; "Clean up now" and "Apply" buttons. Drawer action opens it.
- verify: offscreen launch as in T19 prints no QML warnings; manual: changing units keeps
  the number and the page reloads saved values after restart.
- notes: Done 2026-10-08. Opened from a "Local Storage" button in the drawer footer as a page layer.
  Import date uses Kirigami Addons `DatePopup` (runtime dep, QML only); a future date is clamped
  to today on accept. **Do not set `DatePopup.maximumDate`:** in Kirigami Addons 1.15 it hangs
  the app when the page opens (found by bisecting; nothing is logged). Extra "Defaults" button
  uses `defaultStorageSettings()`. Unit combos change only the unit, so the number is kept.
  Verified offscreen by temporarily opening the page at startup and calling Apply: config got
  `[Storage]` with the new values, and a restart loaded them back (incl. "All mail").
  Kirigami prints "StackView has detected conflicting anchors" (PageRow.qml) on any layer push,
  even an empty page; harmless.

### T46 — Storage page: show usage and enable "Clean up now"
- status: todo
- depends on: T39, T41
- parallel-safe with: T40
- files: src/qml/StorageSettingsPage.qml
- inputs: T39 `storageUsedBytes`, `syncing`, `storageReady`, `cleanUpNow`.
- output: "Currently used" shows `storageUsedBytes` formatted; "Clean up now" enabled when
  `storageReady` and calls `cleanUpNow()`; busy indicator while `syncing`.
- verify: offscreen launch as in T19 prints no QML warnings.
- notes:

### T42 — Manual end-to-end check of local storage
- status: todo
- depends on: T24, T40, T41, T46, Q1
- parallel-safe with: none
- files: none (record results; file new tasks for bugs)
- inputs: the user's Yahoo account.
- output: confirmed behaviour list in notes.
- verify: (manual, with the user) enable storage with a start date 1 month back and 50 MB →
  only mail since that date is stored and used size ≤ 50 MB → lower to 10 MB → oldest local
  copies removed, still visible on the server/webmail → offline (network off) the stored Inbox
  still lists and opens messages.
- notes:

### T43 — CMake presets for debug, release and sanitizer builds
- status: todo
- depends on: T1
- parallel-safe with: T3, T7, T14, T26, T44
- files: CMakePresets.json, CMakeLists.txt
- inputs: ECM module `ECMEnableSanitizers` (reads `ECM_ENABLE_SANITIZERS`).
- output: configure/build/test presets `dev` (Debug, Ninja, `build/`), `release`
  (RelWithDebInfo, `build-release/`), `asan` (Debug, `ECM_ENABLE_SANITIZERS="address;undefined"`,
  `build-asan/`); root CMake includes `ECMEnableSanitizers`. Add `build-*/` to `.gitignore`.
- verify: `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` passes.
- notes:

### T44 — clang-format setup with KDE style
- status: todo
- depends on: T1
- parallel-safe with: T3, T7, T14, T26, T43
- files: CMakeLists.txt, .clang-format (generated)
- inputs: ECM modules `KDEClangFormat` (`kde_clang_format(<files>)` adds target `clang-format`)
  and `KDEGitCommitHooks` (`kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)`).
- output: `cmake --build build --target clang-format` formats all `src/`, `autotests/`, `tools/`
  C++ files; the KDE `.clang-format` file is generated into the source dir and committed;
  pre-commit hook checks formatting.
- verify: `cmake --build build --target clang-format && git diff --exit-code -- src autotests`
  exits 0 after one formatting commit (run the target, commit, run again: no changes).
- notes:

### T45 — `ImapClient`: report a failed connection or login only once
- status: done
- depends on: T5
- parallel-safe with: T6–T15
- files: src/engine/imapclient.{h,cpp}
- inputs: found in the 2026-10-08 review: after `connectionFailed` or a failed `LoginJob`, every
  job already queued on the session also emits `errorOccurred`, so the UI shows several
  identical errors.
- output: after a connection/login failure, the session is dropped and queued jobs' errors are
  suppressed until the next request creates a new session; one `errorOccurred` per failure.
- verify: `cmake --build build --target plasmamail_engine` succeeds; manual with `imap-probe`
  (T6) and a wrong password prints exactly one error line.
- notes: Done 2026-10-08, pulled forward because it blocked T16. Each KIMAP session gets a
  generation number; every job/session callback carries it, and `isCurrent()` drops callbacks
  from an older session or after a reported failure. `fail()` reports once, drops the session;
  the next request opens a new one. `checkJob()` treats a job error as fatal only when the
  session is disconnected (a NO for a missing folder keeps the session). Also fixed a worse bug
  found in T16 testing: the old session's `connectionLost` cleanup used `m_session`, so closing
  an old session (e.g. `setAccount` after a wrong password) destroyed the *new* session; the
  right password then always failed. Verified on Yahoo: wrong password → exactly one error;
  wrong then right password on the same client works; imap-probe still succeeds.

### T47 — Readable login error; strip spaces from Yahoo app passwords
- status: done
- depends on: T20, T45
- parallel-safe with: none
- files: src/engine/imapclient.{h,cpp}, src/qml/AccountSetupPage.qml
- inputs: found in the first GUI run (2026-10-08): a rejected login showed
  "Login failed: Login failed, server replied: A000002 NO LOGIN Invalid credentials
  [ AUTHENTICATIONFAILED ]".
- output: a rejected login (server `NO`/`AUTHENTICATIONFAILED`) shows one translated sentence
  without IMAP protocol text; for Yahoo it adds that an app password is needed, not the normal
  password. Other login failures show "Could not log in: <reason>" once. The setup page removes
  all whitespace from the password for Yahoo only (app passwords never contain spaces; normal
  passwords for other providers may).
- verify: `cmake --build build` succeeds; manual: wrong Yahoo password shows the new message.
- notes: Done 2026-10-08. A login counts as rejected when the job text contains
  `AUTHENTICATIONFAILED` or ` NO `. Builds and offscreen checks pass; the message itself is
  confirmed in the user's GUI run: a rejected Yahoo login shows the new
  sentence as a passive notification.

### Look and feel: Geary-like redesign (2026-10-08)
The user finds the first GUI plain ("make it modern, at least like Geary but as Qt"). Target:
three panes on wide windows (folders | message list | message), one pane at a time on narrow
ones; a compact folder sidebar with the account at the top and settings in a menu; list rows
with a sender avatar, sender + date on one line, subject below, unread shown by a dot + bold;
a message view with a large subject, a sender header (avatar, name, address, date) and the
body below. Message previews in the list (needs fetching body text) are out of scope for now.

### T48 — Dev screenshot hook
- status: done
- depends on: T19
- parallel-safe with: none
- files: src/app/main.cpp
- inputs: needed to review the redesign without the user taking screenshots.
- output: when `PLASMAMAIL_SCREENSHOT=<file.png>` is set, the app waits
  `PLASMAMAIL_SCREENSHOT_DELAY` ms (default 6000), saves the window (`QQuickWindow::grabWindow`,
  the app window only, never the screen) and quits. With `PLASMAMAIL_SCREENSHOT_OPEN=1` it first
  opens the newest message of the current folder. Dev-only; documented in the commands section.
- verify: offscreen run with a fake account writes a PNG and exits 0.
- notes: Done 2026-10-08. Gets the controller with `engine.singletonInstance` and the uid via the
  model's "uid" role name. Offscreen screenshots use a light fallback palette; run on the real
  display for the user's Breeze colors.

### T49 — Folder sidebar
- status: done
- depends on: T48
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml, src/qml/Main.qml
- inputs: `MailController.folderModel`, `accountEmail`.
- output: replaces the GlobalDrawer content: header with the account (avatar + address,
  elided), folder list starting at the top (compact rows, current folder highlighted, depth
  indent), and a menu button ("Local Storage…", "Remove Account…") instead of the two footer
  buttons. Still docked on wide windows, modal on narrow.
- verify: offscreen checks as in T19 print no QML warnings; screenshot (T48) reviewed.
- notes: Done 2026-10-08. An `OverlayDrawer` set as both `globalDrawer` and `pageStack.leftSidebar`
  (inside the page row, so its header lines up with the page title bars). Avatar is
  `org.kde.kirigamiaddons.components` `Avatar` (Kirigami 6 has no `Kirigami.Avatar`).

### T50 — Three-pane layout
- status: done
- depends on: T49
- parallel-safe with: none
- files: src/qml/Main.qml, src/qml/MessageListPage.qml
- inputs: Kirigami `pageStack` (ColumnView).
- output: default window 75×45 grid units; the message list has a fixed column width
  (~28 grid units) and the message page is always pushed after it, filling the rest (empty
  placeholder until a message is opened); on narrow windows clicking a row moves to the message
  column. Toolbar header style so each pane's title row lines up like one header bar.
- verify: offscreen checks as in T19; screenshot with and without an open message reviewed.
- notes: Done 2026-10-08. List column via `Kirigami.ColumnView` min/preferred/max width 20/28/40
  grid units; MessagePage pushed once with `fillWidth` (Main.qml), `currentIndex` moves to it
  on click. Changing folder no longer pops it (the controller clears the message). The default
  ToolBar header style already lines up the title rows. Screenshot with real mail confirmed the
  CSP blocks Yahoo's remote images and fonts (WebEngine logs each block as `js: Refused…`).

### T51 — Message list rows
- status: done
- depends on: T50
- parallel-safe with: none
- files: src/qml/MessageListPage.qml
- inputs: model roles `from`, `subject`, `dateText`, `unread`, `uid`.
- output: rows with `Kirigami.Avatar` (initials from the sender name), sender (bold when unread)
  and date on the first line, subject below (elided, dimmer when read), an accent dot for
  unread, thin separators, comfortable but compact padding. Refresh as an icon button with
  tooltip. Placeholder and progress bar kept.
- verify: offscreen checks as in T19; screenshot reviewed.
- notes: Done 2026-10-08. Kirigami Addons `Avatar` (initials + color from the sender), unread =
  accent dot + bold sender + accent date; read subjects use `disabledTextColor`. Separators are
  inset to start at the text. Refresh is icon-only with a tooltip mentioning F5.

  Refined 2026-10-08 (T53 direction): solid `SenderAvatar` (new SenderAvatar.qml: name color,
  white initial), rounded inset selection/hover, no separators, semibold senders.
### T52 — Message view
- status: done
- depends on: T50
- parallel-safe with: T51
- files: src/qml/MessagePage.qml
- inputs: `MailController` message properties.
- output: large subject heading; sender block with avatar, display name and address (split
  from `messageFrom`), "to" line, date on the right; attachment chips; body below without a
  form layout. Placeholder ("No message selected") when nothing is open. Security settings of
  T23 unchanged.
- verify: offscreen checks as in T19; screenshot with an open message reviewed.
- notes: Done 2026-10-08. **Deviation:** also changed one line in mailcontroller.cpp:
  `messageFrom` is now `from()->asUnicodeString()` ("Name <address>") instead of
  `displayString()` (name only), so the page can show the address. The page title is empty; the
  subject is a level-2 heading in the page. WebEngine console messages are dropped
  (`onJavaScriptConsoleMessage`), so blocked remote loads no longer flood the log.

  Refined 2026-10-08 (T53 direction): level-1 bold subject, 2.5-grid-unit `SenderAvatar`,
  small dim address/"To" line, wider margins. Sidebar uses `SenderAvatar` too.
### T53 — Visual review with the user
- status: todo
- depends on: T49–T52
- parallel-safe with: none
- files: none (feedback becomes new tasks)
- inputs: the user runs the app.
- output: user's verdict on the new look recorded in notes.
- verify: manual.
- notes: 2026-10-08 user direction: "Mimestream's clean look with Mailspring's avatars", done as
  tighter spacing/typography in T51/T52 (no new tasks). Previews / smart grouping (Spark) not
  requested; add as new tasks only if the user asks.

### T54 — Mimestream visual pass
- status: done
- depends on: T51, T52
- parallel-safe with: none
- files: src/qml/MessageListPage.qml, src/qml/FolderSidebar.qml
- inputs: user's Mimestream screenshot (2026-10-08, "like this"): no avatars in the list,
  thin row dividers, soft neutral selection, bold titles, colored role icons in the sidebar.
- output: list rows without avatars (unread dot + sender/date line + subject line, room for a
  preview line from T55), inset hairline dividers, neutral rounded selection/hover; sidebar with
  a small "Folders" section label, icons tinted by `folderRole` (inbox blue, drafts teal, sent
  green, archive purple, junk amber, trash red, others neutral), the same neutral selection, and
  compact rows. Avatars stay in the message view.
- verify: offscreen checks as in T19; screenshot reviewed.
- notes: Done 2026-10-08. Selection/hover = `textColor` at 12 % / 5 % alpha, rounded
  (`Units.cornerRadius`). List title via `titleDelegate` (bold Heading; margin must be
  `leftPadding`, the title slot is not a Layout). Sidebar rows use a custom contentItem with
  `Kirigami.Icon { isMask: true }` so the role color applies.

### T55 — Message previews in the list
- status: todo
- depends on: T54
- parallel-safe with: none
- files: src/engine/imapclient.{h,cpp}, src/models/messagelistmodel.{h,cpp} (+ list delegate line)
- inputs: Mimestream/Spark-style third line in each row. Needs a fetch strategy decision
  (see notes) before starting.
- output: `MessageSummary.preview` (first ~150 characters of the plain text, whitespace
  collapsed), a `preview` model role, shown dimmed as the third row line.
- verify: unit test for the preview text extraction; screenshot reviewed.
- notes: Open decision: (a) fetch each listed message's first text part with BODYSTRUCTURE +
  a partial body fetch (light, more code), or (b) take previews from local storage once the
  store exists (no extra traffic, only for stored mail).

### T25 — Global verify
- status: todo
- depends on: T1–T24, T26–T55, T39a
- parallel-safe with: none
- files: none
- inputs: —
- output: clean build and passing tests.
- verify: `rm -rf build && cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
- notes:

---

## Execution Loop

Execute the plan as a loop. Each iteration handles one task in a fresh context, then ends.

**One iteration:**
1. **Orient**: read `PLAN.md`. Read only the files that task lists.
2. **Pick**: take the first task with `status: todo` whose dependencies are all `done`
   (and whose open questions are answered). Set it to `in-progress`.
3. **Act**: implement only that task. Don't fix unrelated issues you notice; add them to
   `PLAN.md` as new tasks instead.
4. **Verify**: run the task's verify check.
   - If it passes, set `status: done`.
   - If it fails, fix it and try again, at most 2 more times. If it still fails, set
     `status: blocked` and write the error and what you tried in `notes`.
   - If the task turns out bigger than planned, stop. Split it into new tasks in
     `PLAN.md`, set the original to `done` only if some part is finished and verified,
     and end the iteration.
5. **Record**: write handoff notes on the task (decisions, gotchas, anything the next
   task needs). Commit: `T3: add folder helpers`.
6. **Stop**: end the iteration. Don't start the next task in the same context.

**Loop exit conditions**, checked at the start of every iteration:
- All tasks are `done` and the global verify passes: report "PLAN COMPLETE" and stop.
- There is no pickable task (everything left is `blocked` or waits on a blocked task):
  report "PLAN BLOCKED", list the blocked tasks with their reasons, and stop.
- An open question needs the user: ask it and stop.

The loop never deletes tasks, rewrites finished ones, or marks a task `done` without its
verify check passing.
