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
  QML lint (since T159, 2026-10-09): `cmake --build --preset dev --target all_qmllint` (the Qt 6 qmllint
  with the build's import paths; `/usr/bin/qmllint` is an old 1.0 tool that prints nothing — don't use
  it). Baseline: **2** tagged warnings since T160 (2 unqualified: `applicationWindow()` in
  FolderSidebar.qml, a Drawer; was 20 after T159);
  count with `... 2>&1 | grep -o "\[[a-z-]*\]$" | wc -l`. A task must not raise it.
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
2. Gmail via OAuth2, alongside Yahoo: **planned below as "Milestone 2"** (multiple accounts +
   Google sign-in). Gmail extensions (X-GM-THRID threads, X-GM-LABELS) stay for later.
3. Outlook/Microsoft via OAuth2 (reuses milestone 2 code).
4. Compose / reply / forward over SMTP (KPim6::SMTP is installed): **pulled into Milestone 3**
   (2026-10-09, with all the message-toolbar actions).
5. IDLE (push), and **reordering accounts** (user request
   2026-10-08: drag handle or move up/down in Settings → Accounts, saved in the config and
   mirrored in the sidebar's account sections). The rest of multiple accounts (account manager
   with "Add Account…", shared views merging every inbox) moved into Milestone 2.

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
- The app never deletes or moves mail on the server in milestone 1. The only flags it changes are
  `\Seen` (set when a message is opened) and `\Flagged` (set/cleared by the user, T65–T67;
  user decision 2026-10-08).

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
  - Kirigami may print "ScrollablePage.qml:277: TypeError: Cannot read property 'flickable' of
    null" when a ScrollablePage is destroyed (closing the settings window or quitting). Kirigami
    internal teardown binding; harmless.
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
- status: done
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
  2026-10-08 run: unread dot hard to see, no flag shown in the list → T64.
  Seen in the later 2026-10-08 runs (storage checks): a folder opens from the sidebar (Inbox),
  an HTML Yahoo mail shows with its remote images blocked, every restart reconnected from the
  keychain without asking. Confirmed by the user 2026-10-08: an opened unread message lost
  its unread dot and stayed read after a restart. **User decision 2026-10-08:** the last
  step, "Remove account" returns to the setup page, moves to milestone 2's T95 (T90 rewrites
  adding and removing accounts). Every other step is confirmed, so T24 is done.

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
- status: done
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
- notes: Done 2026-10-08. Schema as planned plus a `flagged` column (MessageSummary gained
  `flagged` in T65). `date` is msecs since epoch UTC (NULL if unknown), indexed on (folder, date);
  `PRAGMA user_version = 1`. The `.eml` path is stored relative to baseDir:
  `messages/<sha1(folder)>/<uidvalidity>-<uid>.eml`, written atomically with QSaveFile before the
  row (`INSERT OR REPLACE`); if the row fails, a newly written file is removed; re-adding a uid
  under a new UIDVALIDITY deletes the old file. Each instance has its own QSqlDatabase connection
  name (UUID), removed in the destructor. `lastError()` gives the reason for a failed call.
  `open()` is idempotent. Verified: engine builds with no warnings, full build + all 8 tests pass.

### T29 — Unit test for `MessageStore` add/contains
- status: done
- depends on: T2, T28
- parallel-safe with: T26, T27
- files: autotests/messagestoretest.cpp
- inputs: src/engine/messagestore.h; `QTemporaryDir`.
- output: open creates the DB; add → contains → `.eml` file exists with the same bytes;
  re-adding the same uid replaces it without a duplicate row.
- verify: `ctest --test-dir build -R messagestoretest --output-on-failure`
- notes: Done 2026-10-08. 9 cases: open creates mail.sqlite + messages/ (also when baseDir
  doesn't exist; open twice is fine); calls on an unopened store fail without writing files;
  add → contains → the one `.eml` has the same bytes (CRLF, NUL, 0xFF kept); same uid in two
  folders; folder name with slashes, `..` and non-ASCII stays inside baseDir; re-add replaces
  (one row, one file, new bytes); re-add under a new UIDVALIDITY deletes the old file; data
  survives reopening; two stores at once don't share a connection. Row counts are checked
  through a separate QSQLITE connection (test links Qt6::Sql). The test finds `.eml` files
  by scanning messages/ because the path scheme is private. Verified in a cloud container
  (no KF6 there) by building only messagestore + this test against Qt 6.4 with KDE's strict
  defines and -Wall -Wextra -Werror, plus an ASan/UBSan run: 9/9 pass, no warnings. Then run
  through `ctest` in this machine's real build on 2026-10-08: all 9 test executables pass.

### T30 — `MessageStore`: list, read and mark read
- status: done
- depends on: T29
- parallel-safe with: T26, T27
- files: src/engine/messagestore.{h,cpp}, autotests/messagestoretest.cpp
- inputs: T28 schema.
- output: `QList<MessageSummary> messages(folder, int limit) const` (newest date first),
  `QByteArray rawMessage(folder, uid) const`, `void setUnread(folder, uid, bool)`, plus test
  cases for each.
- verify: `ctest --test-dir build -R messagestoretest --output-on-failure`
- notes: `setUnread` returns bool (false when the message isn't stored) instead of void.
  Results sort newest date first, undated last, uid breaks ties. Verified with
  `ctest --test-dir build -R messagestoretest` on 2026-10-08: passes.

### T31 — `MessageStore`: size accounting and removal
- status: done
- depends on: T30
- parallel-safe with: T26, T27
- files: src/engine/messagestore.{h,cpp}, autotests/messagestoretest.cpp
- inputs: T28 schema.
- output: `qint64 totalSize() const`; `int removeOlderThan(const QDateTime&)`;
  `int removeOldestUntil(qint64 maxBytes)`; `int removeFolder(folder)` (for UIDVALIDITY
  change); `qint64 storedUidValidity(folder) const`; `void clear()`. Each deletes rows and
  `.eml` files together. Plus test cases for each.
- verify: `ctest --test-dir build -R messagestoretest --output-on-failure`
- notes: Remove functions return -1 on error (see `lastError()`); `clear()` returns bool and
  also deletes stray `.eml` files. `removeOlderThan` keeps undated messages; `removeOldestUntil`
  evicts undated ones last (changed in T38a; first version evicted them first). `storedUidValidity` returns 0 when nothing is
  stored. Rows are deleted in one transaction before their files, so a failure can only leave
  an orphan file, never a row without its file. Verified with `ctest --test-dir build` on
  2026-10-08: all 9 test executables pass.

### T32 — Retention: apply settings to the store
- status: done
- depends on: T26, T31
- parallel-safe with: T33, T34
- files: src/engine/retention.h, src/engine/retention.cpp
- inputs: `StorageSettings::cutoff/maxBytes`, `MessageStore::removeOlderThan/removeOldestUntil`.
- output: `RetentionResult applyRetention(MessageStore&, const StorageSettings&, const QDateTime &now)`
  returning removed count and remaining bytes. Disabled settings → `clear()`.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: `RetentionResult` also has `ok` (false when the store failed; reason in
  `MessageStore::lastError()`). The time cutoff runs before the size limit. Storage off counts
  removals with `removeOldestUntil(0)`, then `clear()` drops stray files. `cmake --build build
  --target plasmamail_engine` succeeds on 2026-10-08 with no warnings. Reconfiguring prints
  "error: No such remote 'origin'" (the repo has no remote), and the build still succeeds.

### T33 — Unit test for retention
- status: done
- depends on: T2, T32
- parallel-safe with: T34
- files: autotests/retentiontest.cpp
- inputs: src/engine/retention.h, `MessageStore` in a `QTemporaryDir`.
- output: tests: time cutoff removes only older messages; size limit removes oldest first and
  stops at or under the limit; both together; disabled clears everything; unlimited/forever
  removes nothing.
- verify: `ctest --test-dir build -R retentiontest --output-on-failure`
- notes: Also covers the importSince cutoff, an exact fit at the size limit (kept) and a store
  that isn't open (`ok` false). Registered in autotests/CMakeLists.txt. Verified with
  `ctest --test-dir build` on 2026-10-08: all 10 test executables pass.

### T34 — `ImapClient::searchSince`
- status: done
- depends on: T5
- parallel-safe with: T26–T33
- files: src/engine/imapclient.{h,cpp}
- inputs: `KIMAP::SearchJob` (check its header for the SINCE term and UID mode),
  `SelectJob::uidValidity()`.
- output: `void searchSince(folder, const QDate &since)` (null date = all messages) emitting
  `searchFinished(folder, qint64 uidValidity, QList<qint64> uids)`.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Issues its own SELECT (not `queueSelect`) to read UIDVALIDITY. Null date searches
  `1:*` (KIMAP has no ALL term); an empty folder emits an empty list without searching. SINCE
  uses the server's arrival date, not the Date header. uids are emitted ascending. Builds with
  no warnings on 2026-10-08; not yet run against a server (first use is in the sync tasks).

### T35 — `ImapClient::fetchRawMessages`
- status: done
- depends on: T34, Q3
- parallel-safe with: T26–T33
- files: src/engine/imapclient.{h,cpp}
- inputs: `FetchJob` with `FetchScope::Full`, UID based; `Message::encodedContent()` for raw
  bytes.
- output: `void fetchRawMessages(folder, const QList<qint64> &uids)` emitting
  `rawMessageFetched(folder, MessageSummary, QByteArray raw)` per message and
  `rawFetchFinished(folder)` at the end.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: One FETCH per call, so the caller (SyncManager) batches the uids. Raw bytes are
  `encodedContent(NewlineType::CRLF)`. `FetchScope::Full` sends `BODY.PEEK[] FLAGS UID`
  (checked in libKPim6IMAP's strings), so downloading never marks mail read and the summary
  keeps the real read and flagged state. Header parsing moved into a shared `summaryOf()` used by
  `fetchSummaries` too. An empty uid list emits `rawFetchFinished` at once. Builds with no
  warnings and all 10 tests pass on 2026-10-08; not yet run against a server.

### T36 — `SyncManager`: download one folder within limits
- status: done
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
- notes: Constructor takes `ImapClient*` and `MessageStore*` (not owned); `setSettings()`.
  **Size rule:** without RFC822.SIZE the size is only known after download, so each run
  downloads at most `maxBytes` (newest first), then `applyRetention` removes the oldest copies
  overall. Counting already-stored mail instead would stop a full store from ever taking new
  mail. Disk use can briefly reach about twice `maxBytes` during a first sync.
  SINCE has day precision and uses the arrival date, so fetched messages whose Date header is
  before the cutoff are skipped. Storage off → `syncFolder` only applies retention (clears).
  Any `ImapClient::errorOccurred` during a run aborts it. Results are matched by folder name
  only: starting a new sync of the same folder right after a failure could pick up the old run's
  late results (harmless: they are real messages of that folder). Builds with no warnings on
  2026-10-08; first real run is the T38 probe.

### T37 — `SyncManager`: sync all chosen folders
- status: done
- depends on: T36, Q2
- parallel-safe with: none
- files: src/engine/syncmanager.{h,cpp}
- inputs: `FolderInfo` list (role field), answer to Q2.
- output: `void syncAll(const QList<FolderInfo>&)` runs `syncFolder` one folder at a time for
  the chosen folders; signal `syncFinished()`.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Chosen folders per Q2: every role except "trash" and "junk" (static
  `SyncManager::isChosen`). The first error stops the whole run (an IMAP error drops the
  session, so carrying on would repeat it); `errorOccurred` is then the last signal, without
  `syncFinished`. The `maxBytes` download budget is per folder, so one run downloads at most
  folders × `maxBytes` before retention trims the store. Gmail's "all" and "flagged" folders
  would duplicate mail; revisit in milestone 2. Builds with no warnings on 2026-10-08.

### T38 — `imap-probe --sync` for a manual storage check
- status: done
- depends on: T6, T37, Q1
- parallel-safe with: T39–T41
- files: tools/imapprobe.cpp
- inputs: env vars from T6 plus `PLASMAMAIL_SINCE` (yyyy-MM-dd) and `PLASMAMAIL_MAX_MB`; uses a
  store directory given by `PLASMAMAIL_STORE_DIR`.
- output: `--sync` syncs all chosen folders and prints messages stored per folder and total MB.
- verify: (manual, needs Q1) run twice with `PLASMAMAIL_MAX_MB=5`: total stays ≤ 5 MB, the
  second run downloads nothing new; then with a later `PLASMAMAIL_SINCE` older mail is removed.
- notes: Done 2026-10-08, run by the user against Yahoo (since 2026-01-01, 5 MB): run 1 stored
  INBOX 54 + Sent 6 = 2.48 MB (Trash/Bulk skipped, Draft/Archive empty); run 2 downloaded
  nothing; run 3 with since 2026-09-01 left INBOX 11 + Sent 6 = 0.18 MB. The size limit was
  never reached, so eviction was not exercised against the server (the unit tests cover it).
  **Found:** 6 INBOX and all 6 Sent messages were stored undated: they have no `Date:` header
  (Yahoo webmail doesn't add one). KIMAP fetches INTERNALDATE but loses it (parses it as
  RFC 2822, then `parse()` replaces the headers). The user chose to fall back to the newest
  `Received:` header and treat undated messages as newest for size eviction (T38a); a separate
  INTERNALDATE fetch was not chosen. Code: keep time is forever, so only `PLASMAMAIL_SINCE` and
  `PLASMAMAIL_MAX_MB` limit the store; the 60 s timeout restarts on every progress signal.

### T38a — Dates for messages without a `Date:` header
- status: done
- depends on: T38
- parallel-safe with: T39
- files: src/engine/imapclient.{h,cpp}, src/engine/messagestore.{h,cpp},
  autotests/messagedatetest.cpp, autotests/messagestoretest.cpp, autotests/CMakeLists.txt
- inputs: T38 finding; RFC 5322 `Received:` (date after the last `;`, newest first).
- output: `QDateTime messageDate(const KMime::Message&)`: the Date header, else the date of the
  first `Received:` header that parses, else null; used for every `MessageSummary`.
  `MessageStore::removeOldestUntil` evicts undated messages last (they are usually sent mail).
  Tests for both.
- verify: `ctest --test-dir build --output-on-failure`
- notes: Done 2026-10-08. The Received date is parsed with `KMime::Headers::Date`, so
  trailing comments like `(UTC)` work; Received headers without a parsable date are skipped.
  `messageDate` is declared in imapclient.h (next to `MessageSummary`). Sent mail from Yahoo
  webmail has no Received header and stays undated. The header list from the server
  (`FetchScope::Headers`) only asks for the Date header, so there the fallback can't help; it
  works for stored messages. Checked against the T38 store: the 6 undated INBOX messages'
  newest Received dates are Jan–May 2026, so with this fix the since-2026-09-01 run would have
  removed them. `ctest --test-dir build`: all 11 test executables pass.

### T39 — `MailController`: storage usage, cleanup and sync
- status: done
- depends on: T37, T39a
- parallel-safe with: T38
- files: src/app/mailcontroller.{h,cpp}
- inputs: `MessageStore`, `SyncManager`, `applyRetention`.
- output: properties `storageUsedBytes`, `syncing`, `storageReady` (true once usage/cleanup
  work); `saveStorageSettings(...)` now also applies retention and starts a sync; invokable
  `cleanUpNow()`. Sync runs after connecting when enabled.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Split 2026-10-08: the settings properties moved to T39a. Done 2026-10-08: the store
  lives in `QStandardPaths::AppDataLocation`; `storageReady` is CONSTANT (the store opens once,
  at startup). SyncManager shares the controller's `ImapClient`; its batches leave gaps, so
  UI requests are not stuck behind a long sync. Sync starts after every folder list (connect
  and refresh) for the saved account only, never while a new account is being verified.
  **SyncManager changes:** client errors now end a run with `aborted()` instead of
  `errorOccurred` (the controller already shows the client's error); new `stop()` because a
  disconnect (`setAccount`, `removeAccount`) reports no error and the run would wait forever.
  Store errors are only logged (qWarning), since mail still works from the server. Turning
  storage off stops a running sync, then retention deletes the copies. Removing the account,
  or saving a different one, deletes the local copies. `cmake --build build --target
  plasma-mail` succeeds and all 11 tests pass; not yet run in the GUI (storage is off in the
  user's config, and the settings page arrives in T41).

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
- status: done
- depends on: T39
- parallel-safe with: T38
- files: src/app/mailcontroller.{h,cpp}
- inputs: `MessageStore::messages/rawMessage/setUnread`.
- output: when storage is enabled, `selectFolder` shows stored messages immediately and then
  refreshes from the server; `openMessage` parses the local `.eml` when present instead of
  fetching; marking read also updates the store.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Done 2026-10-08. The stored list shows only when the list is empty (a folder switch),
  so a refresh of the same folder doesn't flash older data. Unread/Flagged views filter the
  stored inbox locally. Local `.eml` is parsed like KIMAP does (`CRLFtoLF`, `parse()`), and
  opening it needs no server request. The reader's date now uses `messageDate()` too (Received
  fallback). **Not covered:** flagging doesn't update the store (no `MessageStore::setFlagged`),
  and read/flag changes made elsewhere only reach the store when a message is re-downloaded,
  which sync never does for stored uids. The server's list replaces the stored one within
  seconds, so this shows only briefly; worth a later task. Builds, all 11 tests pass; not yet
  run in the GUI (storage is off until T41's page).

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
- status: done
- depends on: T39, T41
- parallel-safe with: T40
- files: src/qml/StorageSettingsPage.qml
- inputs: T39 `storageUsedBytes`, `syncing`, `storageReady`, `cleanUpNow`.
- output: "Currently used" shows `storageUsedBytes` formatted; "Clean up now" enabled when
  `storageReady` and calls `cleanUpNow()`; busy indicator while `syncing`.
- verify: offscreen launch as in T19 prints no QML warnings.
- notes: Done 2026-10-08. Size via `org.kde.coreaddons` `Format.formatByteSize`; a small busy
  indicator and "Downloading…" while `syncing`; "Not available" text when the store didn't
  open. "Clean Up Now" uses the saved limits (Apply already cleans up with new ones), so it is
  enabled only when storage is on and the store is ready. Verified offscreen by temporarily
  pushing the page at startup (reverted): no warnings from the page. **Pre-existing, not from
  this task:** every offscreen start prints "Created graphical object was not placed in the
  graphics scene" for MessageListPage (Main.qml:112) and MessagePage (Main.qml:118).

### T46a — Remember the folder list for offline starts
- status: done
- depends on: T40, T46
- parallel-safe with: none
- files: src/engine/messagestore.{h,cpp}, autotests/messagestoretest.cpp,
  src/app/mailcontroller.{h,cpp}, PLAN.md (T42 depends on this)
- inputs: found while preparing T42: offline, the folder list never arrives, so the inbox path
  is unknown and nothing is shown, stored or not.
- output: `MessageStore::setFolders(QList<FolderInfo>)` / `folders()` (a `folders` table, in
  list order; `clear()` empties it too). The controller saves each folder list when storage is
  on, and at startup with storage on shows the stored folders and the stored All Inboxes
  before connecting; the first real folder list then refreshes the current view from the
  server. Test cases for the store part.
- verify: `ctest --test-dir build --output-on-failure`; offscreen start prints no new warnings.
- notes: Done 2026-10-08. The `folders` table is created with `IF NOT EXISTS`, so existing
  stores gain it without a schema bump. Null names or roles are stored as "" (NOT NULL
  columns). The stored list is not put in `m_folders`, so a sync still waits for the server's
  own list. Folder lists from a new account still being verified are not saved. Offline, picking
  another folder shows its stored mail, then reports the connection error. All 11 tests pass;
  offscreen start shows only the two known warnings. The stored-start path itself is first
  exercised in T42 (storage is off in the user's config).

### T42 — Manual end-to-end check of local storage
- status: done
- depends on: T24, T40, T41, T46, T46a, Q1
- parallel-safe with: none
- files: none (record results; file new tasks for bugs)
- inputs: the user's Yahoo account.
- output: confirmed behaviour list in notes.
- verify: (manual, with the user) enable storage with a start date 1 month back and 50 MB →
  only mail since that date is stored and used size ≤ 50 MB → lower to 10 MB → oldest local
  copies removed, still visible on the server/webmail → offline (network off) the stored Inbox
  still lists and opens messages.
- notes: Done 2026-10-08 with the user (Yahoo). Storage on, since 2026-09-08, 50 MB then
  10 MB: 5 INBOX messages stored (0.11 MB, all dated October; the six once-undated ones are
  now dated Jan–May by T38a and correctly left out). Sent stored nothing (its mail predates
  the cutoff). The size limit was never reached, so eviction in the GUI is unconfirmed (unit
  tests cover it). Offline was simulated with `unshare --map-current-user -n
  build/bin/plasma-mail` (the app has no network; the rest of the desktop keeps it): folders
  and the 5 stored INBOX messages show, "Could not connect to imap.mail.yahoo.com." appears, a
  stored message opens. **Bug found and fixed:** the folder list was only saved when it arrived
  while storage was already on, so turning storage on in a running app left no list for an
  offline start; `saveStorageSettings` now saves it too. The log also showed
  `ScrollablePage.qml:277: TypeError: Cannot read property 'flickable' of null` (3×) after the
  settings window was used; **fixed 2026-10-08**: a Kirigami bug (its `scrollingArea` binds
  `root.flickable.width` without a null guard, still so upstream), hit when the settings window
  is destroyed with ScrollablePages in it (ours, FormCardPage and the About page).
  Main.qml now clears the settings window's pages and layers on its `closing` signal (the
  window is destroyed only after the signal). Reproduced 5/5 in a minimal QML file and in the
  app (offscreen: open Settings at Local Storage, close); with the fix 0 errors.

### T43 — CMake presets for debug, release and sanitizer builds
- status: done
- depends on: T1
- parallel-safe with: T3, T7, T14, T26, T44
- files: CMakePresets.json, CMakeLists.txt
- inputs: ECM module `ECMEnableSanitizers` (reads `ECM_ENABLE_SANITIZERS`).
- output: configure/build/test presets `dev` (Debug, Ninja, `build/`), `release`
  (RelWithDebInfo, `build-release/`), `asan` (Debug, `ECM_ENABLE_SANITIZERS="address;undefined"`,
  `build-asan/`); root CMake includes `ECMEnableSanitizers`. Add `build-*/` to `.gitignore`.
- verify: `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` passes.
- notes: Done 2026-10-08. Presets file version 3 (needs CMake ≥ 3.21; the project requires
  3.22). Test presets print output on failure; `asan` also sets
  `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`, so undefined behaviour fails the test.
  `build-*/` was already in .gitignore. License via `CMakePresets.json.license` (REUSE; JSON
  has no comments). Verified: `asan` binaries link libasan and libubsan, all 11 tests pass with
  leak detection on; `dev` (the existing build/) and `release` also build and pass.

### T44 — clang-format setup with KDE style
- status: done
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
- notes: Done 2026-10-08 (clang-format 23). **Deviation:** `.clang-format` is git-ignored,
  not committed: ECM's docs say so (it regenerates the file at configure time, and a committed
  copy would stop ECM updating it). The target covers src/, autotests/ and tools/ (globbed with
  CONFIGURE_DEPENDS). First run reformatted 10 files (~90 lines, layout only: enums one value
  per line, lambda bodies re-indented); a second run changes nothing; build and all 11 tests
  pass. `kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)` writes .git/hooks/pre-commit,
  which runs `git clang-format` on staged lines.

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
- status: done
- depends on: T49–T52
- parallel-safe with: none
- files: none (feedback becomes new tasks)
- inputs: the user runs the app.
- output: user's verdict on the new look recorded in notes.
- verify: manual.
- notes: 2026-10-08 user direction: "Mimestream's clean look with Mailspring's avatars", done as
  tighter spacing/typography in T51/T52 (no new tasks). Previews / smart grouping (Spark) not
  requested; add as new tasks only if the user asks.
  **Verdict 2026-10-08 (user): "the look is good".**

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
- status: done
- depends on: T40, T54
- parallel-safe with: T41–T46
- files: src/engine/messagestore.{h,cpp} (preview column), src/models/messagelistmodel.{h,cpp}
  (+ list delegate line)
- inputs: Mimestream/Spark-style third line in each row. **Decided 2026-10-08 (user): option b**,
  previews come from local storage; no extra IMAP fetching. Messages not stored locally show no
  preview line.
- output: the store keeps a preview (first ~150 characters of the plain text, whitespace
  collapsed) per message, computed when the message is stored; `MessageSummary.preview` filled
  from the store; `preview` model role shown dimmed as the third row line when not empty.
- verify: unit test for preview extraction; screenshot reviewed.
- notes: Done 2026-10-08. `messagePreview()` in messagerenderer: text/plain part first, else
  the HTML part via `QTextDocumentFragment` (parses only, loads nothing); quoted `>` lines
  skipped; U+FFFC image placeholders and format characters (zero-width preheader padding)
  removed; whitespace collapsed; cut at 150 with "…". `MessageStore::addMessage` computes it
  (`preview` column, schema 3); existing stores get the column on open, and any schema bump
  recomputes all previews from the .eml files (bump again when `messagePreview` improves).
  `previews(folder)` lets the controller put previews into the server's list, which has no
  body text. QML: dimmed third line, up to 2 lines, hidden when empty. Tests: 6 preview cases
  in messagerenderertest; store previews, v1 migration and recompute in messagestoretest; all
  11 pass. Screenshot (real display) reviewed: stored INBOX rows show previews, others stay
  two lines; first version showed U+FFFC boxes from images, fixed.

### T56 — Message toolbar (look only)
- status: done
- depends on: T54
- parallel-safe with: none
- files: src/qml/MessagePage.qml
- inputs: user 2026-10-08: "toolbar actions for sure… icons just for looks, can be disabled".
- output: the message column's title bar holds Mimestream-style icon buttons — Archive, Delete,
  Mark as Unread, Move to Folder, Label | Reply, Reply All, Forward — and a search field on the
  right. All disabled (milestone 1 is read-only; they get wired up in later milestones), with
  accessible names set. Shown only when a message is open, except the search field.
- verify: offscreen checks as in T19; screenshot reviewed.
- notes: Done 2026-10-08. Built as the page's `titleDelegate` (Repeater of ToolButtons, a
  Separator, spacer, `Kirigami.SearchField`). No Breeze `mail-move` icon: Move uses `folder`.
  Disabled buttons show greyed, as intended.

### Sidebar by account with shared folders (2026-10-08)
User: "folders on left side panel should be sorted by accounts. On top should be common inbox
and maybe two more folders." Chosen (user): **All Inboxes, Unread, Flagged** on top, then one
section per account with its folders. Only one account exists until milestones 2–3, so the
shared views cover that account's Inbox; with more accounts they will merge every Inbox.
Unread/Flagged cover the inboxes only (cheap: one SEARCH per inbox), not every folder.

### T57 — `ImapClient`: filtered header fetch
- status: done
- depends on: T5
- parallel-safe with: none
- files: src/engine/imapclient.{h,cpp}
- inputs: KIMAP `SearchJob` (`Term(Seen).setNegated(true)` / `Term(Flagged)`, UID based).
- output: `enum HeaderFilter { AllMessages, Unread, Flagged }`; `fetchHeaders(folder, limit,
  filter = AllMessages)`: AllMessages as before; the others SEARCH, take the newest `limit`
  UIDs and UID FETCH their headers. `headersFetched(folder, filter, messages)` carries the
  filter so callers can tell results apart. `MessageSummary.flagged` filled from `\Flagged`.
  MailController's slot gets the new signature (ignores the filter until T58).
- verify: `cmake --build build` succeeds; tests pass.
- notes: Done 2026-10-08. Shared header-fetch code moved to `fetchSummaries(set, uidBased, …)`.
  Search terms are `KIMAP::Term::Seen` (negated) / `Term::Flagged` (enum lives in `Term`).
  mailcontroller.h now includes imapclient.h (nested enum in a slot signature); imap-probe's
  lambda updated for the new signal.

### T58 — `MailController`: shared views
- status: done
- depends on: T57
- parallel-safe with: none
- files: src/app/mailcontroller.{h,cpp}
- inputs: T57.
- output: constant property `sharedFolders` (list of maps: `path`, `name`, `iconName`) for
  "All Inboxes", "Unread", "Flagged", with reserved paths `plasmamail:inboxes`,
  `plasmamail:unread`, `plasmamail:flagged`. `selectFolder` accepts them: each maps to the
  inbox plus a filter; `currentFolderName` returns their names; opening/marking messages uses
  the real folder. After connecting, the start view is All Inboxes instead of Inbox.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Done 2026-10-08. `imapFolderFor(path)` / `filterFor(path)` map a view to inbox + filter;
  header results are matched on both folder and filter. A shared view selected before the
  folder list is known fetches nothing (no inbox path yet).

### T59 — Sidebar sections: shared folders, then per account
- status: done
- depends on: T58
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml
- inputs: `sharedFolders`, `folderModel`, `accountEmail`.
- output: the sidebar's title bar shows "Mail" and the account menu; below it a section with
  All Inboxes (blue), Unread (accent), Flagged (orange); then an account section headed by the
  account's avatar and address (click to collapse) listing its folders as now.
- verify: offscreen checks as in T19; screenshot reviewed.
- notes: Done 2026-10-08. Inline components `FolderRow` and `SectionLabel`; one ScrollView with
  shared rows, the collapsible account header and the account's folders. Verified on the real
  account via a temporary Main.qml patch selecting each view: All Inboxes lists the inbox;
  Unread and Flagged were empty with no error (the inbox had no unread/flagged mail), so a
  non-empty match is still unconfirmed — check when unread/flagged mail exists.

  **Bug found by the user 2026-10-08, fixed:** clicking an account folder did nothing. The
  account delegate redeclared `required property string path`, which hid `FolderRow.path`; the
  component's click handler reads its own (empty) `path`, so `selectFolder("")` returned early.
  Shared views worked because they set `path` directly. Delegate now uses `required property var
  model`. Lesson: never redeclare a property an inline component already has.
### T60 — Settings window
- status: done
- depends on: T41, T59
- parallel-safe with: none
- files: src/qml/Main.qml, src/qml/FolderSidebar.qml
- inputs: user 2026-10-08: "make settings button, add it to menu above left panel".
- output: Kirigami Addons `ConfigurationView` (own window on desktop) with modules Accounts
  (T61), Local Storage (StorageSettingsPage), About Plasma Mail (FormCard `AboutPage`, from
  KAboutData). The ☰ menu: "Settings…" (Ctrl+,) and "About Plasma Mail"; "Local Storage…" and
  "Remove Account…" move into Settings.
- verify: offscreen checks as in T19; screenshot of each settings page reviewed.
- notes: Done 2026-10-08. Modules: Local Storage, About (Accounts comes with T61, so
  "Remove Account…" stays in the menu until then). KAboutData: desktop file name `plasma-mail`
  (the default `org.kde.…` made the About page link KDE's donate/get-involved pages), empty bug
  address (else a bugs.kde.org link), license GPL-2.0-or-later. Screenshot hook (T48) now grabs
  the focused window, so the settings window can be captured.

### T61 — Accounts settings page
- status: done
- depends on: T60
- parallel-safe with: none
- files: src/qml/AccountSettingsPage.qml, src/app/mailcontroller.{h,cpp}
- inputs: `Account` fields.
- output: controller properties `accountDisplayName`, `accountProvider` (readable name),
  `accountServer` ("host:port"); a FormCard page showing the account (avatar, address, name,
  provider, IMAP server) and a "Remove Account…" button with the confirmation dialog
  (moved from Main.qml).
- verify: offscreen checks as in T19; screenshot reviewed.
- notes: Done 2026-10-08. The ☰ menu is now "Settings…" and "About Plasma Mail"; Remove Account
  lives on this page. After removal, Main.qml closes the settings window (desktop) or clears the
  layers (mobile) on `accountChanged`. Removal itself not exercised on the real account (it would
  delete the user's saved account); the dialog flow is unchanged from T21.

### T62 — Compose button and New Message window (no sending yet)
- status: done
- depends on: T54
- parallel-safe with: none
- files: src/qml/ComposeWindow.qml, src/qml/MessageListPage.qml
- inputs: user 2026-10-08: "add create new mail and window for creating new mail next to check
  for new mail". **Decided (user): window now, sending later** (sending is milestone 4).
- output: a "New Message" icon button (`mail-message-new`, Ctrl+N) left of Refresh in the list
  header; opens a separate window: From (account address, read-only), To, Cc, Subject, message
  text; footer with Discard (asks first when anything was typed) and Send, disabled with a
  tooltip saying sending comes in a later version. Several windows can be open; each is
  destroyed when closed.
- verify: offscreen checks as in T19; screenshot of the window reviewed.
- notes: Done 2026-10-08. A `Kirigami.ApplicationWindow` created per click with the main window as
  parent; `onClosing` asks before discarding typed text, then `destroy()`s. **Deviation:** a
  disabled button shows no tooltip (no hover events), so a small footer note says "Sending mail
  comes in a later version." instead.

### T63 — Accounts page as an account manager
- status: done
- depends on: T61
- parallel-safe with: none
- files: src/qml/AccountSettingsPage.qml, src/qml/AccountDetailsPage.qml
- inputs: user 2026-10-08: "account info under account name; at account page should be a list
  of accounts only; front page should be used as accounts manager".
- output: Settings → Accounts (the first page) lists the accounts only: a row per account with
  avatar, name and, under it, address · provider. Clicking a row opens AccountDetailsPage (a page
  layer in the settings window) with the details from T61 and Remove Account. Below the list an
  "Add Account…" button, disabled with a note until multiple accounts exist (milestones 2–3).
- verify: offscreen checks as in T19; screenshots of both pages reviewed.
- notes: Done 2026-10-08. AccountDetailsPage is the former T61 page (git mv), titled with the
  account name. Rows open it with `QQC2.ApplicationWindow.window.pageStack.layers.push`, which
  stays inside the settings window and shows a back arrow. Verified by triggering the row's
  click in a temporary patch; both pages screenshotted.

### T64 — Clearer unread marker and flag in the list
- status: done
- depends on: T57, T54
- parallel-safe with: none
- files: src/models/messagelistmodel.{h,cpp}, src/qml/MessageListPage.qml (+ model test)
- inputs: user's T24 run 2026-10-08: "dot for unread is not very visible, no starred info".
- output: `FlaggedRole` ("flagged") from `MessageSummary.flagged` (filled since T57); unread rows
  get a larger accent dot plus an accent bar at the row's left edge; flagged rows show an orange
  flag icon next to the date. Model test covers the new role.
- verify: `ctest --test-dir build -R messagelistmodeltest`; offscreen checks as in T19;
  screenshot with unread and flagged mail reviewed.
- notes: Done 2026-10-08. Unread = 10 px dot, 3 px left bar, 10 % tinted row and accent date, in a
  fixed `#3daee9` (`page.unreadColor`): the theme `highlightColor` goes dull navy when the
  window is inactive. Flag = orange `flag` icon before the date. Confirmed in the user's own
  screenshot of the new build.

### T65 — `ImapClient::setFlagged`
- status: done
- depends on: T57
- parallel-safe with: none
- files: src/engine/imapclient.{h,cpp}
- inputs: user 2026-10-08: "add flag and unflag". Same pattern as `markSeen`.
- output: `setFlagged(folder, uid, bool flagged)`: UID STORE +FLAGS / -FLAGS `\Flagged`.
- verify: `cmake --build build` succeeds.
- notes: Done 2026-10-08. Mirrors `markSeen`; RemoveFlags mode for unflag.

### T66 — `MailController`: flag / unflag
- status: done
- depends on: T65, T64
- parallel-safe with: none
- files: src/app/mailcontroller.{h,cpp}, src/models/messagelistmodel.{h,cpp} (+ model test)
- inputs: T65.
- output: `MessageListModel::setFlagged(uid, bool)` (emits FlaggedRole); controller property
  `messageFlagged` (open message, part of `currentMessageChanged`) and invokable
  `setFlagged(uid, bool)`: updates the list and the open message at once, then stores the flag
  on the server in the uid's folder (the shared views map to the inbox).
- verify: `ctest --test-dir build -R messagelistmodeltest`; build succeeds.
- notes: Done 2026-10-08. `messageFlagged` is taken from the list row when a message is opened
  (KMime's parsed message has no IMAP flags). Optimistic update: list and open message change
  first, then UID STORE; a failure is reported via `errorOccurred` like other jobs.

### T67 — Flag controls in the UI
- status: done
- depends on: T66, T56
- parallel-safe with: none
- files: src/qml/MessagePage.qml, src/qml/MessageListPage.qml
- inputs: T66.
- output: the message toolbar gets a working Flag/Unflag toggle (orange when flagged), first
  enabled toolbar button; shortcut Ctrl+Shift+F toggles the open message. In the list, hovering a
  row shows a flag button before the date; clicking it toggles that message (flagged rows keep the
  orange flag visible). Flagged view: unflagged rows stay until the next refresh (like Unread).
- verify: offscreen checks as in T19; manual: flag in the app, see it in Yahoo webmail, and back.
- notes: Done 2026-10-08. Toolbar toggle is a third Loader component in the T56 Repeater; list
  toggle is a flat ToolButton (opacity 1 flagged / 0.5 hovered / 0). Also fixed: the toolbar flag
  now shows while the message loads (controller emits after setting `messageFlagged`), and the
  message header stays hidden until the message arrives (`loaded`), instead of "(no subject)" and
  an empty avatar. Server round trip still to be checked by the user (flag in app → webmail).

### T25 — Global verify
- status: done
- depends on: T1–T24, T26–T67, T39a
- parallel-safe with: none
- files: none
- inputs: —
- output: clean build and passing tests.
- verify: `rm -rf build && cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
- notes: 2026-10-08: verify command run (`rm -rf build`, configure, build, ctest): clean build,
  no warnings, all 11 test executables pass. Marked done after T24 and T53 (only PLAN.md
  changed since that run). **Milestone 1 complete.**

---

# Milestone 2 — Google account alongside Yahoo (OAuth2)

- **Goal**: The app holds several accounts at once. The user adds a Gmail account next to Yahoo by
  signing in with Google in the browser (OAuth2, no password stored); both accounts show in the
  sidebar with their folders, All Inboxes / Unread / Flagged merge both inboxes, and both
  reconnect at startup without asking (Gmail via a stored refresh token).
- **Out of scope**: Outlook, sending mail, reordering accounts (milestone 5), Gmail threads and
  labels (X-GM-THRID/X-GM-LABELS), IDLE, Google app passwords. Never print or log access or refresh
  tokens; never commit the OAuth client ID/secret (they live in the user's plasmamailrc or env).
- **Global verify**: `rm -rf build && cmake --preset dev && cmake --build --preset dev && ctest --preset dev`
- **Decisions (user, 2026-10-08)**: Google is added **alongside** Yahoo (multi-account now, not
  replace); sign-in is **OAuth2 browser login** (not an app password).
- **Design decisions** (made while planning; change only with the user):
  - An account's id is its email address (already the keychain key). Accounts keep the order
    they were added in; the sidebar shows them in that order.
  - For OAuth accounts the keychain entry under the email holds the **refresh token** instead of
    a password. Access tokens live only in memory and are refreshed before they expire.
  - Each account gets its own local store in `<AppData>/accounts/<sha1(email)>/`. The existing
    single store (`<AppData>/mail.sqlite` + `messages/`) is moved into the first account's folder.
    Storage settings stay global and apply to every account.
  - Gmail: All Mail (`\All`), Starred (`\Flagged`) and Important (`\Important`) are not stored
    locally; they repeat mail from other folders. They still open from the sidebar.
  - Google sign-in: authorization code flow with PKCE (S256), redirect to
    `http://127.0.0.1:<random port>/`, scope `https://mail.google.com/`, `access_type=offline`,
    `prompt=consent` (so a refresh token is always returned), `login_hint=<email>`.
- **Open questions**:
  - **Q4**: Has the user created a Google OAuth client? Steps: console.cloud.google.com → new
    project → "Google Auth Platform" / OAuth consent screen: External, app name "Plasma Mail",
    add their Gmail address as a test user (or publish "In production", unverified, fine for
    personal use) → Clients → Create client → type **Desktop app** → copy client ID and secret
    into `~/.config/plasmamailrc`:
    `[GoogleOAuth]` `ClientId=…` `ClientSecret=…` (or env `PLASMAMAIL_GOOGLE_CLIENT_ID` /
    `PLASMAMAIL_GOOGLE_CLIENT_SECRET`). Needed by T80 and T95 (manual checks only).

### T68 — `Account`: several accounts in the config
- status: done
- depends on: none
- parallel-safe with: T70, T72, T74
- files: src/engine/account.h, src/engine/account.cpp
- inputs: today's single `[Account]` group (`Account::load/save/remove`), KConfig `plasmamailrc`.
- output: field `QString authMethod` ("password" or "oauth"; "password" when missing);
  `static QList<Account> loadAll()` (order kept; group `[Accounts]` key `Order=<email list>`, one
  group `[Account <email>]` each); `static void saveAll(const QList<Account>&)`. If only the old
  `[Account]` group exists, `loadAll()` returns it as the single account and `saveAll` deletes
  the old group. `load/save/remove` stay (callers move in T85) but are marked deprecated in comments.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Done 2026-10-08. Groups are `[Account <email>]`, order in `[Accounts] Order`. An
  empty or missing Order falls back to the old `[Account]` group. Shared `readAccount` /
  `writeAccount` helpers also serve the deprecated `load()/save()`, so `AuthMethod` is
  written there too. Groups listed in Order but missing are skipped. Builds; all 11 tests pass.

### T69 — Tests for several accounts in the config
- status: done
- depends on: T68
- parallel-safe with: T70–T75
- files: autotests/accounttest.cpp
- inputs: T68 API; the test already uses `QStandardPaths::setTestModeEnabled(true)`.
- output: tests: save two accounts → loadAll returns both in order with all fields incl.
  authMethod; an old `[Account]` group migrates and disappears after saveAll; empty config →
  empty list; saving one account less removes its group.
- verify: `ctest --test-dir build -R accounttest --output-on-failure`
- notes: Done 2026-10-08. 4 new tests (empty, order + fields incl. authMethod, dropped
  accounts' groups removed, migration of the old `[Account]` group). The test now turns on
  test mode once in `initTestCase` (before any openConfig) and empties every config group in
  `init()`, because the test config (~/.qttest/config/plasmamailrc) survives between runs.
  13/13 pass, also on a second run; the user's real ~/.config/plasmamailrc is untouched.

### T70 — Gmail folder roles and which folders are stored
- status: done
- depends on: none
- parallel-safe with: T68, T72, T74
- files: src/engine/folderutils.cpp, src/engine/syncmanager.cpp
- inputs: `folderRoleFromFlags` (maps `\\all`, `\\flagged` already), `SyncManager::isChosen`.
- output: `\\important` maps to role "important" (sorted after "all"); `isChosen` also skips
  "all", "flagged" and "important".
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Done 2026-10-08. Also updated the role list and sort-order comments in
  folderutils.h and the isChosen comment in syncmanager.h. "flagged" is skipped for any
  server, since a `\Flagged` folder is always virtual. "important" has no icon of its own in
  FolderModel yet (default folder icon). Builds; all 11 tests pass.

### T71 — Tests for Gmail folder roles
- status: done
- depends on: T70
- parallel-safe with: T68, T69, T72–T75
- files: autotests/folderutilstest.cpp
- inputs: T70.
- output: tests: Gmail-style LIST flags (`[Gmail]/All Mail` `\\All`, `[Gmail]/Important`
  `\\Important`, `[Gmail]/Starred` `\\Flagged`) get roles all/important/flagged; `isChosen` is
  false for those three, trash and junk, true for inbox/sent/drafts/archive/plain folders.
- verify: `ctest --test-dir build -R folderutilstest --output-on-failure`
- notes: Done 2026-10-08. 6 Gmail rows in testFolderRoleFromFlags (All Mail, Important,
  Starred, Sent Mail, Spam, a plain label; each with `\HasNoChildren` too), a sort test (all →
  important → junk → trash), and testWhichFoldersAreStored (10 roles). folderutilstest
  37/37; all 11 test executables pass.

### T72 — Google OAuth client settings
- status: done
- depends on: none
- parallel-safe with: T68, T70, T74
- files: src/engine/oauthsettings.h, src/engine/oauthsettings.cpp (+ one line in src/CMakeLists.txt)
- inputs: Q4's config keys and env vars.
- output: `struct GoogleOAuthSettings { QString clientId, clientSecret; bool isConfigured() const;
  static GoogleOAuthSettings load(); }`; env vars win over the config file.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Done 2026-10-08. Values are trimmed; an environment variable wins only when it is
  non-empty (a set but empty variable doesn't hide the config value). Builds; all 11 tests
  pass; new files clang-format clean.

### T73 — Test for Google OAuth client settings
- status: done
- depends on: T72
- parallel-safe with: T68–T71, T74, T75
- files: autotests/oauthsettingstest.cpp, autotests/CMakeLists.txt
- inputs: T72; test mode config.
- output: tests: empty → not configured; config values read; env vars override config.
- verify: `ctest --test-dir build -R oauthsettingstest --output-on-failure`
- notes: Done 2026-10-08. 6 tests: nothing set; from config (trimmed); both values needed;
  each env var wins on its own; empty/blank env vars don't hide config; env only. `init()`
  deletes the group and unsets both variables. 12/12 test executables pass.

### T74 — Link Qt NetworkAuth
- status: done
- depends on: none
- parallel-safe with: T68–T73
- files: CMakeLists.txt, src/CMakeLists.txt
- inputs: `qt6-networkauth` 6.11 is installed.
- output: `NetworkAuth` in the root `find_package(Qt6 …)`; `plasmamail_engine` links `Qt6::NetworkAuth`.
- verify: `cmake --preset dev && cmake --build --preset dev` succeeds.
- notes: Done 2026-10-09. `cmake --preset dev && cmake --build --preset dev` builds all
  targets. (Configure prints "no upstream / no remote origin" from the git-hook helper; harmless.)

### T75 — `OAuthClient`: Google sign-in in the browser
- status: done
- depends on: T72, T74
- parallel-safe with: T68–T71
- files: src/engine/oauthclient.h, src/engine/oauthclient.cpp (+ one line in src/CMakeLists.txt)
- inputs: Milestone 2 design (PKCE, loopback, scope, params); `QOAuth2AuthorizationCodeFlow`,
  `QOAuthHttpServerReplyHandler` (listen on 127.0.0.1, port 0), `QDesktopServices::openUrl`.
- output: `void signIn(const QString &email)`; signals `signedIn(QString accessToken, QString
  refreshToken, QDateTime expiresAt)` and `failed(QString message)` (readable, no tokens). The
  browser page after the redirect says the user can close it.
- verify: `cmake --build build --target plasmamail_engine` succeeds (live check in T80).
- notes: Done 2026-10-09. Callback host forced to 127.0.0.1 (Google's loopback redirect);
  a new signIn() cancels one still waiting. Emits failed() when the client isn't configured,
  the listener can't start, the user cancels (access_denied), on network/state errors, or when
  Google returns no refresh token. Only one signal per sign-in. Engine builds without warnings.

### T76 — `OAuthClient`: refresh the access token
- status: done
- depends on: T75
- parallel-safe with: T68–T71
- files: src/engine/oauthclient.h, src/engine/oauthclient.cpp
- inputs: T75; `QOAuth2AuthorizationCodeFlow::setRefreshToken/refreshTokens`.
- output: `void refresh(const QString &refreshToken)` emitting `accessTokenChanged(QString token,
  QDateTime expiresAt)` or `failed(QString)`; while a token is in use it refreshes on its own
  about 5 minutes before expiry and emits `accessTokenChanged` again. A revoked refresh token
  gives a failed() message that tells the user to sign in again.
- verify: `cmake --build build --target plasmamail_engine` succeeds (live check in T80).
- notes: Done 2026-10-09. Uses Qt's autoRefresh with a 5-minute lead time; `tokenChanged` is
  turned into one `accessTokenChanged` (queued, so the new expiry is already set). `invalid_grant`
  from Google gives "revoked or expired, sign in again". An empty token or missing client gives
  failed(). Engine builds without warnings; 12/12 tests pass (live check in T80).

### T77 — `ImapClient`: XOAUTH2 login
- status: done
- depends on: T68
- parallel-safe with: T69–T76
- files: src/engine/imapclient.h, src/engine/imapclient.cpp
- inputs: `Account::authMethod`; `KIMAP::LoginJob::XOAuth2` (needs the installed SASL plugin
  `/usr/lib/sasl2/libkdexoauth2.so`).
- output: an "oauth" account logs in with `XOAuth2` using the access token as password;
  `void setAccessToken(const QString&)` replaces the token for future sessions without
  dropping the current one. Login errors for OAuth say to sign in again, not "wrong password".
- verify: `cmake --build build --target plasmamail_engine` succeeds (live check in T80).
- notes: Done 2026-10-09. Account::authMethod "oauth" → XOAuth2, else ClearText as before.
  `setAccessToken()` only swaps the stored secret, so the open session keeps working; the next
  session logs in with the new token. 12/12 tests pass (live check in T80).

### T78 — Link the OAuth client into `imap-probe`
- status: done
- depends on: T76
- parallel-safe with: T77
- files: tools/CMakeLists.txt
- inputs: `imap-probe` is a console app; OAuth needs `QDesktopServices` (Qt6::Gui).
- output: `imap-probe` links what `OAuthClient` needs (engine already public-links Gui/NetworkAuth;
  confirm and adjust only if needed); `imap-probe` uses `QGuiApplication` only with `--google`.
- verify: `cmake --build build --target imap-probe` succeeds.
- notes: Done 2026-10-09. The engine already links Gui/NetworkAuth publicly; `imap-probe` now
  names Qt6::Gui too (it includes QGuiApplication). It makes a QGuiApplication only when
  `--google` is on the command line (`tools/imapprobe.cpp`), else a QCoreApplication as before.

### T79 — `imap-probe --google`
- status: done
- depends on: T76, T77, T78
- parallel-safe with: T81–T84
- files: tools/imapprobe.cpp
- inputs: env `PLASMAMAIL_EMAIL`, T72 settings; T75/T76/T77.
- output: `--google` signs in through the browser, then lists folders with roles and the newest
  10 INBOX headers like the Yahoo mode; then refreshes the token once and lists folders again
  over a new session. Prints only "signed in" / "token refreshed", never tokens.
- verify: build succeeds; live run is T80.
- notes: Done 2026-10-09. Forces provider gmail + authMethod oauth; no password asked. Order:
  sign-in → "Signed in" → folders → newest 10 INBOX headers → newest message → refresh →
  "Token refreshed" → folders over a new session → OK. The 60 s no-answer timer only starts
  after the browser sign-in. `--google --sync` is refused. Without a configured client it
  prints the "No Google OAuth client" error and exits 1 (checked); live run is T80.

### T80 — Manual check: Google sign-in with `imap-probe`
- status: done
- depends on: T79, Q4
- parallel-safe with: T81–T92
- files: none (record results; new tasks for bugs)
- inputs: the user's Gmail address and OAuth client (Q4); the user signs in in their browser.
- output: confirmed in notes: browser opens, consent works, folders incl. `[Gmail]/…` with
  roles, newest headers, refresh + second session work, no token in the output.
- verify: (manual, with the user) `PLASMAMAIL_EMAIL=… build/bin/imap-probe --google` exits 0.
- notes:
  Closed 2026-10-09: the user signed in to Gmail in the app (own OAuth client, after the T147 fix) and confirmed "gmail works" next to Yahoo. T80 was done through the app, not the `imap-probe --google` command.

### T81 — Move the old single store into an account folder
- status: done
- depends on: none
- parallel-safe with: T68–T80
- files: src/engine/messagestore.h, src/engine/messagestore.cpp
- inputs: store layout (`mail.sqlite` + `messages/` under the base dir; paths in rows are
  relative, so the files can move).
- output: `static QString accountDir(const QString &appDataDir, const QString &email)`
  (`<appData>/accounts/<sha1 hex of lower-cased email>`); `static bool moveLegacyStore(const
  QString &appDataDir, const QString &accountDir)`: moves `mail.sqlite` and `messages/` if they
  exist and the target has no store yet; true when there was nothing to do.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes: Done 2026-10-09. `mail.sqlite` goes first, then `messages/`; if the second rename
  fails the database is moved back. The store doesn't use WAL, so no side files exist. Nothing
  calls these yet (T83/T85 do).

### T82 — Tests for the store move
- status: done
- depends on: T81
- parallel-safe with: T68–T80
- files: autotests/messagestoretest.cpp
- inputs: T81.
- output: tests: a store with a message moves and opens in the new folder with its message and
  file; nothing to move → true, no folders created; target already has a store → nothing moved.
- verify: `ctest --test-dir build -R messagestoretest --output-on-failure`
- notes: Done 2026-10-09. 4 new tests: accountDir (case-insensitive, 40 hex chars); the move keeps
  message, preview data and .eml bytes; nothing to move → true, no `accounts/` created; existing
  account store → old files stay, account store unchanged. 12/12 test executables pass.

### T83 — `AccountSession`: one account's client, folders and store
- status: done
- depends on: T68, T81
- parallel-safe with: T69–T80
- files: src/app/accountsession.h, src/app/accountsession.cpp (+ one line in src/CMakeLists.txt)
- inputs: what `MailController` does today per account: `ImapClient`, `FolderModel`,
  `MessageStore`, `SyncManager`, folder list saving, `startSync`, usage (mailcontroller.cpp).
- output: QObject owning those for one `Account`; `connectWithSecret(QString)`, `folders()`,
  `client()`, `store()`, `folderModel()`, `storageUsedBytes()`; it saves folder lists and starts
  syncs itself when storage is on; signals forward the client's. Not used by the controller yet.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Done 2026-10-09. `AccountSession(account, storeDir, storageSettings)`; extra API beyond
  the plan: `showStoredFolders()`, `setStorageSettings()`, `applyStorageLimits()`, `clearStore()`,
  `startSync()`, `syncing()`, `storageActive()/storageReady()`. The caller computes `storeDir`
  (`MessageStore::accountDir`) and runs `moveLegacyStore` first (T85). Folder lists are saved and
  syncs started whenever the server's list arrives, also for an account still being verified —
  T90 must clear that store if verification fails. `foldersListed` is forwarded after the
  session has handled it. Builds, 12/12 tests pass; not used by MailController yet.

### T84 — `AccountListModel` for the sidebar and settings
- status: done
- depends on: T83
- parallel-safe with: T85
- files: src/models/accountlistmodel.h, src/models/accountlistmodel.cpp (+ CMake line)
- inputs: T83.
- output: list model over `AccountSession*` with roles `email`, `displayName`, `provider`
  (readable name), `server`, `folderModel` (QObject*); `setSessions(QList<AccountSession*>)`.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Done 2026-10-09. The model lives in src/models/ but is compiled into the `plasma-mail`
  target (not `plasmamail_models`), because AccountSession is in the app target. Sessions stay
  owned by the caller; a deleted session drops out of the list. Extras: `session(row)` and the
  static `providerName()` (T85 should make MailController::accountProvider use it).

### T85 — `MailController`: run every saved account through `AccountSession`
- status: done
- depends on: T83, T84
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T68 `loadAll`, T81 store move (first account only), T83, T84.
- output: one `AccountSession` per saved account, all connecting at startup (password accounts
  only for now); property `accounts` (AccountListModel); `hasAccount` = any account. The
  existing QML API keeps working against the **first** account (folderModel, selectFolder(path),
  openMessage(uid), setFlagged, storage) so the app behaves as before with one account.
- verify: build succeeds, all tests pass, `run it again` shows Yahoo exactly as before.
- notes: Done 2026-10-09 (user's "run it again" check still to do). One `AccountSession` per saved
  account (`m_sessions`), created at startup; the legacy store moves into the first account's folder
  first. QML API works on the first account ("UI session"); `folderModel` and `storageReady` now
  notify via `accountChanged` (the folder model is the UI session's). `accounts` property added.
  Storage settings go to every session; usage/syncing are sums/any. A new account is verified in
  a pending session, then (after the keychain write) **replaces** the saved account — T90 changes
  this to adding. `removeAccount()` removes the first account only (T90). OAuth accounts get a
  session but no connect yet (T92). Errors of non-UI sessions are prefixed with the email.
  Checked: build, 12/12 tests, and an offline run (`unshare -n`, offscreen) on a *copy* of the
  user's data: legacy store moved into `accounts/<sha1>/`, sidebar + All Inboxes from storage
  looked as before. The user's real data dir has not been touched yet by the new code.

### T86 — Messages carry their account
- status: done
- depends on: none
- parallel-safe with: T68–T84
- files: src/engine/imapclient.h, src/models/messagelistmodel.h, src/models/messagelistmodel.cpp
- inputs: `MessageSummary`, `MessageListModel` (rows keyed by uid).
- output: `MessageSummary::account` (email); model role `account`; `message(account, uid)`,
  `setUnread(account, uid, …)`, `setFlagged(account, uid, …)` (uid alone is not unique across
  accounts). Old uid-only overloads removed; callers updated in T87.
- verify: `cmake --build build --target plasmamail_models` succeeds.
- notes: Done 2026-10-09. `MessageSummary::account` is set by ImapClient for headers and raw
  fetches, and by the controller for stored messages (`uiEmail()`); the store itself doesn't keep
  it (one store per account). The old uid-only overloads are gone; callers (MailController and
  the existing messagelistmodeltest, which now uses account me@example.org) were updated here,
  not in T87, so the build stays green. 12/12 tests pass.

### T87 — Tests for messages from two accounts
- status: done
- depends on: T86
- parallel-safe with: T68–T85
- files: autotests/messagelistmodeltest.cpp
- inputs: T86.
- output: tests: two accounts with the same uid stay separate rows; setUnread/setFlagged change
  only the row of the given account; the `account` role returns the email.
- verify: `ctest --test-dir build -R messagelistmodeltest --output-on-failure`
- notes: Done 2026-10-09. Added testSameUidInTwoAccounts (two rows with uid 7 stay separate, the
  `account` role and `message(account, uid)` tell them apart, unknown account → empty) and
  testChangesOnlyTouchTheGivenAccount (setUnread/setFlagged change one row, one signal; an
  unknown account changes nothing); the role-name test checks "account".

### T88 — `MailController`: folders and messages per account
- status: done
- depends on: T85, T86
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T85, T86.
- output: property `currentAccount`; `selectFolder(account, path)`, `openMessage(account, uid)`,
  `setFlagged(account, uid, flagged)`; every request goes to that account's session; summaries
  get `account` set. Old one-argument versions removed (QML updated in T93/T94).
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes: Done 2026-10-09. New: `currentAccount` property (empty for a shared view, which still
  reads the first account until T89), `selectFolder(account, path)`, `openMessage(account, uid)`,
  `setFlagged(account, uid, flagged)`; each goes to that account's session, and the answers
  (`headersFetched`, `messageFetched`) are matched to the session they came from. **Deviation:**
  the old one-argument versions are kept as thin wrappers (they act on the open account) so the
  current QML keeps working; T93/T94 switch QML to the new ones and remove the wrappers.
  A set `m_awaitingFolders` tracks which sessions have a folder-list request in flight, so
  `busy` can't stick when the user changes account meanwhile. `folderModel`/`accountEmail` etc.
  still describe the first account (T93/T96 use `accounts`). Checked: build, 12/12 tests,
  offline run on a copy of the user's data (list, stored message opens, toolbar as before).

### T89 — `MailController`: shared views merge every account
- status: done
- depends on: T88
- parallel-safe with: none
- files: src/app/mailcontroller.cpp
- inputs: T88; shared views today read the first inbox (`imapFolderFor`).
- output: All Inboxes / Unread / Flagged ask every account's inbox (and stored copies first),
  merge newest date first, keep the newest `messageLimit`; a late answer from one account
  re-merges without dropping the other's rows.
- verify: build succeeds; manual in T95.
- notes: Done 2026-10-09. Shared views keep one part per account (`m_sharedParts`); each answer
  (stored copies first, then the server's) replaces only its account's part, then the parts are
  merged (newest first, undated last, `messageLimit` rows). An account joins the view when its
  folder list arrives (`fetchSharedPart`, once per selection); refresh on a shared view re-lists
  every account's folders and each answer re-asks. Accounts that are not connected yet are
  skipped (their `folders()` is empty). Checked offline on a copy of the user's data with a
  made-up second account whose store is a copy of Yahoo's: both stores merged into one list.
  **Known until T94:** the list highlight and `openMessage(uid)` use the uid only, so in a merged
  list two accounts' messages with the same uid both look selected and the one-argument
  `openMessage` opens the first account's copy. T94 passes the row's account.

### T90 — `MailController`: add and remove accounts
- status: done
- depends on: T88
- parallel-safe with: T89
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T85; today's `setupAccount` replaces the account and clears the store.
- output: `setupAccount(...)` (password) verifies and **adds** an account (an existing email is
  refused); `removeAccount(account)` removes only that account: session, keychain entry, its
  store folder, config entry. Other accounts keep running.
- verify: build succeeds; manual in T95.
- notes: Done 2026-10-09. `setupAccount` refuses an email that is already added
  ("… is already added.") and otherwise appends the verified account after the keychain write; the
  first account resets the view as before, later ones just join an open shared view.
  `removeAccount(account)` removes only that account (session, keychain entry, store folder,
  config entry); if the open folder or message was its, the view goes to All Inboxes (or empties
  with no accounts), otherwise a shared view is re-merged without it. The no-argument
  `removeAccount()` is kept (removes the first account) until T96. Build and 12/12 tests pass;
  not exercised at runtime yet (the UI can't add a second account until T96/T97) — covered by T95.

### T91 — `MailController`: storage over all accounts
- status: done
- depends on: T88
- parallel-safe with: T89, T90
- files: src/app/mailcontroller.cpp
- inputs: T83 (per-session store/sync).
- output: `storageUsedBytes` = sum over accounts; `syncing` = any session syncing;
  `cleanUpNow` and `saveStorageSettings` apply to every session; `storageReady` = all stores open.
- verify: build succeeds; all tests pass.
- notes: Done 2026-10-09. Already in place from T85: `storageUsedBytes` sums the sessions,
  `syncing` is true when any session syncs, `saveStorageSettings` and `cleanUpNow` go to every
  session, `storageReady` needs every store open. Added here: a newly added account gets the
  current storage settings when it joins (they may have changed while it was being verified).
  Build and 12/12 tests pass.

### T92 — `MailController`: Google accounts (sign-in, startup, refresh)
- status: done
- depends on: T76, T77, T90
- parallel-safe with: T93, T94
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp (may use src/app/accountsession.cpp)
- inputs: T75/T76 `OAuthClient`, T77 `setAccessToken`, T72 `isConfigured`.
- output: `setupGoogleAccount(email, displayName)`: browser sign-in → verify by listing folders →
  refresh token into the keychain → account saved with authMethod "oauth". At startup an "oauth"
  account reads its refresh token, gets an access token, connects; new tokens go to the client.
  Property `googleConfigured` for the setup page.
- verify: build succeeds; manual in T95.
- notes: Done 2026-10-09. The OAuth work sits in `AccountSession` (`signInWithGoogle()`,
  `connectWithRefreshToken()`, `refreshToken()`; it owns an `OAuthClient` for Google accounts; the
  first access token connects, later ones go to `ImapClient::setAccessToken`). Controller:
  `setupGoogleAccount(email, displayName)` (Gmail preset, authMethod oauth; refused when the
  client isn't configured or the email exists), `cancelSetup()` (extra: the browser wait has no
  timeout, so the setup page needs a Cancel), property `googleConfigured` (CONSTANT, read at
  startup: set the client, then restart the app). The keychain entry of a Google account holds the
  refresh token; at startup it is read and used for the refresh. A revoked token shows the
  "sign in again" message; there is no re-sign-in button yet (would be a new task). Build and 12/12
  tests pass; offline start unchanged; the live flow is T80/T95.

### T93 — Sidebar: one section per account
- status: done
- depends on: T88
- parallel-safe with: T94, T96
- files: src/qml/FolderSidebar.qml
- inputs: `MailController.accounts` (T84), `selectFolder(account, path)`, `currentAccount`.
- output: a Repeater over accounts, each with its own header (avatar, email, expand arrow) and
  its folders; clicking a folder selects it for that account; highlight checks account + path.
- verify: offscreen start prints no new QML warnings; `run it again` reviewed by the user.
- notes: Done 2026-10-09 (user review pending). A Repeater over `MailController.accounts`; each
  section has its own expand state, header (avatar, email) and folder list; `FolderRow` got an
  `account` property (empty for the shared views): click → `selectFolder(account, path)`,
  highlight = path and account match. Checked offscreen/offline on a copy of the user's data with
  a made-up second account: two sections with their folders, no new QML warnings (the two
  "not placed in the graphics scene" ones were there before).

### T94 — Message list and message view pass the account
- status: done
- depends on: T88
- parallel-safe with: T93, T96
- files: src/qml/MessageListPage.qml, src/qml/MessagePage.qml
- inputs: model role `account` (T86), T88 API, `MailController.currentMessageAccount` (add in
  T88 if missing).
- output: rows open/flag with `(account, uid)`; the selected row checks account + uid; the
  message view flags with its message's account.
- verify: offscreen start prints no new QML warnings.
- notes: Done 2026-10-09. Added `MailController.currentMessageAccount` (it was not in T88). The
  list rows open and flag with their `account` role; a row is selected only when uid and account
  match; the message view flags with the open message's account. The one-argument `openMessage`
  and `setFlagged` are removed (the screenshot hook in main.cpp now passes the row's account).
  Checked offscreen/offline with the made-up two-account copy: no new QML warnings, and of the two
  same-uid rows only the opened one is highlighted. 12/12 tests pass. The one-argument `selectFolder` is gone too (internal calls pass an empty account for shared views).

### T96 — Settings → Accounts lists every account
- status: done
- depends on: T90
- parallel-safe with: T93, T94
- files: src/qml/AccountSettingsPage.qml, src/qml/AccountDetailsPage.qml
- inputs: `MailController.accounts`; `removeAccount(account)`.
- output: one row per account; "Add Account…" opens the setup page as a layer; the details
  page shows the chosen account and removes only it.
- verify: offscreen start prints no new QML warnings; manual in T95.
- notes: Done 2026-10-09 (user review pending). Settings → Accounts has a Repeater over
  `MailController.accounts` (new model role `authMethod`); a row pushes the details page with that
  row's values (`email`, `displayName`, `provider`, `server`, `authMethod` — the page no longer
  reads the first account); "Remove Account" calls `removeAccount(email)` and pops the layer
  (Main.qml still closes the settings when it was the last account). "Add Account…" is enabled and
  pushes `AccountSetupPage` with `asLayer: true` (title "Add Account"; it pops itself on
  `accountChanged`; leaving it calls `cancelSetup()`). Main.qml now rebuilds the page stack on
  `accountChanged` only when having-an-account flips, so adding a second account keeps the open
  mail view. Dev hook: `PLASMAMAIL_SCREENSHOT_SETTINGS=<module id>` opens a settings page before
  the screenshot. Checked offscreen/offline with the made-up two-account copy: the Accounts page
  lists both accounts and Add Account…, no new QML warnings. **Not run yet:** the details page, the
  remove dialog and the Add Account layer (need clicks) — covered by T95. qmllint is unusable
  here (it can't find the Kirigami imports).

### T97 — Setup page: Sign in with Google
- status: done
- depends on: T92
- parallel-safe with: T93, T94, T96
- files: src/qml/AccountSetupPage.qml
- inputs: `setupGoogleAccount`, `googleConfigured`.
- output: Gmail enabled in the provider list; for Gmail the password field is replaced by a
  "Sign in with Google" button and a note that the browser opens; if not configured, a note
  points to the `[GoogleOAuth]` settings (Q4) instead. Usable both first-run and from "Add Account…".
- verify: offscreen start prints no new QML warnings.
- notes: Done 2026-10-09. Gmail is enabled in the provider list. For Gmail the password field and
  the Yahoo hint are hidden, a note explains the browser sign-in, and the button reads "Sign in
  with Google…" (calls `setupGoogleAccount(email, name)`). Without a configured client the note is
  red and says to add ClientId/ClientSecret to `[GoogleOAuth]` and restart; the button is
  disabled. Extra: a Cancel button next to the busy indicator (`cancelSetup()`), for every
  provider. Checked offscreen with an empty config and a temporary Gmail default (reverted): the
  not-configured state looks right; no new QML warnings (the "not placed in the graphics scene"
  line is the old one). The configured state and the real sign-in are T80/T95.

### T95 — Manual end-to-end check: Gmail next to Yahoo
- status: done
- depends on: T80, T89, T91, T93, T94, T96, T97
- parallel-safe with: none
- files: none (record results; new tasks for bugs)
- inputs: the user's Gmail and Yahoo accounts, Q4 client.
- output: confirmed in notes: Add Account → Gmail → browser sign-in → Gmail section appears
  under Yahoo with its folders; All Inboxes shows both, newest first; opening and flagging work
  in each account; restart reconnects both without a browser; with storage on, Gmail's All
  Mail/Starred/Important are not stored; removing Gmail leaves Yahoo untouched; removing the
  last account returns to the setup page (moved here from T24 by the user).
- verify: manual, with the user.
- notes:
  Closed 2026-10-09: the user signed in to Gmail in the app (own OAuth client, after the T147 fix) and confirmed "gmail works" next to Yahoo. T80 was done through the app, not the `imap-probe --google` command.

### T98 — Global verify (Milestone 2)
- status: done
- depends on: T68–T97
- parallel-safe with: none
- files: none
- inputs: —
- output: clean build and passing tests.
- verify: `rm -rf build && cmake --preset dev && cmake --build --preset dev && ctest --preset dev`
- notes: 2026-10-09: the verify command ran clean: `rm -rf build`, configure, full build (no
  warnings, no errors), 12/12 test executables pass. **Stays `todo`**: it depends on T80 and T95,
  the two manual checks that need the Google client (Q4). Run it once more after them.
  2026-10-09, final run (user closed the app first): `rm -rf build`, configure, full build, 17/17 tests pass. The build showed one compiler warning (deprecated `QRegularExpression::match` on a temporary `QStringView`, from T163 in `messagerenderer.cpp`); fixed in the same commit with `matchView`, rebuilt without warnings, 17/17 pass. Google sign-in and Gmail next to Yahoo confirmed by the user (T80, T95).

---

# Milestone 3 — Message actions (user request 2026-10-09: "enable all the action buttons")

- **Goal**: every button in the message toolbar works: Mark as Unread, Archive, Delete, Move to
  Folder, Label, Reply, Reply All, Forward; plus the Search field. Flag already works.
- **Decision (user, 2026-10-09)**: all of them. This **lifts the milestone-1 rule** "the app never
  deletes or moves mail on the server": moving to Archive/Trash/another folder is now allowed,
  always through a move (never permanent deletion; "Delete" = move to the Trash folder).
  Sending mail (formerly milestone 4) is pulled into this milestone because Reply/Forward need it.
- **Order**: runs **after Milestone 2** (it needs T86's account-aware messages and T88's
  per-account folders). Can be moved earlier if the user says so.
- **Out of scope**: permanent deletion / emptying Trash, drafts saved on the server, attachments
  in new messages, rich-text composing, signatures, Outlook, Gmail threads.
- **Global verify**: as milestone 2.
- **Design decisions** (made while planning; change only with the user):
  - Every action works on the open message of the **account it belongs to** (T86).
  - Server first, UI after: the list row disappears / changes only when the server accepted the
    command; on failure an error toast shows and nothing changes locally.
  - Move = IMAP `UID MOVE` when the server lists MOVE, else `UID COPY` + `\Deleted` + `UID EXPUNGE`
    (UIDPLUS) — never a plain EXPUNGE, which would remove other messages marked deleted.
  - Archive: the account's `archive` folder; Gmail: its All Mail (`all`) — moving from INBOX
    there is Gmail's archive. Delete: the `trash` folder. If the folder doesn't exist the button
    is disabled with a tooltip saying why.
  - After Archive/Delete/Move the next message in the list opens (or none if it was the last).
  - Local store follows the server: moved/deleted messages are removed from the store; the
    target folder's copy arrives with the next sync.
  - Reply/Forward: plain-text body (quoted for replies), `In-Reply-To`/`References` set,
    "Re:"/"Fwd:" subjects without doubling, From = the message's account.
  - Sending uses KSMTP with the same login as IMAP (password, or XOAUTH2 for Google). After
    sending, a copy is appended to the Sent folder, except for Gmail, which does it itself.
- **Open questions**:
  - **Q5**: What should "Label" do for Yahoo (no labels)? Proposal: Gmail only (X-GM-LABELS);
    for other accounts it uses IMAP keywords if the server allows them, else is disabled with a
    tooltip. Needed by T118.

### T99 — `ImapClient`: mark a message unread
- status: done
- depends on: none
- parallel-safe with: T100–T102
- files: src/engine/imapclient.h, src/engine/imapclient.cpp
- inputs: `markSeen` (existing StoreJob usage).
- output: `void setSeen(const QString &folder, qint64 uid, bool seen)`; `markSeen` calls it.
- verify: `cmake --build build --target plasmamail_engine`.
- notes: Done 2026-10-09. `setSeen(folder, uid, seen)` is the old `markSeen` body with
  AppendFlags/RemoveFlags by `seen`; `markSeen` calls it with true. Builds without warnings,
  12/12 tests pass; the live check is T107.

### T100 — Mark as Unread / Read in the controller and toolbar
- status: done
- depends on: T99
- parallel-safe with: T101, T102
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp, src/qml/MessagePage.qml
- inputs: T99; `setFlagged` as the pattern.
- output: `Q_INVOKABLE void setUnread(qint64 uid, bool unread)` (list, store, server);
  `messageUnread` property; the button toggles "Mark as Unread" / "Mark as Read"; it does not
  re-mark the message read while it stays open.
- verify: build; offscreen start has no new QML warnings.
- notes: Done 2026-10-09. `setUnread(account, uid, unread)` (account-aware, like `setFlagged`)
  updates the list, the stored copy and `messageUnread`, then calls `setSeen` on the server.
  The toolbar's third button is now a toggle (envelope icon "Mark as Unread" / "Mark as Read",
  tooltip) and is enabled. An unread open message stays unread: only opening a message marks it
  read. Checked offscreen/offline: the button is enabled with a message open, no new warnings;
  the server round trip is T107. Build and 12/12 tests pass.

### T101 — `ImapClient`: move a message
- status: done
- depends on: none
- parallel-safe with: T99, T100, T102
- files: src/engine/imapclient.h, src/engine/imapclient.cpp
- inputs: `KIMAP::MoveJob`, `CopyJob`, `StoreJob`, `ExpungeJob`, `CapabilitiesJob` (MOVE, UIDPLUS).
- output: `void moveMessage(const QString &from, qint64 uid, const QString &to)`; signal
  `messageMoved(from, uid, to)` or `errorOccurred`; the fallback from the design decisions.
- verify: build. Live check in T107.
- notes: Done 2026-10-09. `moveMessage(from, uid, to)` + signal `messageMoved(from, uid, to)`.
  The server's capabilities are read once per session (`m_capabilities`, cleared when the session
  drops). With MOVE: `MoveJob`. Without: COPY, flag the original `\Deleted`, SEARCH DELETED, and
  EXPUNGE **only if our uid is the only deleted one** — KIMAP's `ExpungeJob` is a plain EXPUNGE
  (no UID EXPUNGE), so the plan's "UID EXPUNGE" wasn't possible; in the unsafe case the flag is
  taken back, the copy stays, and `errorOccurred` explains. Builds without warnings, 12/12 tests
  pass; nothing calls it yet and nothing was run against a server (T107).

### T102 — Folder lookup for Archive and Delete
- status: done
- depends on: none
- parallel-safe with: T99–T101
- files: src/engine/folderutils.h, src/engine/folderutils.cpp, autotests/folderutilstest.cpp
- inputs: `FolderInfo::role`.
- output: `QString folderForRole(const QList<FolderInfo>&, const QString &role)` (empty if none)
  and `archiveTarget(...)` (archive, else all for Gmail) with tests.
- verify: `ctest --test-dir build -R folderutilstest --output-on-failure`
- notes: Done 2026-10-09. `folderForRole(folders, role)` (first match, else empty) and
  `archiveTarget(folders)` (archive, else all, else empty); the trash folder is just
  `folderForRole(…, "trash")`. 2 new tests (first of several, Yahoo-like, Gmail-like, both,
  neither/empty).

### T103 — `MessageStore`: remove one message
- status: done
- depends on: none
- parallel-safe with: T99–T102
- files: src/engine/messagestore.h, src/engine/messagestore.cpp, autotests/messagestoretest.cpp
- inputs: existing remove functions.
- output: `bool removeMessage(const QString &folder, qint64 uid)` (row and .eml); tests.
- verify: `ctest --test-dir build -R messagestoretest --output-on-failure`
- notes: Done 2026-10-09. `removeMessage(folder, uid)`: true when the message is gone afterwards
  (also when it was never stored), false on error / closed store; uses `removeStored`, so row and
  file go together. 3 new tests: row and file removed while the same uid in another folder and
  other messages stay; unknown message/folder is fine; closed store → false with an error text.

### T104 — Controller: Archive, Delete, Move
- status: done
- depends on: T101, T102, T103, T88
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T101–T103; the account of the open message.
- output: `archiveMessage()`, `deleteMessage()`, `moveMessage(folderPath)` on the open message;
  properties `canArchive`, `canDelete`; on `messageMoved` the row and store copy go and the next
  message opens; on error the list stays and `errorOccurred` fires.
- verify: build; all tests pass.
- notes: Done 2026-10-09. `archiveMessage()`, `deleteMessage()`, `moveMessage(folderPath)` act on the
  open message of its own account; properties `canArchive` / `canDelete` (folder exists, message
  not in it already, not already moving). The move goes to the server first (`m_moving` keys);
  on `messageMoved` the row, its stored copy and its part of a shared view go, and the next row
  (else the previous, else nothing) opens. A session error drops that account's pending moves and
  frees `busy`. Extras beyond the plan's files: `MessageListModel::removeMessage()` /
  `messageAt()` (+1 test in messagelistmodeltest) and `AccountSession::messageMoved` forwarding.
  Not run against a server (T107); storage usage in Settings refreshes with the next sync, not
  right after a move. Build and 12/12 tests pass.

### T105 — Toolbar: Archive, Delete, Move to Folder
- status: done
- depends on: T104
- parallel-safe with: T100
- files: src/qml/MessagePage.qml
- inputs: T104.
- output: Archive and Delete enabled (shortcuts E and Delete, tooltips), disabled with a reason
  when `canArchive`/`canDelete` is false; Move to Folder opens a menu of the account's folders.
- verify: offscreen start prints no new QML warnings.
- notes: Done 2026-10-09 (user review pending). Archive and Delete are real buttons bound to
  `canArchive`/`canDelete` with tooltips that say why they are unavailable (Delete says "Move to
  Trash" when enabled); Move to Folder opens a menu of the open message's account's folders
  (indented by depth, the current folder greyed) via new controller properties
  `messageFolderModel`, `currentMessageFolder`, `canMove`. Offline (no server folder list) all
  three are disabled, as seen in an offscreen/offline screenshot (stored folders don't count).
  No new QML warnings; clicking them needs a server (T107). Shortcuts come in T106.

### T106 — Toolbar: keyboard shortcuts and message-list context
- status: done
- depends on: T105
- parallel-safe with: none
- files: src/qml/MessagePage.qml, src/qml/MessageListPage.qml
- inputs: T100, T105.
- output: shortcuts shown in tooltips (Mark unread U, Archive E, Delete Del); a right-click menu
  on list rows with the same actions.
- verify: offscreen start prints no new QML warnings.
- notes: Done 2026-10-09 (user review pending). Shortcuts U (mark unread/read), E (archive), Delete
  (move to Trash) in `MessagePage` (disabled when the action isn't available; a focused text field
  takes the key instead); the tooltips show them. Right-clicking a list row opens that message
  and shows a menu: Mark as Unread/Read, Flag/Unflag, Archive, Delete, Move to Folder ▸ (the
  actions work on the open message, so right-click also marks it read once it has loaded — mark it
  unread from the menu if that is not wanted). New file `src/qml/MoveFolderMenu.qml` is shared by
  the toolbar and the context menu. Offscreen/offline start: no new warnings; right-click, the keys
  and the menu need the desktop / a server (T107).

### T107 — Manual check: Mark as Unread, Archive, Delete, Move
- status: done
- depends on: T100, T105
- parallel-safe with: none
- files: none (record results; new tasks for bugs)
- inputs: the user's accounts; use test messages the user picks (this changes the real mailbox).
- output: confirmed in notes: each action changes the server (visible in webmail), the list and
  the store; failures leave the message where it was.
- verify: manual, with the user.
- notes:
  Closed 2026-10-09: the user tried Mark as Unread, Archive, Move to Folder and Delete on test messages (build with T145) and said "all worked". The app log of that session had no errors.

### T108 — Link KSMTP
- status: done
- depends on: none
- parallel-safe with: T99–T107
- files: CMakeLists.txt, src/CMakeLists.txt
- inputs: `KPim6SMTP` is installed.
- output: `find_package(KPim6SMTP)` and `plasmamail_engine` links it.
- verify: `cmake --preset dev && cmake --build --preset dev`.
- notes: Done 2026-10-09. `find_package(KPim6SMTP REQUIRED)` next to the IMAP one; `plasmamail_engine`
  links `KPim6::SMTP` (the executable now loads libKPim6SMTP.so.6). Build without warnings;
  14/14 tests.

### T109 — `SmtpClient`: send a message
- status: done
- depends on: T108, T77
- parallel-safe with: T99–T107
- files: src/engine/smtpclient.h, src/engine/smtpclient.cpp (+ CMake line)
- inputs: `KSMTP::Session/LoginJob/SendJob`; account SMTP settings; password or XOAUTH2.
- output: `send(account, secret, from, to, cc, bcc, subject, body, inReplyTo, references)` →
  `sent()` / `failed(message)`; builds the message with KMime; encrypted connection only.
- verify: build. Live check in T115.
- notes: Done 2026-10-09 (nothing sent yet; live check is T115). Two classes instead of one:
  `OutgoingMessage` (src/engine/outgoingmessage.*; a struct with from, to/cc/bcc, subject, body,
  inReplyTo, references and `build(now)`) builds the RFC 5322 bytes with KMime — CRLF, 7-bit only,
  UTF-8 text as quoted-printable (KMime encodes the body itself; an own encoder double-encoded
  `=` as `=3D3D`, so there is none), encoded headers, Date, generated Message-ID on the sender's
  domain, MIME-Version, In-Reply-To / References, **no Bcc header** — and the bare envelope
  recipients (Bcc included). 10 tests (outgoingmessagetest: headers, recipients, no Bcc leak, body
  round trip with non-ASCII/emoji/long lines, 7-bit and CRLF, reply headers, no name, several
  addresses in one field). `SmtpClient` (smtpclient.*; KSMTP namespace is `KSmtp`, include path
  `<KSMTP/…>`): `send(account, secret, message)` → `sent()` / `failed(text)`; port 465 = TLS,
  else STARTTLS (never unencrypted); login with the password or XOAUTH2; one SendJob (`From` =
  the account's address); 60 s timeout; a second send while one runs fails at once; the secret
  is cleared when done; error texts never contain the password or a token. Build without
  warnings; 15/15 tests.

### T110 — Message builder and reply/forward text
- status: done
- depends on: none
- parallel-safe with: T99–T109
- files: src/engine/messagecomposer.h, src/engine/messagecomposer.cpp, autotests/messagecomposertest.cpp
- inputs: `KMime::Message`; `messagebodyplain` logic in messagerenderer.
- output: `replySubject/forwardSubject`, `quotedBody(message, header line)`, `replyRecipients(
  message, ownAddress, replyAll)`, `references(message)`; tests for "Re: Re:", reply-all without
  the own address, Reply-To, References chain.
- verify: `ctest --test-dir build -R messagecomposertest --output-on-failure`
- notes: Done 2026-10-09. `MessageComposer` namespace (src/engine/messagecomposer.*): `replySubject`
  / `forwardSubject` (keeps Re:/RE:/Re[2]: and Fwd:/Fw:; "Reservation" still gets "Re: "),
  `replyRecipients(original, ownAddresses, replyAll)` (Reply-To else From; a reply to one's own
  message goes to its To; reply all adds To and Cc without own addresses, nobody twice, compared
  case-insensitively), `plainBody` (text/plain, else the HTML as text), `quotedReply` ("On <date>,
  <name> wrote:" + "> "-quoted lines), `forwardedBody` (the "---------- Forwarded message ----------"
  block with From/Date/Subject/To), `threadIds` (In-Reply-To = the original's Message-ID, References
  = its References + the Message-ID, no repeats, without <>). 14 tests / 24 checks
  (messagecomposertest). KMime drops the body's final newline, so a forwarded body ends without
  one. 16/16 test executables pass.

### T111 — Controller: prepare Reply, Reply All, Forward
- status: done
- depends on: T110, T88
- parallel-safe with: T109
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T110; the open message and its account.
- output: `Q_INVOKABLE QVariantMap composeFor(const QString &kind)` ("reply", "replyAll",
  "forward") → {from, to, cc, subject, body, inReplyTo, references}.
- verify: build; all tests pass.
- notes: Done 2026-10-09. `composeFor(kind)` ("reply" | "replyAll" | "forward") returns a map:
  `kind`, `account` (the sending account = the open message's), `from` ("Name <address>" or just
  the address), `to`, `cc` (comma separated), `subject`, `body` ("\n\n" + the quoted text or the
  forwarded block, so the cursor can go on top), `inReplyTo`, `references`; empty when nothing is
  loaded. The controller now keeps the parsed open message (`m_openMessage`, set in `showMessage`,
  which takes a `shared_ptr` now; cleared with the message). Property `canCompose` (a message is
  open and loaded) for the buttons (T114). Build; 16/16 tests; not used by QML yet (T112/T114).

### T112 — Compose window: Send, prefilled
- status: done
- depends on: T109, T111
- parallel-safe with: none
- files: src/qml/ComposeWindow.qml, src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T109, T111; existing compose window (Send disabled).
- output: the window takes initial values; From picks an account; Send is enabled when there is a
  recipient and sends through the controller; shows progress, closes on success, keeps the text
  and shows the error on failure.
- verify: offscreen start prints no new QML warnings.
- notes: Done 2026-10-09 (no mail sent yet; live check T115). Controller: `sendMessage(map)`
  (account, to, cc, bcc, subject, body, inReplyTo, references) → signals `messageSent()` /
  `sendFailed(text)`, property `sending`; it uses the account's current IMAP secret (the
  password, or the access token — new `ImapClient::secret()`), so an account that is not
  connected yet answers "not connected yet, try again"; one send at a time; a missing recipient
  is reported before anything is built. `ComposeWindow.qml`: a From box (the accounts), To, Cc,
  **Bcc** (new), Subject, body; `initialAccount/To/Cc/Subject/Body/InReplyTo/References` properties
  for T114; the cursor goes above the quoted text on a reply; Send is enabled with a recipient,
  while sending the fields lock and "Sending…" with a busy indicator shows, success closes the
  window, failure shows the text in red and keeps everything; closing asks only when the fields
  differ from the starting values. Dev hook `PLASMAMAIL_SCREENSHOT_COMPOSE=new|reply`. Checked
  offscreen: the reply window looks right (From, To, Re: subject, cursor on top, quoted text);
  build without warnings; 16/16 tests.

### T113 — Copy of sent mail into the Sent folder
- status: done
- depends on: T112
- parallel-safe with: none
- files: src/engine/imapclient.h, src/engine/imapclient.cpp, src/app/mailcontroller.cpp
- inputs: `KIMAP::AppendJob`; the account's `sent` folder; Gmail saves its own copy.
- output: after a send, non-Gmail accounts append the sent message to Sent with `\Seen`.
- verify: build. Live check in T115.
- notes: Done 2026-10-09 (live check T115). `ImapClient::appendMessage(folder, bytes, flags)` (KIMAP
  `AppendJob`, internal date = now) → `messageAppended(folder)`. `SmtpClient::sent` now carries the
  message bytes as sent (no Bcc header); `MailController::saveSentCopy` appends them to the account's
  "sent" folder with `\Seen` after every successful send, except for Gmail accounts (Gmail files the
  copy itself); no Sent folder → a log line only. A failed append shows the usual error toast
  (the mail itself went out). Build without warnings; 16/16 tests.

### T114 — Toolbar: Reply, Reply All, Forward
- status: done
- depends on: T112
- parallel-safe with: T113
- files: src/qml/MessagePage.qml
- inputs: T111, T112.
- output: the three buttons enabled (shortcuts R, Shift+R, Shift+F), each opens the compose window
  prefilled.
- verify: offscreen start prints no new QML warnings.
- notes: First version (cbb7f80) imported ComposeWindow.qml as a module (fails), called open(),
  set the prefill after Component.onCompleted and had no shortcuts. Fixed: one `openCompose(kind)`
  helper using `Qt.createComponent("PlasmaMail", "ComposeWindow")` + `createObject(..., {initial*})`
  + `show()`; Shortcuts R / Shift+R / Shift+F. Build ok, ctest 16/16, offscreen start: no warnings
  from MessagePage. Click path is checked live in T115.

### T115 — Manual check: send, reply, forward
- status: done
- depends on: T113, T114
- parallel-safe with: none
- files: none
- inputs: the user's accounts; send to the user's own address first.
- output: confirmed: new message, reply, reply all, forward arrive and thread correctly; Yahoo
  shows a copy in Sent; Gmail has exactly one copy.
- verify: manual, with the user.
- notes:
  Closed 2026-10-09: the user tried send, reply, reply all and forward and said it passed.

### T116 — `ImapClient`: search
- status: done
- depends on: none
- parallel-safe with: T99–T115
- files: src/engine/imapclient.h, src/engine/imapclient.cpp
- inputs: `KIMAP::SearchJob`, existing `fetchHeaders`.
- output: `void search(const QString &folder, const QString &text)` → `headersFetched` for the
  matches (subject, from, body text).
- verify: build. Live check in T117.
- notes: UID SEARCH Or(Subject, From, Body), capped to the newest 200 matches (`SearchLimit`), then
  fetchSummaries → headersFetched(folder, AllMessages, …). Blank text emits an empty list at once.
  Build ok, ctest 16/16; no ImapClient unit test (no fake server), live check in T117.

### T117 — Search field in the toolbar
- status: done
- depends on: T116, T88
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp, src/qml/MessagePage.qml
- inputs: T116.
- output: the search field is enabled; typing + Enter lists matches of the current folder (all
  accounts for the shared views), Escape returns to the folder; the list header says "Search".
- verify: offscreen start prints no new QML warnings; manual with the user.
- notes: Code done 2026-10-09. Also touched src/qml/MessageListPage.qml (the list header lives
  there). MailController: search(text), clearSearch(), searchActive; per-session FIFO
  `m_inflight` {isSearch, generation} drops stale headersFetched replies (cleared on session
  error/discard/reconnect); shared views search every inbox (merge cap 200), Unread/Flagged filter
  the matches client-side; results never reach the store, unread counts or the new-mail tracker.
  Build ok, ctest 16/16, qmllint clean, offscreen start clean. Known limits: a non-fatal IMAP error
  clears the whole FIFO (may show one stale listing); m_requests can leak when a reconnect drops
  jobs (pre-existing). Waiting: user tries search in a folder, in All Inboxes, Escape returns.
  2026-10-09 user try-out: Yahoo matches whole words only (`osmand` finds the OsmAnd mails, `osm` finds nothing). Fixed: the search field searches on Enter only (`autoAccept: false`; it used to search after every keystroke); the three terms are now nested ORs (IMAP's OR takes two keys). Partial-word search would need a local substring search; not planned.
  Closed 2026-10-09 by the user after trying full-word search in All Inboxes.

### T118 — Label (needs Q5)
- status: skipped
- depends on: Q5, T104
- parallel-safe with: none
- files: to be set after Q5
- inputs: Q5's answer.
- output: to be set after Q5.
- verify: to be set after Q5.
- notes:
  Skipped 2026-10-09 on the user's decision (Q5 answered: Label does nothing for Yahoo, so there is nothing to build until a Gmail account needs it; Gmail labels via X-GM-LABELS can be planned then).

### T119 — Global verify (Milestone 3)
- status: done
- depends on: T99–T118
- parallel-safe with: none
- files: none
- inputs: global verify command.
- output: clean build, all tests pass, no new QML warnings.
- verify: `rm -rf build && cmake --preset dev && cmake --build --preset dev && ctest --preset dev`
- notes: T118 is skipped (user decision), so it does not block this task.
  2026-10-09, run after T160 with the app closed: `rm -rf build`, configure, full build with no compiler warnings, 17/17 tests pass; real lint (`all_qmllint`) 2 warnings (the documented baseline); offscreen start with a throw-away profile prints only the known `AccountSetupPage ... not placed in the graphics scene` line. The `error: No such remote 'origin'` line in the log is harmless (this repo has no remote). T118 was skipped and T129/T141 were skipped by the user, so the plan is complete.


# Milestone 3b — App icon, tray icon, new-mail notifications (user request 2026-10-09)

- **Goal**: Plasma Mail has its own icon/logo (window, launcher, tray, About page); a tray icon
  shows unread mail and lets the window be hidden to the tray; new mail triggers a desktop
  notification.
- **Decisions** (made while planning; change only with the user):
  - The logo is an SVG made by hand (a simple envelope on a rounded square with the Plasma
    "spark" colour, blue #3daee9 like the unread accent), installed as the themed icon
    `plasma-mail` (scalable + PNG sizes made with rsvg-convert); `plasma-mail.desktop` makes the
    app show in the launcher. Install rules are added (`cmake --install` works; the dev build uses
    the icon from the source tree).
  - Tray: `KStatusNotifierItem` (works on Plasma and other SNI trays): icon `plasma-mail`, a
    badge/overlay with the unread count, left click shows/hides the window, menu: Show, Check
    for Mail, Quit. Closing the window keeps the app running in the tray **only if** the user turns
    on "Keep running in the tray when the window is closed" (default off, saved in plasmamailrc).
  - New mail needs the app to look for it: this milestone adds a **background check every
    5 minutes** (all connected accounts re-list their INBOX); IDLE (push) stays for later. A
    message counts as new when its uid is above the highest uid seen for that account's inbox
    since the previous check, it is unread, and it is not older than the last check by more than a
    day (so the first start after a long time doesn't announce old mail).
  - Notification: `KNotification` (event id `newmail`, `plasma-mail.notifyrc` installed): one
    notification per account per check ("3 new messages in topke63@yahoo.com", or sender + subject
    for a single message); clicking it shows the window and opens that message. No notification
    while the window is focused. Notification settings use KDE's standard dialog (a "Configure
    Notifications…" row in Settings).
  - Never put message content into logs; the notification text is shown by the desktop only.
- **Out of scope**: IDLE, sounds of our own (KDE's notification settings handle them), badges in
  the launcher, per-folder notification rules, notifications for Gmail threads.

### T120 — App icon (logo)
- status: done
- depends on: none
- parallel-safe with: T121, T122
- files: data/icons/sc-apps-plasma-mail.svg (new)
- inputs: —
- output: a clean SVG logo (envelope on a rounded square, blue, readable at 16 px), checked by
  rendering 16/32/48/128 px PNGs with `rsvg-convert` into a scratch folder and viewing them.
- verify: the PNGs look right at all four sizes (looked at by me, then the user).
- notes: Done 2026-10-09 (user review pending). `data/icons/sc-apps-plasma-mail.svg`: white envelope
  with a blue flap on a rounded blue square (gradient #56b9f0 → #1d8fd8), soft shadow, 128×128
  viewBox, CC0 header. Rendered at 16/32/48/128 px on a light and a dark background and looked at:
  the envelope and flap are clear at all sizes. A first version had an orange unread dot; I took
  it out because a logo that always shows "unread" would mislead — T126 should make a separate
  `plasma-mail-unread` tray variant with the dot (add to its files).

### T121 — Install the icon and desktop file
- status: done
- depends on: T120
- parallel-safe with: T122
- files: CMakeLists.txt, src/CMakeLists.txt, data/plasma-mail.desktop (new)
- inputs: ECM `KDEInstallDirs`, `ecm_install_icons`.
- output: `ecm_install_icons` for the SVG; `plasma-mail.desktop` (Name, Comment, Exec, Icon=
  plasma-mail, Categories=Network;Email;Qt;KDE;) installed; `install(TARGETS plasma-mail)`.
  `QGuiApplication::setWindowIcon(QIcon::fromTheme("plasma-mail", QIcon(":/…")))` so the window
  and About page show it when run from the build folder (icon also added to the QML module as a
  resource).
- verify: `cmake --install build --prefix <a fresh scratch folder>` installs the icon, desktop
  file and binary; `desktop-file-validate` (if installed) passes; offscreen start prints no new warnings.
- notes: Done 2026-10-09. Root CMake: `ECMInstallIcons` → `ecm_install_icons` puts the SVG at
  `share/icons/hicolor/scalable/apps/plasma-mail.svg`; `data/plasma-mail.desktop` (Name, Comment,
  Exec, Icon, Categories=Qt;KDE;Network;Email;, Keywords) → `share/applications`; `install(TARGETS
  plasma-mail)` in src. The SVG is also a Qt resource (`:/icons/…`) so runs from `build/` show it:
  `main.cpp` sets the window icon and `KAboutData::setProgramLogo` from
  `QIcon::fromTheme("plasma-mail", resource)`. Checked: `cmake --install` into a scratch prefix
  installs the binary, icon and desktop file; `desktop-file-validate` passes; the About page
  (offscreen screenshot) shows the logo; 12/12 tests pass. To see it in the launcher / task
  manager the user must install (e.g. `cmake --install build --prefix ~/.local`); the "App info
  not found for 'plasma-mail'" portal notice should then go away. An unexplained `""` line
  appeared in the stderr of the About-page run (not seen with other pages).

### T122 — Settings: Keep running in the tray (storage of the option)
- status: done
- depends on: none
- parallel-safe with: T120, T121
- files: src/engine/appsettings.h, src/engine/appsettings.cpp, autotests/appsettingstest.cpp (+ CMake lines)
- inputs: `StorageSettings` as the pattern.
- output: `AppSettings` with `keepInTray` (bool, default false) and `checkMinutes` (default 5,
  1–60), load/save in plasmamailrc group `[General]`; tests: defaults, round trip, out-of-range
  clamped.
- verify: `ctest --test-dir build -R appsettingstest --output-on-failure`
- notes: Done 2026-10-09. `AppSettings { keepInTray = false; checkMinutes = 5 }` with `load()` /
  `save()` in `[General]` (`KeepInTray`, `CheckMinutes`), limits 1–60 applied on load and save
  (`minCheckMinutes` / `maxCheckMinutes`). 6 tests: defaults, round trip, clamping on load and on
  save, a non-number falls back to 5, saving leaves the other groups alone. (KConfig reads any
  other text as `true` for a bool, so there is no "garbage bool" test.) 13/13 test executables
  pass.

### T123 — Link KStatusNotifierItem and KNotifications
- status: done
- depends on: none
- parallel-safe with: T120–T122
- files: CMakeLists.txt, src/CMakeLists.txt
- inputs: `KF6StatusNotifierItem`, `KF6Notifications` are installed.
- output: both found in the root `find_package(KF6 …)`; `plasma-mail` links them.
- verify: `cmake --preset dev && cmake --build --preset dev`.
- notes: Done 2026-10-09. `Notifications` and `StatusNotifierItem` added to the root
  `find_package(KF6 …)`; `plasma-mail` links `KF6::Notifications` and `KF6::StatusNotifierItem`
  (the executable now loads libKF6Notifications and libKF6StatusNotifierItem). Both found as 6.30.0.
  Full build has no warnings; 13/13 tests pass.

### T124 — `NewMailChecker`: find new messages
- status: done
- depends on: T83
- parallel-safe with: T122, T123
- files: src/engine/newmailtracker.h, src/engine/newmailtracker.cpp, autotests/newmailtrackertest.cpp (+ CMake lines)
- inputs: `MessageSummary`; design rules above (highest uid seen, unread, not too old).
- output: pure logic class: `QList<MessageSummary> newMessages(account, folder, uidValidity,
  summaries, now)` and `markSeenUpTo`; remembers the highest uid per (account, folder,
  uidValidity); a UIDVALIDITY change or the first run records without announcing; tests for first
  run, second run with 2 new, read mail ignored, old mail ignored, UIDVALIDITY reset, two accounts.
- verify: `ctest --test-dir build -R newmailtrackertest --output-on-failure`
- notes: Done 2026-10-09. `NewMailTracker` (not "Checker"; no separate `markSeenUpTo`):
  `newMessages(account, folder, uidValidity, summaries, now)` + `forget(account)`; in-memory state
  per (account, folder). First call, and the first call after a real UIDVALIDITY change, only
  record. After that: uid above the highest seen, unread, not older than a day (a missing date
  counts as recent), newest uid first; everything fetched is recorded, so a read or old message
  is never announced later and a message that turns unread later isn't either. `uidValidity 0`
  means unknown (never counts as a change) — T125 can pass 0 because `headersFetched` carries no
  UIDVALIDITY. 10 tests (first run, 2 new newest-first and announced once, read mail, old mail and
  missing date, UIDVALIDITY reset, unknown value, two accounts/folders, forget, empty inbox).
  14/14 test executables pass.

### T125 — Background check every few minutes
- status: done
- depends on: T122, T124, T88
- parallel-safe with: T126
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T122 `checkMinutes`, T124; sessions already list folders and fetch headers.
- output: a timer (`checkMinutes`) asks every connected account for its INBOX headers (the
  newest 20) without touching the open view; results go through the tracker; signal
  `newMailArrived(account, count, sender, subject, uid)` (sender/subject only for count 1);
  property `unreadCount` (unread messages of all inboxes, from the stored/fetched headers) for the
  tray. A reconnect after errors is tried on the next tick.
- verify: build; manual in T129.
- notes: Done 2026-10-09. Engine: `ImapClient::checkInbox(folder, limit)` (SEARCH UNSEEN, headers of
  the newest `limit` unread) → signal `inboxChecked(folder, unreadTotal, newestUnread)`; it never
  emits `headersFetched`, so the open view is untouched (`fetchSummaries` got an optional `done`
  callback for this). Controller: a `QTimer` every `AppSettings::checkMinutes` (read at startup;
  T128 will change it live) plus one check right after an account's folders arrive (that first one
  records the baseline and counts unread); one check per account at a time; accounts that are not
  connected are skipped. `NewMailTracker` (uidValidity 0) decides what is new → signal
  `newMailArrived(account, count, sender, subject, uid)` (sender/subject only when count is 1).
  Property `unreadCount` = sum over inboxes, from the checks and adjusted locally when a message
  is read here, marked unread, or an unread message leaves the inbox. A failed background check is
  only logged (no toast every 5 minutes while offline); removing an account forgets its state.
  Unread-only fetching means the tracker's "highest uid" is the highest *unread* uid. Build
  without warnings; 14/14 tests pass; offline start unchanged; not run against a server (T129).

### T126 — Tray icon
- status: done
- depends on: T121, T122, T123
- parallel-safe with: T125
- files: src/app/trayicon.h, src/app/trayicon.cpp, src/app/main.cpp, data/icons/sc-apps-plasma-mail-unread.svg (new, logo + orange dot) (+ CMake line)
- inputs: `KStatusNotifierItem`, T122 `keepInTray`.
- output: `TrayIcon` object: icon `plasma-mail`, tooltip "Plasma Mail – N unread", overlay/state
  `NeedsAttention` while unread > 0; left click toggles the main window; menu Show, Check for
  Mail (`MailController.refresh()`), Quit. With `keepInTray` the window's close hides it instead
  of quitting (`Window.onClosing`); otherwise unchanged.
- verify: build; offscreen start prints no new warnings; manual in T129.
- notes: Done 2026-10-09 (user review pending). `TrayIcon` (src/app/trayicon.*) wraps a
  `KStatusNotifierItem`: logo icon, `plasma-mail-unread` (logo + orange dot, new
  `data/icons/sc-apps-plasma-mail-unread.svg`, installed and also a Qt resource) as the attention
  icon while `unreadCount > 0`, tooltip "N unread messages" / "No unread mail", the main window
  associated (a click shows/hides it), menu "Check for Mail" above the standard Restore/Quit.
  Before the app is installed the icons are sent as pictures from the resources. **Big change:**
  KStatusNotifierItem's menu is a QMenu, so `main.cpp` now uses `QApplication` and links
  `Qt6::Widgets`. `setKeepInTray(bool)` sets `quitOnLastWindowClosed` (read from `AppSettings` at
  startup; T128 makes it live). Screenshot runs skip the tray (`PLASMAMAIL_SCREENSHOT_TRAY=1`
  keeps it). Checked: build, 14/14 tests, the app starts with the tray without a D-Bus session
  (no crash), the window looks unchanged, the unread icon looked right at 16/22/32/128 px. The
  tray itself, the click behaviour and the menu need the desktop (T129).

### T127 — New-mail notification
- status: done
- depends on: T123, T125
- parallel-safe with: T126
- files: src/app/mailnotifier.h, src/app/mailnotifier.cpp, data/plasma-mail.notifyrc (new) (+ CMake lines)
- inputs: `KNotification`, `newMailArrived`.
- output: on `newMailArrived`, unless the window is active: a `newmail` notification (texts per the
  design decisions) with an "Open" default action → `MailController` shows that message (select
  the account's inbox, open uid) and raises the window.
- verify: build; manual in T129.
- notes: Done 2026-10-09 (not seen on a desktop yet). `MailNotifier` (src/app/mailnotifier.*):
  on `newMailArrived`, unless the window is the active one, a `KNotification` (event `newmail`,
  component `plasma-mail`, logo icon): one message → title = sender's name (address if no name),
  text = subject; several → "N new messages" + the account's email. Default action "Open" →
  `MailController::openNewMail(account, uid)` (new): selects that account's inbox, opens the
  message once the list has arrived (`m_pendingOpen*`), and emits `showMessageRequested` (Main.qml
  moves to the message column); the notifier also shows/raises the window. New
  `data/plasma-mail.notifyrc` installed to `KNOTIFYRCDIR` (the install into a scratch prefix
  was checked); before installing KDE has no event config, so it uses its defaults. No
  message text is logged. Build without warnings, 14/14 tests, offline start with tray + notifier
  unchanged. Not triggered: needs real new mail (T129).

### T128 — Settings: Notifications page
- status: done
- depends on: T122, T126, T127
- parallel-safe with: none
- files: src/qml/NotificationSettingsPage.qml (new), src/qml/Main.qml, src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T122 values; `KNotifyConfigWidget` is widgets-only, so the page has our own switches
  and a "Configure Notifications…" button that runs `systemsettings kcm_notifications` / the
  KDE per-app dialog via `KNotifyConfigWidget` in a small helper if available.
- output: a "Notifications" settings module: "Check for new mail every N minutes" (1–60), "Keep
  running in the tray when the window is closed", and the button above.
- verify: offscreen start with `PLASMAMAIL_SCREENSHOT_SETTINGS=notifications` prints no new
  warnings and shows the page.
- notes: Done 2026-10-09 (user review pending). New Settings module "Notifications"
  (`NotificationSettingsPage.qml`): a spin box "Check for new mail every (minutes)" 1–60, a switch
  "Keep running in the tray when the window is closed", and "Configure Notifications…" which opens
  KDE's `KNotifyConfigWidget` dialog for `plasma-mail` (links `KF6::NotifyConfig`; enabled only
  when `knotifications6/plasma-mail.notifyrc` is found, i.e. after `cmake --install`, otherwise
  greyed with a hint). Changes apply at once: `MailController::saveAppSettings()` saves
  `AppSettings`, restarts the check timer and emits `appSettingsChanged`; `main.cpp` connects that
  to `TrayIcon::setKeepInTray`. New controller properties `keepInTray`, `checkMinutes`,
  `notificationsConfigurable`. Checked offscreen/offline: the page renders (5, switch off, button
  greyed), no new QML warnings; build without warnings, 14/14 tests. Clicking the controls and the
  KDE dialog need the desktop (T129).

### T129 — Manual check: icon, tray, notification
- status: skipped
- depends on: T121, T126, T127, T128
- parallel-safe with: none
- files: none
- inputs: the user's desktop; send a mail to the user's own address from another account (or
  webmail) while the app runs.
- output: confirmed: icon in the window, launcher (after install) and tray; unread count in the
  tray tooltip; closing hides to the tray when enabled; a new mail gives one notification within
  the check interval; clicking it opens the message; no notification for old mail at startup.
- verify: manual, with the user.
- notes: 2026-10-09 run with the user (app installed into ~/.local; interval 1 min). Found and
  fixed two things: (1) the check could keep seeing an old snapshot of the selected inbox → a
  check now always sends a fresh SELECT (`checkInbox`); (2) the background check only notified and
  never reloaded the list that is open → `refreshInboxView()` reloads an open Inbox / shared view
  when a check finds new mail or a changed unread count. Added an `Inbox check for <account>: N
  unread, M new` info log (counts only). Confirmed by the user: tray icon and tooltip, the
  notification, the new mail appearing in the open list on its own. Not confirmed yet: clicking the
  notification to open the message, closing to the tray, "Configure Notifications…", and a sound
  (the notifyrc has `Action=Popup` only — a sound is an open question for the user).
  Skipped 2026-10-09 on the user's decision. Confirmed earlier: tray icon and tooltip, the notification, the new mail appearing in the open list. Never checked: clicking a notification to open the message, closing to the tray, "Configure Notifications…". Sound: the user does not want one (the open question is answered: popup only).


# Milestone 3c — Resizable panels (user request 2026-10-09: "I need to resize panels")

- **Goal**: the sidebar and the message-list column can be made wider or narrower by dragging
  the thin divider between panels; the message column takes the rest. Sizes are remembered.
- **Today**: the sidebar is fixed (14 grid units, `FolderSidebar.qml`); the list column has
  Kirigami hints min 20 / preferred 28 / max 40 grid units (`MessageListPage.qml`) and cannot be
  dragged; Kirigami's page row has no drag handles of its own.
- **Decisions** (made while planning; change only with the user):
  - Our own divider: a thin vertical handle on the right edge of the sidebar and of the list
    column (a hover/drag highlight, the horizontal-split cursor), dragged with a `DragHandler`.
    Double-click on a handle resets that panel to its default width.
  - Widths are stored in logical pixels in plasmamailrc `[General]` (`SidebarWidth`,
    `ListWidth`), clamped: sidebar 160–420, list 280–800; defaults = today's sizes. Saved when the
    drag ends, not on every pixel.
  - When the window is narrow (sidebar slides over / one column at a time) there are no handles
    and the stored widths are not used.
  - The message column keeps filling the remaining width; the list's maximum rises from 40 grid
    units to the clamp above.
- **Out of scope**: dragging the message column itself, a splitter between the toolbar and the
  message body, remembering the window size/position (separate request if wanted), reordering
  panels.

### T130 — `AppSettings`: panel widths
- status: done
- depends on: none
- parallel-safe with: none
- files: src/engine/appsettings.h, src/engine/appsettings.cpp, autotests/appsettingstest.cpp
- inputs: T122 `AppSettings` (clamping pattern).
- output: `sidebarWidth` (default 224) and `listWidth` (default 448) with `[General]` keys
  `SidebarWidth` / `ListWidth`, clamped to 160–420 and 280–800 on load and save; constants for the
  bounds and defaults; tests: defaults, round trip, clamping on load and save, other options kept.
- verify: `ctest --test-dir build -R appsettingstest --output-on-failure`
- notes: Done 2026-10-09. `AppSettings::sidebarWidth` (252, 160–420) and `listWidth` (360,
  280–800), keys `SidebarWidth` / `ListWidth` in `[General]`, clamped on load and on save; the
  default/min/max values are public constants for QML (T131). 3 new tests (clamp on load, clamp on
  save, garbage → defaults) and the defaults/round-trip tests now cover the widths. **Watch out
  (T131):** `MailController::saveAppSettings` builds a fresh `AppSettings` with only the tray and
  interval set, so it must start from `m_app` or it would reset the widths to the defaults — already
  fixed here (it now copies `m_app` first).

### T131 — Controller: panel width properties
- status: done
- depends on: T130
- parallel-safe with: T132
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T130; `saveAppSettings` as the pattern.
- output: properties `sidebarWidth`, `listWidth` (NOTIFY `panelWidthsChanged`) and constants for
  the limits/defaults exposed to QML; `Q_INVOKABLE void savePanelWidths(int sidebar, int list)`
  (clamps, saves, emits; no restart of anything else); `saveAppSettings` keeps the widths.
- verify: build succeeds; all tests pass.
- notes: Done 2026-10-09. Properties `sidebarWidth`, `listWidth` (NOTIFY `panelWidthsChanged`) and
  CONSTANT `sidebarMin/Max/DefaultWidth`, `listMin/Max/DefaultWidth`; `savePanelWidths(sidebar,
  list)` clamps, saves via a copy of `m_app` (so the tray/interval options stay) and emits only
  when something changed. `saveAppSettings` already keeps the widths (T130). Build without
  warnings, 14/14 tests; nothing in QML uses them yet (T133/T134).

### T132 — `PanelResizer.qml`: the drag handle
- status: done
- depends on: none
- parallel-safe with: T130, T131
- files: src/qml/PanelResizer.qml (new), src/CMakeLists.txt
- inputs: Kirigami theme colours; `DragHandler`, `HoverHandler`, `Qt.SplitHCursor`.
- output: a 6 px wide item (a 1 px visible line, highlighted on hover/drag) with properties
  `minimum`, `maximum`, `defaultWidth`, `panelWidth`; while dragging it updates a `liveWidth`
  (clamped); signals `resized(int width)` on release and `resetRequested()` on double-click.
- verify: offscreen start prints no new QML warnings; screenshot of the handle placed on a panel.
- notes: Done 2026-10-09. `src/qml/PanelResizer.qml`: a 12 px wide transparent strip (z 100) with
  a line that is invisible at rest and shows 3 px in the highlight colour on hover/drag; required
  `panelWidth`, `minimum`, `maximum`; read-only `dragging` and `liveWidth` (start width + the
  pointer's movement on screen, clamped — measured from scene positions because the divider
  itself moves with the panel); signals `resized(int width)` when the drag ends and
  `resetRequested()` on a double tap; `Qt.SplitHCursor` on hover/drag. Registered in CMake. Checked:
  it loads with no QML warnings, and a temporary copy placed over the list's right edge (with the
  line forced visible, then reverted) rendered correctly. Dragging itself is first exercised in
  T133/T134 (and by hand in T135).

### T133 — Sidebar: resizable
- status: done
- depends on: T131, T132
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml
- inputs: T131, T132; `modal` mode of the drawer.
- output: the sidebar's width follows `MailController.sidebarWidth` (live while dragging) with a
  handle on its right edge when docked (not modal); release saves; double-click resets.
- verify: offscreen start prints no new QML warnings; the sidebar looks unchanged at the default.
- notes: Done 2026-10-09 (drag not tried yet — T135). `FolderSidebar.qml`: the content is now an
  `Item` holding the old `ColumnLayout` (re-indented) and a `PanelResizer` on the right edge.
  Docked (not modal): `width` = `MailController.sidebarWidth`, or the resizer's `liveWidth` while
  dragging; release → `savePanelWidths(width, listWidth)`; double-click → default. Modal (narrow
  window): no handle and the old fixed 14 grid units. **Defaults corrected:** a grid unit is
  18 px here, so "today's sizes" are 252 px (sidebar, 14 units) and 504 px (list, 28 units), not the
  224/448 first planned; `AppSettings` and its test now use 252/504. Checked offscreen/offline:
  the sidebar and list look identical to before at the default, no new QML warnings; 14/14 tests.

### T134 — Message list: resizable
- status: done
- depends on: T131, T132
- parallel-safe with: none
- files: src/qml/MessageListPage.qml
- inputs: T131, T132; the page's `Kirigami.ColumnView` hints.
- output: `preferredWidth` follows `MailController.listWidth` (live while dragging), `minimumWidth`
  / `maximumWidth` from the clamp; a handle on the list's right edge when the window is wide
  enough for two columns; release saves; double-click resets; the message column fills the rest.
- verify: offscreen start prints no new QML warnings; the list looks unchanged at the default.
- notes: Done 2026-10-09 (drag not tried yet — T135). `MessageListPage.qml`: the
  `Kirigami.ColumnView` min/max/preferred widths now come from `MailController`
  (`listMinWidth`/`listMaxWidth`; preferred = `listWidth`, or the resizer's `liveWidth` while
  dragging); a `PanelResizer` on the page's `overlay` (so it does not scroll with the list) on the
  right edge, only when `applicationWindow().wideScreen`; release → `savePanelWidths(sidebarWidth,
  width)`; double-click → `listDefaultWidth`. Checked offscreen/offline at the defaults with a
  message open: the layout looks the same as before, no new QML warnings; 14/14 tests.
  **Fix after the user's try (2026-10-09):** dragging the list divider saved a width but changed
  nothing — in a wide window the page row's default `FixedColumns` mode ignores the per-page
  `ColumnView` width hints (every non-fill column is `defaultColumnWidth`, 360 px), and the first
  repair (driving `defaultColumnWidth`) breaks `wideMode` (`width >= 2 × defaultColumnWidth`). Now
  `Main.qml` binds the row's `columnView.columnResizeMode` to `DynamicColumns` (wide) /
  `SingleColumn` (narrow), where the list's min/preferred/max hints count. The widths are capped so
  the message column keeps 30 grid units (540 px, room for its toolbar): the list's maximum is
  `window − sidebar − 540`, the sidebar's `window − list minimum − 540` (a width saved on a bigger
  window shrinks to fit). The list's default is now **360 px** (today's real width; the 504 I
  planned was never applied by Kirigami). Checked offscreen/offline with the user's saved
  287/800: sidebar 287, list 523, message 540 with its whole toolbar; 14/14 tests.

### T135 — Manual check: resizing panels
- status: done
- depends on: T133, T134
- parallel-safe with: none
- files: none
- inputs: the user's desktop.
- output: confirmed: both dividers drag smoothly with the right cursor; limits hold; double-click
  resets; sizes survive a restart; narrow window still works (no handles).
- verify: manual, with the user.
- notes: Done 2026-10-09 with the user: after the T134 fix both dividers drag, the list is capped
  so the message column keeps its toolbar, and the user confirmed it works. (Sizes after a restart
  and the narrow-window layout were not reported separately.)

# Milestone 3d — Status bar (user request 2026-10-09: "status bar at the bottom with connection
# status and what you think I needed")

- **Goal**: a slim bar at the bottom of the main window that tells, at a glance, whether the
  accounts are connected and what the app is doing.
- **Content (my choice, change on request)**, left to right:
  1. **Connection**: one chip per account — a coloured dot (green connected, amber connecting,
     red failed/offline, grey not started) and the account's short name; the tooltip has the
     server and the last error text; clicking a red chip reconnects that account.
  2. **Activity** (the middle, one line): "Checking for new mail…", "Syncing <folder> 120 / 340",
     "Moving message…" while busy, otherwise "Up to date · checked 10:26" (the last background
     check).
  3. **Counters** (right): "N unread" (all inboxes) and, with local storage on, "12.3 MB stored".
- **Decisions**:
  - The bar is the window's `footer`; hidden on the first-run setup page (no account) and in
    settings layers. It stays one line (`Kirigami.Units.gridUnit * 1.6` high) with the small font.
  - States come from the accounts themselves (`AccountSession::connectionState`): connecting
    when a login/list starts, connected when a folder list arrived or any answer came back,
    failed with the error text when the session reports an error (also "connection lost").
    Offline is simply failed.
  - Error toasts stay as they are; the bar is the quiet, permanent view of the same facts.
  - No settings toggle for now.
- **Out of scope**: a progress bar, per-folder status, bandwidth numbers, a log viewer.

### T136 — `AccountSession`: connection state
- status: done
- depends on: none
- parallel-safe with: none
- files: src/app/accountsession.h, src/app/accountsession.cpp
- inputs: the client's signals; `connectWithSecret` / `connectWithRefreshToken`.
- output: enum `ConnectionState { Idle, Connecting, Connected, Failed }`, `connectionState()`,
  `lastError()` and signal `connectionStateChanged()`: Connecting when a connect starts, Connected
  when folders/headers/messages arrive, Failed (with the text) on `errorOccurred`; `Idle` before
  the first connect.
- verify: build succeeds; all tests pass.
- notes: Done 2026-10-09. `AccountSession::ConnectionState { Idle, Connecting, Connected, Failed }`,
  `connectionState()`, `lastError()`, `stateName()` ("idle"/"connecting"/"connected"/"failed")
  and signal `connectionStateChanged()`. Connecting when `connectWithSecret`, `signInWithGoogle` or
  `connectWithRefreshToken` start; Connected (from Connecting or Failed only) when headers, a
  message, a move or an inbox check come back, or the folder list arrives; Failed with the text
  when the client reports an error **and has no session left** (new `ImapClient::hasSession()` —
  a refused command like a missing folder keeps the session, so it doesn't turn the chip red) or
  the OAuth client fails. Build; 14/14 tests; no UI uses it yet (T138).

### T137 — `AccountSession`: sync progress text
- status: done
- depends on: none
- parallel-safe with: T136
- files: src/app/accountsession.h, src/app/accountsession.cpp
- inputs: `SyncManager::progress(folder, done, total)`, `syncFinished`, `aborted`.
- output: `syncText()` (e.g. "Syncing INBOX 120 / 340", empty when idle) and a signal
  `syncTextChanged()`; cleared when the sync ends.
- verify: build succeeds; all tests pass.
- notes: Done 2026-10-09. `AccountSession::syncText()` ("Syncing <folder name> done / total", the
  folder shown by its display name) updates on every `SyncManager::progress` and is cleared when
  the sync ends, aborts, fails or is stopped (`setSyncing(false)`); signal `syncTextChanged()`.
  Build; 14/14 tests.

### T138 — `AccountListModel`: status roles
- status: done
- depends on: T136, T137
- parallel-safe with: none
- files: src/models/accountlistmodel.h, src/models/accountlistmodel.cpp
- inputs: T136, T137.
- output: roles `connectionState` ("idle" | "connecting" | "connected" | "failed"), `statusText`
  (the last error or empty) and `syncText`; `dataChanged` when a session's state or text changes.
- verify: build succeeds; all tests pass.
- notes: Done 2026-10-09. Roles `connectionState` ("idle"/"connecting"/"connected"/"failed"),
  `statusText` (the last error) and `syncText`; `dataChanged` for the session's row (looked up
  again each time) when its state or sync text changes. Build; 14/14 tests; first used by T140.

### T139 — Controller: activity line
- status: done
- depends on: T138
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: `busy`, `syncing`, `m_checking`, `m_moving`, `onInboxChecked`.
- output: properties `lastCheckedAt` (QDateTime, null before the first check) and `activityText`
  (busy → "Loading…", moving → "Moving message…", a check in flight → "Checking for new mail…",
  a sync → that session's `syncText`, else empty); `reconnect(account)` (invokable) that
  reconnects a failed account the way startup does.
- verify: build succeeds; all tests pass.
- notes: Done 2026-10-09. `activityText` (priority: a move → "Moving message…"; an account
  connecting → "Connecting…"; a sync → that account's sync text; a background check →
  "Checking for new mail…"; a busy request → "Loading…"; else empty) and `lastCheckedAt`, both
  with `activityChanged`, recomputed by `updateActivity()` on busy / sync / check / move / session
  state changes; `reconnect(account)` reads the keychain entry again and lets `onSecretRead` start
  the connection (a password account logs in, a Google account refreshes its token). Build;
  14/14 tests; shown in T140.

### T140 — `StatusBar.qml` in the main window
- status: done
- depends on: T138, T139
- parallel-safe with: none
- files: src/qml/StatusBar.qml (new), src/qml/Main.qml, src/CMakeLists.txt
- inputs: T138 roles, T139 properties, `unreadCount`, `storageUsedBytes`, `storageEnabled`.
- output: the bar described above as the window `footer`, visible only with an account and the
  mail view; a thin top separator; chips with tooltips; clicking a failed chip calls
  `MailController.reconnect(email)`.
- verify: offscreen start prints no new QML warnings; screenshots offline (red chip) and normal.
- notes: Done 2026-10-09 (user review pending, T141). `src/qml/StatusBar.qml` (a `QQC2.Pane`, 1.8
  grid units high, small font, thin top separator) is the window `footer` while there is an account
  (`footer: hasAccount ? statusBar : null`). Left: a chip per account — a dot (green / amber / red /
  grey) and the email; tooltip with server, state and the last error; a failed chip is clickable
  (`reconnect`). Middle: `activityText`, else "Up to date · checked HH:MM" (or just "Up to date"
  before the first check). Right: "N unread" and, with storage on and ready, "X stored".
  **Side effect fixed:** a docked Kirigami drawer subtracts the window footer from its height a
  second time, leaving a gap under the sidebar, so `FolderSidebar` now binds its height to its
  parent's when docked. Checked offscreen/offline on a copy of the user's data: red chip (no
  network), "Up to date", "0 unread", "130,3 KiB stored", sidebar reaching the bar; no new QML
  warnings; 14/14 tests. Not seen with a live connection yet.

### T141 — Manual check: status bar
- status: skipped
- depends on: T140
- parallel-safe with: none
- files: none
- inputs: the user's desktop.
- output: confirmed: the chip is green while connected; turning the network off turns it red with
  the error in the tooltip and "Up to date" changes when back online; syncing text appears during a
  sync; counters match the tray tooltip.
- verify: manual, with the user.
- notes: 2026-10-09: the user ran the installed build and said "looks fine" (look, colours and
  sizes of the bar). **Not reported yet:** the network-off test (red chip, error tooltip,
  click-to-reconnect), a live "Syncing …" text, and unread count vs tray tooltip — so this stays
  `todo` until the user confirms them or says to close it.

# Milestone 3d — Last action in the status bar (user request 2026-10-09)

- **Goal**: the status bar shows the last thing that happened, such as "New mail in <account>",
  "Message sent", "Message archived", with the time, so the user can see what the app did.
- **Decisions** (made while planning; change only with the user):
  - It is a separate label in the bar, next to the existing activity text ("Up to date · checked
    HH:MM" / "Syncing …"), which stays as it is.
  - Events recorded: new mail (counts only, never sender or subject), message sent, message
    archived, deleted, moved (with the folder name), and failures of those ("Sending failed").
    Marking read/unread and flagging are too frequent and are not recorded.
  - Only the last one is kept, in memory; it is not saved between starts.
  - Never put message content (sender, subject, text) in it: it is shown on screen and may be
    screenshotted.
- **Out of scope**: a history list, clicking the label, saving it to disk.

### T142 — Controller: `lastAction` property
- status: done
- depends on: T104, T113, T127
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: the existing signals/places: `messageSent`, `sendFailed`, `newMailArrived`, and the
  archive / delete / move code paths.
- output: `Q_PROPERTY(QString lastActionText READ lastActionText NOTIFY lastActionChanged)` (empty
  at start; a `QString` such as "Message sent · 17:05", time as HH:MM), set by a private
  `setLastAction(const QString &)` that emits `lastActionChanged` only when the text changed. Called
  on the events listed under Decisions. Texts go through `i18n()`.
- verify: build succeeds; `ctest --preset dev` passes; a unit test is not needed (the controller
  has none) but the signal must be declared, since an earlier attempt had a property with no
  `NOTIFY` signal (rejected).
- notes: Done 2026-10-09. setLastAction("%1 · HH:mm") hooks: sendMessage SMTP sent/failed lambdas
  (validation refusals record nothing), onInboxChecked (count + account email), archive/delete/move
  pass done/failed texts via moveOpenMessage into `m_moveTexts` (keyed by moveKey); success in
  onMessageMoved, failure in onSessionError; removeAccount now also clears that account's
  m_moving/m_moveTexts and releases the requests. Build ok, ctest 16/16, offscreen clean.

### T143 — Status bar: show the last action
- status: done
- depends on: T142
- parallel-safe with: none
- files: src/qml/StatusBar.qml
- inputs: T142.
- output: a small label with `MailController.lastActionText`, visible only when it is not empty,
  placed between the activity text and the counters, elided on the right when space is short;
  tooltip with the full text.
- verify: `qmllint` clean on the file; offscreen start prints no new QML warnings; screenshot hook
  shows the bar still fits at the default window width.
- notes: Done 2026-10-09. Label `lastActionLabel` between the activity text and the counters:
  smallFont, disabledTextColor, ElideRight, max width 16 gridUnits (no fillWidth, so the activity
  text shrinks first); tooltip with the full text only when truncated. qmllint clean, offscreen start
  clean. The screenshot check could not show the bar (a throw-away profile has no account, only the
  setup page; no fake account added), so "fits at default width" moves to T144.

### T144 — Manual check: last action in the status bar
- status: done
- depends on: T143
- parallel-safe with: none
- files: none
- inputs: the user's desktop; send a message to the user's own address.
- output: confirmed: after sending, the bar says "Message sent · HH:MM"; a new mail from the
  background check says so; archiving or moving a test message shows it; nothing from the message
  (sender, subject) appears.
- verify: manual, with the user.
- notes: Also check the bar still fits at the default window width with the label shown (from
  T143). Needs a rebuild/reinstall of the running app first.
  Closed 2026-10-09 by the user after trying the last-action label in the status bar.


### T145 — Moves dropped on reconnect / over-broad move failures (found in T142)
- status: done
- depends on: T142
- parallel-safe with: none
- files: src/engine/imapclient.h, src/engine/imapclient.cpp, src/app/mailcontroller.cpp
  (also mailcontroller.h and accountsession.{h,cpp}, which forward the new signals)
- inputs: tester notes of T142.
- output: (a) when `ImapClient::setAccount()`/`disconnectFromServer()` bump the generation, moves
  in flight are reported (e.g. a `jobsDropped()` signal) so MailController clears `m_moving`,
  `m_moveTexts` and the busy count instead of showing "Moving message…" forever; (b) a non-move
  error on an account no longer marks every in-flight move as failed ("Moving failed" shown for a
  move that the server completed).
- verify: build; `ctest --preset dev`; offscreen start clean.
- notes: pre-existing; not part of the user's request.
  Done 2026-10-09 (user approved). ImapClient: `m_pendingMoves`, every move step through
  `checkMoveJob`; signals `moveFailed(folder, uid, msg)` (job NO, session survives), `sessionLost(msg)`
  (from fail(), the only fatal path) and `jobsDropped()` (generation bump with moves pending).
  MailController: `onMoveFailed` drops one move; `dropMoves(session, failed)` on sessionLost (with
  "Moving failed") and jobsDropped (silent); onSessionError no longer drops moves. Each move's request
  is released exactly once (traced). No ImapClient test harness: checked by reading; ctest 17/17,
  offscreen clean. Left as is: onSessionError still calls resetRequests() on any error; a
  moveByCopy failure after the copy can leave a copy in the target; a reconnect forgets a move the
  server may have finished (list fixed at the next sync). Same family as T117's note (a non-fatal
  error clears the search FIFO; m_requests can leak).

### T146 — Toolbar: hover text on the buttons right of Archive/Delete
- status: done
- depends on: T114, T143
- parallel-safe with: none
- files: src/qml/MessagePage.qml
- inputs: the user's report (2026-10-09): "action buttons from Move to Folder to the right don't have
  hover text". Buttons affected: Move to Folder, Label, Reply, Reply All, Forward. Archive, Delete,
  Mark as Unread and Flag already have `QQC2.ToolTip` with the shortcut.
- output: every toolbar button shows a tooltip on hover with its name and shortcut, like the
  others (Reply "(R)", Reply All "(Shift+R)", Forward "(Shift+F)"; Label and Move have none). The
  generic `buttonComponent` gets the tooltip. Move to Folder already has one in the code, so find
  out why it does not show (is the button disabled? disabled controls get no hover) and fix it so
  a disabled button still explains itself, as Archive and Delete do.
- verify: `qmllint` clean; offscreen start prints no new QML warnings; the user hovers the buttons
  (manual).
- notes: 2026-10-09. `buttonComponent` gets QQC2.ToolTip (text from a new `tooltip` model field):
  "Reply (R)", "Reply All (Shift+R)", "Forward (Shift+F)", "Label". Move to Folder: its tooltip already
  worked (QtTest probe hosting the real MessagePage, enabled and disabled; disabled ToolButtons still
  report hovered in Qt 6.12); it now says "Move to Folder is not available for this message." when
  disabled, like Archive/Delete. qmllint clean, ctest 16/16, offscreen start clean. The app the user
  was running had been started before this rebuild, so it must be restarted to show the change.
  Closed 2026-10-09 by the user after hovering the toolbar buttons.

### T147 — Google sign-in: "Could not start the local sign-in listener"
- status: done
- depends on: T146
- parallel-safe with: none
- files: src/engine/oauthclient.cpp
- inputs: the user's first live Google sign-in (2026-10-09): the toast "Could not start the local
  sign-in listener." and, in the log, `QTcpServer::listen() called when already listening` (twice).
  Cause (to confirm): `QOAuthHttpServerReplyHandler(this)` already starts listening in its
  constructor, so the explicit `listen(QHostAddress::LocalHost, 0)` in `OAuthClient::signIn()`
  fails.
- output: `signIn()` uses the handler's own listening (or constructs it with `LocalHost` and port 0
  and checks `isListening()`), fails with the same message only if it is not listening, and still
  closes the handler in `finish()`. A second sign-in after a failed or cancelled one works.
- verify: build succeeds; `ctest --preset dev` passes; the manual check is the user's sign-in
  (T80/T95).
- notes: Done 2026-10-09. Cause confirmed with a probe (Qt 6.12.0): the `(QObject*)` constructor
  already listens, so the extra `listen()` failed. Now `QOAuthHttpServerReplyHandler(QHostAddress::
  LocalHost, 0, this)` (loopback only) and a check of `isListening()`. Probe: two construct/close/
  deleteLater cycles both listen; callback() is http://127.0.0.1:<port>/. `m_replyHandler` is a
  QPointer, so a second sign-in is safe. Build ok, ctest 16/16. Live sign-in: T80/T95 (user).

# Milestone 5a — Reorder accounts (user request 2026-10-08, repeated 2026-10-09 after adding Gmail)

- **Goal**: the user can change the order of the accounts (Yahoo, Gmail, …); the order is saved and
  the sidebar sections and Settings → Accounts follow it.
- **Decisions** (made while planning; change only with the user):
  - First version: **Move up / Move down** buttons on each row of Settings → Accounts (keyboard
    friendly, no drag code). Dragging can come later.
  - The order is the order of `m_sessions` and of `[Accounts] Order` in `plasmamailrc` (T68), so
    saving already keeps it; the work is to move a session, save, and tell the models.
  - Which account is "first" matters nowhere else (shared views merge every account), so moving
    must not reconnect, resync or change the open folder.
- **Out of scope**: drag and drop, custom sorting rules.

### T148 — Controller and model: move an account
- status: done
- depends on: T147
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp, src/models/accountlistmodel.h,
  src/models/accountlistmodel.cpp
- inputs: `m_sessions`, `saveAccounts()`, `AccountListModel` (one row per session, in order).
- output: `Q_INVOKABLE void moveAccount(const QString &email, int delta)` (delta -1 up, +1 down;
  ignores a move past either end or an unknown email) that moves the session in `m_sessions`,
  calls `saveAccounts()`, and makes `AccountListModel` move the row (`beginMoveRows`/`endMoveRows`,
  not a reset, so the sidebar's expanded/collapsed sections and the selection stay). No
  reconnect, no new request, the open folder stays open.
- verify: build succeeds; `ctest --preset dev` passes; a model test (extend the existing
  `AccountListModel` test if there is one, else add `autotests/accountlistmodeltest.cpp`) moves a
  row up, down, and past the end.
- notes: Done 2026-10-09. `MailController::moveAccount(email, ±1)` → `AccountListModel::moveRow(from,
  to)` (beginMoveRows, destination to+1 when moving down) + `m_sessions.move` + `saveAccounts()`;
  emits accountChanged() only when index 0 is involved (legacy first-account properties). The
  sidebar's Repeater (FolderSidebar.qml) binds to the AccountListModel, so rowsMoved keeps its
  sections. Also changed src/CMakeLists.txt (not listed): accountsession.cpp and
  accountlistmodel.cpp moved into the `plasmamail_models` static lib so the new
  autotests/accountlistmodeltest.cpp can link them (no QML registration involved). ctest 17/17,
  fresh configure in a separate build dir ok, offscreen clean. moveAccount itself has no unit test.
  Commit note: the source/CMake part landed in 06b8d30 (a planning commit picked up the working
  tree); 3de53eb has the test and these notes.

### T149 — Settings → Accounts: move up / down buttons
- status: done
- depends on: T148
- parallel-safe with: none
- files: src/qml/AccountSettingsPage.qml
- inputs: T148.
- output: each account row has two small buttons (arrow up / arrow down, tooltips "Move up" /
  "Move down"); the first row's up and the last row's down are disabled. The sidebar sections
  reorder at once.
- verify: `qmllint` clean; offscreen start prints no new QML warnings.
- notes: Done 2026-10-09. ToolButtons go-up/go-down per row (tooltips + accessible names "Move up" /
  "Move down"), enabled by `index > 0` / `index < accountRepeater.count - 1`. QtTest probe hosting the
  real page with a stub controller: states correct before and after moves, an arrow click does not
  open the details page (a row click still does). qmllint clean, ctest 17/17, offscreen clean.

### T150 — Manual check: reorder accounts
- status: done
- depends on: T149
- parallel-safe with: none
- files: none
- inputs: the user's desktop with the Yahoo and Gmail accounts.
- output: confirmed: the buttons swap the accounts in Settings and in the sidebar; the order is
  still the same after restarting; the open folder and messages stay as they were.
- verify: manual, with the user.
- notes:

# Milestone 3e — Attachments: open, save and attach (user request 2026-10-09)

- **Goal**: the user can open and save the attachments of a received message (the chips in the
  message view are only labels today), and attach files to a message being written.
- **Decisions** (made while planning; change only with the user):
  - Received attachments come from the whole message that is already stored locally or fetched
    (`fetchMessage`); nothing new is asked of the server.
  - Open = write the attachment into a private temp folder (created 0700 by the app, one folder
    per session, removed at quit), then `QDesktopServices::openUrl`. Save As = a file dialog.
    The file name from the message is never trusted: only its last path component is used,
    control characters are removed, and an empty name becomes "attachment".
  - **Never open executable or script types straight away** (`.sh`, `.desktop`, `.exe`, `.bat`,
    `.cmd`, `.com`, `.scr`, `.jar`, `.appimage`, `.run`, `.py`, `.js`, …, and MIME types that are
    executable): offer Save As only, with a short explanation.
  - Attaching: a button in the compose window with a file dialog (several files at once), a list
    of the attached files with a remove button each, total size shown; refuse above 25 MB in
    total (Gmail's limit) with a clear message. Content-Type from `QMimeDatabase`, base64 body.
  - Never put attachment names or contents into logs.
- **Out of scope**: drag and drop into the compose window, inline images in the body, previews of
  attachments, forwarding with the original attachments (a later request), virus scanning.
  Closed 2026-10-09: the user tried the Move up / Move down buttons and said reordering works (restart persistence not reported).
  Closed 2026-10-09: the user tried the Move up / Move down buttons and said reordering works (restart persistence not reported).

### T151 — Engine: attachment data from a message
- status: done
- depends on: T9
- parallel-safe with: T155
- files: src/engine/messagerenderer.h, src/engine/messagerenderer.cpp,
  autotests/messagerenderertest.cpp
- inputs: `attachmentNames()` (same order and the same naming rules).
- output: `struct AttachmentData { QString name; QString mimeType; QByteArray data; };` and
  `QList<AttachmentData> attachmentParts(const KMime::Message &msg)` in the same order as
  `attachmentNames()`, with decoded content (`decodedContent()`), a safe file name (rules in the
  milestone decisions) and the Content-Type (`application/octet-stream` when missing).
- verify: build succeeds; the renderer test gets rows for: a base64 text attachment, a name with
  `../` and a slash, an unnamed attachment, and no attachments; `ctest --preset dev` passes.
- notes: Done 2026-10-09. Free functions in messagerenderer.h: `AttachmentData`,
  `attachmentParts(msg)` (same `msg.attachments()` order and naming as attachmentNames()) and
  `safeAttachmentFileName(raw)` (last component after / or \\, Other_Control removed, trimmed; empty,
  "." or ".." → "attachment") for T152. KMime here has `decodedBody()` (decodedContent() was renamed
  in 25.12). A part without Content-Type gets KMime's default text/plain (RFC 2045), not
  octet-stream. attachmentNames() still returns raw names (UI unchanged). ctest 17/17.

### T152 — Controller: open and save an attachment
- status: done
- depends on: T151
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: the open message (kept as `KMime::Message` or re-parsed from the stored raw), T151.
- output: `Q_INVOKABLE void openAttachment(int index)` (temp folder + `QDesktopServices::openUrl`,
  or `attachmentBlocked(name)` signal for executable types), `Q_INVOKABLE bool saveAttachment(int
  index, const QUrl &target)` (writes the file, mode 0600, returns success; failure shows the
  existing error toast), `Q_INVOKABLE bool attachmentIsRisky(int index)`. The temp folder is
  created with 0700 on first use and removed on quit. No file names in logs.
- verify: build succeeds; `ctest --preset dev` passes.
- notes: Done 2026-10-09. openAttachment / saveAttachment / attachmentIsRisky + signal
  attachmentBlocked(name); errors via errorOccurred. Temp: `QTemporaryDir` plasma-mail-XXXXXX (0700),
  numbered 0700 subfolder per opened attachment, files 0600 via QSaveFile; reset on aboutToQuit and in
  the destructor. The risky rule lives in messagerenderer: `isRiskyAttachment(AttachmentData)` (last
  suffix decides; declared, name-detected and content-detected MIME types; lists in one place) with
  14 test rows (also touched src/engine/messagerenderer.{h,cpp}, autotests/messagerenderertest.cpp).
  ctest 17/17, offscreen clean. Follow-ups in T158.

### T153 — Message view: clickable attachments
- status: done
- depends on: T152
- parallel-safe with: none
- files: src/qml/MessagePage.qml
- inputs: T152; the chips at `MessagePage.qml` (`MailController.messageAttachments`).
- output: clicking a chip opens the attachment (a risky one shows a dialog: "Opening this type of
  file can run code. Save it instead?" with Save As…); a menu on each chip (right click or small
  arrow): Open, Save As… (a `QtQuick.Dialogs.FileDialog` in save mode, default name = the
  attachment name); tooltip with the full name.
- verify: `qmllint` clean; offscreen start prints no new QML warnings.
- notes: Done 2026-10-09. Chip click opens (risky → Kirigami.PromptDialog "Opening this type of file can
  run code. Save it instead?" with Save As… / Cancel); arrow button + right click menu Open / Save As…;
  one FileDialog (SaveFile, Downloads, default name = new `MailController.attachmentFileName(index)`,
  the sanitized name, URL-encoded) → saveAttachment. `pendingIndex` is set before every
  openAttachment, so the attachmentBlocked safety net does not depend on names. Also touched
  src/app/mailcontroller.{h,cpp} (attachmentFileName). QtTest probe with a stub controller 8/8;
  qmllint clean, ctest 17/17, offscreen clean. Chip labels still show the raw names (display only).

### T154 — Manual check: open and save attachments
- status: done
- depends on: T153
- parallel-safe with: none
- files: none
- inputs: the user's desktop; a message with a PDF or image attachment and one with an
  unnamed or odd-named attachment.
- output: confirmed: click opens it in the right program; Save As writes the same bytes; a
  `.sh` attachment offers Save As only.
- verify: manual, with the user.
- notes:
  Closed 2026-10-09: the user tried the final build and said "it's fine now" (a general confirmation; no per-case details given).

### T155 — Message builder: attachments
- status: done
- depends on: T110
- parallel-safe with: T151
- files: src/engine/outgoingmessage.h, src/engine/outgoingmessage.cpp, src/engine/smtpclient.cpp,
  autotests/outgoingmessagetest.cpp (corrected 2026-10-09: the planned messagecomposer only builds
  reply/forward text; `OutgoingMessage::build()` makes the bytes that are sent)
- inputs: the existing builder (T110) that makes the `KMime::Message` / raw text to send.
- output: the builder takes a list of file paths (`QStringList attachments`); the message becomes
  `multipart/mixed` (the text part first, then one part per file with base64 transfer encoding, a
  `Content-Disposition: attachment; filename=…` (RFC 2231 encoded when not ASCII) and a
  Content-Type from `QMimeDatabase`). Without attachments the message is as before. Unreadable
  file: the builder fails with an error message naming the file's base name only.
- verify: build succeeds; the test builds a message with two small temp files (one non-ASCII
  name), parses it back with KMime and finds both attachments with equal bytes; a message
  without attachments is unchanged; `ctest --preset dev` passes.
- notes: Done 2026-10-09. `OutgoingMessage::attachments` (paths), `MaxAttachmentBytes` (25 MB, raw
  size), `Built::error` (empty data on failure); SmtpClient::send emits failed(error) synchronously,
  after MailController has connected (checked). Errors name only the base name. Size checked before
  reading and the read is bounded. Without attachments the output is unchanged. The sent copy (T113)
  uses the same bytes, so it has the attachments. KMime: read a multipart body via
  `textContent()->decodedText()`. ctest 17/17 (outgoingmessagetest 14).

### T156 — Compose window: attach files
- status: done
- depends on: T155, T112
- parallel-safe with: none
- files: src/qml/ComposeWindow.qml, src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: T155; the compose window's Send path (`sendMessage(QVariantMap)`).
- output: an "Attach" button (icon `mail-attachment`, tooltip "Attach files") opening a
  multi-select `FileDialog`; a row of chips for the chosen files (name, size, a remove button);
  total size label; Send is refused above 25 MB with the message "Attachments are larger than
  25 MB." The `sendMessage` map gets an `attachments` list of local file paths that the
  controller passes to the builder. The sent copy in Sent (T113) contains the attachments.
- verify: build succeeds; `qmllint` clean; offscreen start prints no new QML warnings;
  `ctest --preset dev` passes.
- notes: Done 2026-10-09. MailController: `maxAttachmentBytes` (CONSTANT), `fileInfo(url)` → {path, name,
  size} ({} unless a regular file; QFileInfo::isFile is false for FIFOs/devices, checked), sendMessage
  reads map["attachments"] (file URLs → toLocalFile). ComposeWindow: Attach button (mail-attachment)
  → FileDialog OpenFiles, deduped closable chips with sizes, "Total: …", over 25 MB → red "Attachments
  are larger than 25 MB." and Send disabled/refused; attachments alone count as content for the
  discard prompt. QtTest probe with a stub controller 11/11 (FileDialog accept itself not driven).
  ctest 17/17, offscreen clean. qmllint (real one, /usr/lib/qt6/bin/qmllint): 4 new "Unqualified
  access" warnings of the kind the file already has (singleton + root id in a delegate) → T159.

### T157 — Manual check: send an attachment
- status: done
- depends on: T156, T154
- parallel-safe with: none
- files: none
- inputs: the user's accounts; send to the user's own address.
- output: confirmed: a message with a picture and a PDF arrives at the other account with both
  attachments, which open; Yahoo's Sent folder has a copy with them; Gmail has exactly one copy;
  a too large file is refused.
- verify: manual, with the user.
- notes:
  Closed 2026-10-09: the user tried the final build and said "it's fine now" (a general confirmation; no per-case details given).

### T158 — Attachments: follow-ups from T152
- status: done
- depends on: T153
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp (main.cpp not needed; the sweep rule went
  into src/engine/messagerenderer.{h,cpp} + autotests/messagerenderertest.cpp)
- inputs: tester notes of T152.
- output: (a) the decoded attachment parts are cached with the open message instead of re-decoding
  the whole message on every openAttachment/saveAttachment/attachmentIsRisky call; (b) when
  `QDesktopServices::openUrl` returns false, show "No program is set up to open this file." via
  errorOccurred; (c) at start, remove stale `plasma-mail-*` temp folders owned by the user (left
  after a crash; they hold decoded attachments).
- verify: build; `ctest --preset dev`; offscreen start clean.
- notes: Done 2026-10-09 (user approved). (a) `m_attachmentCache` (optional list), reset in showMessage
  and clearMessage, the only places m_openMessage changes. (b) openUrl false → errorOccurred. (c) Temp
  template now `plasma-mail-<pid>-XXXXXX`; `removeStaleAttachmentFolders()` at controller start (Linux
  only): own, non-symlink dirs in tempPath where `isStaleAttachmentFolder()` says so (new form: pid
  gone from /proc; old form: older than 24 h). Tested end to end with TMPDIR in scratch: only dead-pid
  and 2-day-old folders removed; symlinks, their targets, files and live folders kept. ctest 17/17.
  Limit: inside a sandbox without the host's /proc a live host instance would look dead.

### T159 — Real qmllint baseline
- status: done
- depends on: none
- parallel-safe with: none
- files: src/qml/*.qml, CMakePresets.json or src/CMakeLists.txt (lint target)
- inputs: found 2026-10-09: `/usr/bin/qmllint` is an old 1.0 tool that prints nothing, so every
  "qmllint clean" so far (incl. T143, T146, T149, T153, T156) proved nothing. The Qt 6 one,
  `/usr/lib/qt6/bin/qmllint`, reports ~250 warnings over src/qml (mostly "Unqualified access",
  some missing-property `pageStack`, an id shadowing `window`, PlasmaMail import paths).
- output: a lint command that uses the Qt 6 qmllint with the build's import paths (e.g. the
  `all_qmllint` target of qt_add_qml_module, or a documented command) recorded under Global verify;
  `pragma ComponentBehavior: Bound` and qualified ids where cheap; the remaining warnings counted and
  written down as the baseline, so later tasks check "no new warnings" against it.
- verify: the lint command runs; the count is at or below the recorded baseline; offscreen start clean.
- notes: not a user request; a quality task found while verifying T156. Done 2026-10-09 (user
  approved). The `all_qmllint` target existed but ran without a Qt import path (Qt6_DIR resolves under
  /opt/cuda, so QT6_INSTALL_QML is wrong): added `IMPORT_PATH ${KDE_INSTALL_FULL_QMLDIR}` to
  qt_add_qml_module in src/CMakeLists.txt. Before: 1449 lines (mostly failed imports); with the path
  56; after `pragma ComponentBehavior: Bound` in StatusBar, MessageListPage, MessagePage,
  ComposeWindow, AccountSettingsPage, AccountSetupPage, FolderSidebar (+ `toolLoader.modelData` in the
  MessagePage toolbar) and Main.qml's root id `window` → `mainWindow`: 20 (baseline above). Runtime
  check: throw-away profile with a fake .invalid account, no D-Bus, no network, seeded store,
  screenshots of the sidebar, list, open message, settings and compose: no QML errors; the two
  "not placed in the graphics scene" lines are the same on HEAD. Probes 11/8/6 pass, ctest 17/17.
  Left (T160): pageStack/show/close on untyped objects, applicationWindow() unqualified,
  Qt.application.layoutDirection, 3 unused imports.

### T160 — QML lint: the remaining 20 warnings
- status: done
- depends on: T159
- parallel-safe with: none
- files: src/qml/Main.qml, src/qml/NotificationSettingsPage.qml, src/qml/MessageListPage.qml,
  src/qml/MessagePage.qml, src/qml/FolderSidebar.qml, src/qml/Account*Page.qml (Details, Settings, Setup)
- inputs: T159's list.
- output: unused imports removed (check attached properties first); `pageStack`, `show`, `close` on
  typed objects (e.g. `Kirigami.ApplicationWindow` casts / typed properties) or a commented
  `// qmllint disable` where Kirigami gives no type; count below 20.
- verify: lint count lower; offscreen start and the scratch probes clean.
- notes: optional quality task; user approved 2026-10-09. Done: lint 20 → 2. `(page.QQC2.ApplicationWindow
  .window as Kirigami.ApplicationWindow)` for pageStack/maxWidth/wideScreen instead of
  applicationWindow() in MessageListPage/MessagePage and the window's pageStack in Account*Page;
  `(composer as Window).show()`; `Application.layoutDirection`; Main.qml: `(configViewItem as
  Kirigami.ApplicationWindow)?.pageStack` and `configViewItem as Window` → close() else layers.clear()
  (same as the old duck typing: desktop ConfigWindow vs mobile page); unused imports removed (Main:
  QQC2, Layouts; NotificationSettingsPage: QQC2). Left: FolderSidebar's 2 `applicationWindow()` calls
  (a Drawer has no ApplicationWindow attached property; `parent` not proven to be the same window).
  Screenshots HEAD vs new on a seeded throw-away profile: main, open message, settings (accounts,
  notifications, storage), RTL: pixel-identical; compose new/reply differ only by the caret blink.
  Logs unchanged. Not run: remove-last-account with settings open, mobile settings page.

# Milestone 3f — Readable HTML messages (user request 2026-10-09: "colors need fix", "visibility")

- **Goal**: an HTML message is always readable in the app, dark or light theme. Today plain-text
  mail gets the theme's text and background colours, but HTML mail is passed through unchanged
  (`messageBodyHtml`, HTML branch): without colours of its own the web view falls back to its
  defaults (black text, dark-blue links) on the theme's background, which is hard to read in the
  dark theme (the user's screenshot: a reply with quoted text and links).
- **Decisions** (made while planning; change only with the user):
  - A mail that sets **no colours of its own** (no `color`, `background`, `bgcolor` or `<font
    color>` in its markup or style) gets the theme: text colour, link colour (the theme's link
    colour, readable on the background), quote bars and the background, through a `<style>` block
    placed before the mail's markup, so the mail's own rules still win.
  - A mail that **sets its own colours** (newsletters) was designed for a light page: it is shown
    on a white page with black default text, so its colours stay readable in both themes. No
    "invert colours" tricks.
  - Detection is a simple check of the raw HTML text, in the engine; a wrong guess only picks
    the other readable option. No remote loads are added (CSP and image policy stay as they are).
- **Out of scope**: a setting to switch modes, per-sender choices, dark versions of images.

### T161 — Engine: readable colours for HTML messages
- status: done
- depends on: T7
- parallel-safe with: none
- files: src/engine/messagerenderer.h, src/engine/messagerenderer.cpp,
  autotests/messagerenderertest.cpp, src/app/mailcontroller.cpp (only if the link colour has to be
  passed in; the palette is read at the call near `messageBodyHtml(msg, palette.color(...))`)
- inputs: `messageBodyHtml(msg, text, background)`; the Qt palette's `Link` colour.
- output: the HTML branch adds the style block described above when the mail has no colours of
  its own, and a white-page block (`html{background:#fff;color:#000}`, links default) when it
  has. The function signature gains a link colour argument only if needed; existing callers and
  tests are updated.
- verify: build succeeds; renderer test rows: HTML without colours (contains the theme text and
  background colours and the link colour), HTML with `style="color:#333"`, with `bgcolor`, with
  `<font color>`, with `border-color` only (counts as no own colours); the CSP meta still comes
  first; `ctest --preset dev` passes.
- notes: Done 2026-10-09. File names as planned. `messageBodyHtml(msg, text, background, link)` (link
  required; mailcontroller passes palette Link); `htmlHasOwnColours()` regex
  `(?<![-\w])(color|background(-color)?)\s*:|\sbgcolor\s*=|<body\b[^>]*\s(text|link|vlink|alink|background)\s*=|<font\b[^>]*\scolor\s*=`
  (case-insensitive; border-color etc. excluded; "color:" in visible text → white page, the safe side);
  `readableLinkColour()` keeps ≥3:1 (WCAG `contrastRatio()`). Theme block after the CSP meta; own colours
  → `html{background:#fff;color:#000}`. 15 new test rows, ctest 17/17. Dark check: throw-away profile
  with BreezeDark kdeglobals (needs `QT_QPA_PLATFORMTHEME=KDE` offscreen, else the palette stays light),
  seeded store, screenshots: reply text #fcfcfc on #232629, link #1d99f3 (~4.8:1), quote readable;
  newsletter shown on white with its own colours. Found: quirks mode for every HTML mail → T163.

### T162 — Manual check: readable HTML messages
- status: done
- depends on: T161
- parallel-safe with: none
- files: none
- inputs: the user's desktop; the reply with quoted text from the screenshot and a newsletter
  with its own colours.
- output: confirmed: both are readable, links included, in the dark theme.
- verify: manual, with the user.
- notes:
  Closed 2026-10-09: the user checked the reply and a newsletter in the dark theme and said they are readable.

### T163 — HTML mail renders in quirks mode (found in T161)
- status: done
- depends on: T161
- parallel-safe with: none
- files: src/engine/messagerenderer.cpp, autotests/messagerenderertest.cpp (+ messagerenderer.h for
  the helper)
- inputs: T161 tester: the CSP meta is emitted before the mail's markup, so a mail's
  `<!DOCTYPE html>` comes after the first token and is ignored; every HTML mail renders in quirks
  mode (seen: body bgcolor fills the viewport for a mail with a DOCTYPE).
- output: if the mail starts (after whitespace/BOM/comments) with a DOCTYPE, that DOCTYPE is emitted
  first, then the CSP meta and style; otherwise unchanged. `testCspAlwaysPresent` updated to "CSP meta
  comes before any mail markup other than the DOCTYPE".
- verify: build; ctest; a render of a DOCTYPE mail is in standards mode (`document.compatMode` ==
  "CSS1Compat", or the bgcolor no longer fills the viewport).
- notes: needs the user's OK: it changes the "CSP meta first" rule slightly (still before any markup
  that could load something). User approved 2026-10-09.
  Done 2026-10-09. `leadingDoctypeLength(html)`: one optional U+FEFF + ASCII whitespace, then
  `^<!DOCTYPE\s+html\b[^<>]*>` (anchored, linear: 1 MB inputs < 35 ms). When it matches, the output is
  our literal `<!DOCTYPE html>` + CSP meta + style + the rest (mail's doctype stripped, never copied);
  otherwise unchanged. Nothing from the mail can precede the CSP meta (the prefix is fixed, the regex
  only decides the strip). testDoctypeHoisting: 16 rows incl. comment/script/img/hostile CSP meta/text
  before the doctype, `<` inside it, double doctype, svg doctype, NUL, quoted `>`, 1 MB inputs; each
  with and without remote images. Render check (offscreen, seeded, no network): the DOCTYPE newsletter
  no longer fills the viewport grey (pixel 244 → 255); a 100%-height table: red only at the top with
  DOCTYPE, full height without. ctest 17/17.

# Milestone 3g — Load remote images on request (user request 2026-10-09)

- **Goal**: remote images in a message stay blocked by default (privacy: they tell the sender
  you opened the mail). When a message has some, a bar above the body says so, and a button
  "Load images" shows them for **that message only**.
- **Decisions** (made while planning; change only with the user):
  - Per message, one click, not remembered: closing or switching the message blocks again.
    "Always load from this sender" is a later request.
  - Only `https:` images are allowed when the user asks (`img-src https:` in the CSP for that
    message and the web view's `autoLoadImages` on); plain `http:` stays blocked. Scripts, remote
    styles, fonts, frames and everything else stay blocked as before.
  - The bar only appears when the HTML really refers to remote images (`<img src="http(s)://…">`,
    `background=` or CSS `url(http…)`), found by a simple scan in the engine.
  - `cid:` images (pictures embedded in the message itself) are a separate feature and are not
    part of this milestone.
- **Out of scope**: remembering senders, a global setting, `http:` images, `cid:` images,
  proxying images through a privacy service.

### T164 — Engine: detect remote images and allow them in the policy
- status: done
- depends on: T161
- parallel-safe with: none
- files: src/engine/messagerenderer.h, src/engine/messagerenderer.cpp,
  autotests/messagerenderertest.cpp (the policy was defined in messagerenderer.h itself)
- inputs: `messageBodyHtml(...)`, `cspMeta()`, the current policy (no remote loads at all).
- output: `bool htmlHasRemoteImages(const QString &html)`; `messageBodyHtml` takes a flag
  `allowRemoteImages` (default false) that adds `img-src https:` to the policy for that render
  only (other directives unchanged; the policy text must still start the document as it does
  today); the flag has no effect on plain text. The existing callers keep their behaviour.
- verify: build succeeds; renderer test rows: a mail with `<img src="https://…">` → true; with
  `http://`, with CSS `url(https://…)`, with only `cid:` → (http and https true, cid false); the
  policy contains `img-src https:` only when the flag is set and never lists `http:`, scripts or
  frames; `ctest --preset dev` passes.
- notes: Done 2026-10-09. `messageContentSecurityPolicy(bool allowRemoteImages = false)` is now a
  function (was an inline constexpr string; only the renderer and its test used it). Off:
  `default-src 'none'; style-src 'unsafe-inline'; img-src data: cid:` (unchanged); on: `… img-src data:
  cid: https:`. `messageBodyHtml(…, link, bool allowRemoteImages = false)`; CSP meta still first;
  plain text ignores the flag. `htmlHasRemoteImages()`: img/source/input src|srcset with
  http(s):// or //, `background=` http(s), CSS `url(` http(s) or //; cid:/data: don't count (any
  `<input src>` counts; srcset checked at its first URL). 3 new tests (12 detection rows); ctest 17/17.

### T165 — Message view: "Load images" bar
- status: done
- depends on: T164
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp, src/qml/MessagePage.qml (also
  src/engine/messagerenderer.{h,cpp} + autotests/messagerenderertest.cpp for the engine helper)
- inputs: T164; `showMessage()` builds `m_messageHtml`; the `WebEngineView` has
  `settings.autoLoadImages: false`.
- output: property `messageHasRemoteImages` (true when the open message has remote images and they
  are not loaded yet) and `Q_INVOKABLE void loadRemoteImages()` that rebuilds `m_messageHtml` with
  the flag, emits `currentMessageChanged` and sets the property false; a thin bar above the body
  ("This message has images that were not loaded.  [Load images]", theme colours, an info icon)
  shown only while the property is true; the web view's `autoLoadImages` follows a QML property
  that is true only after the button was pressed for the open message and resets on every other
  message. Opening another message always starts blocked.
- verify: build succeeds; real qmllint (`cmake --build --preset dev --target all_qmllint`) adds no
  warnings over the baseline; `ctest --preset dev` passes; offscreen start with a throw-away
  profile and a seeded local message that has an `<img src="https://…">` (the tester may use the
  screenshot hook): the bar shows; after `loadRemoteImages()` it is gone. No network is used.
- notes: Done 2026-10-09. Engine `messageHasRemoteImages(const KMime::Message&)` scans only the mail's
  text/html part (mainBodyPart), 7 test rows. MailController: `messageHasRemoteImages`,
  `remoteImagesLoaded`, `loadRemoteImages()` (rebuilds with allowRemoteImages=true, same colours);
  both flags reset in showMessage/clearMessage; a background sync does not re-show the open message.
  MessagePage: Kirigami.InlineMessage + "Load images" action; showHtml() sets
  `view.settings.autoLoadImages = MailController.remoteImagesLoaded` right before loadHtml, so
  another message never inherits it. Seeded offscreen screenshots (no network): bar on the remote-image
  mail, none on a plain HTML mail; QtTest probe 7/7; ctest 17/17; lint 20 (= baseline). Loading
  reloads the page (scroll resets); refs outside the scan (SVG image, video poster) show no bar and
  stay blocked.

### T166 — Manual check: load images
- status: done
- depends on: T165
- parallel-safe with: none
- files: none
- inputs: the user's desktop; a newsletter with pictures.
- output: confirmed: pictures are blocked and the bar shows; "Load images" shows them; another
  message is blocked again; a message without images shows no bar.
- verify: manual, with the user.
- notes:
  Closed 2026-10-09: the user tried Load images and said "it is good now".
  Skipped 2026-10-09 on the user's decision. Confirmed earlier: the look, colours and sizes of the bar ("looks fine"). Never checked live: the network-off test (red chip, error tooltip, click to reconnect), the live "Syncing …" text, unread count vs tray tooltip.

# Milestone 3h — Drag and drop attachments (user request 2026-10-09)

- **Goal**: files can be dragged from the file manager onto the compose window to attach them,
  as an addition to the Attach button.
- **Decisions** (made while planning; change only with the user):
  - Reuse `composer.addAttachments(urls)` (it already asks `MailController.fileInfo(url)`, skips
    duplicates, and the 25 MB rule and the chips apply as with the button). No new C++.
  - Only local files are accepted (`file:` URLs); anything else (text, remote URLs) is ignored
    and the drop is not accepted, so dropping text into the body field keeps working. A folder
    is ignored (as `fileInfo` already does, to be confirmed).
  - While the message is being sent (`composer.waiting` or `MailController.sending`) a drop is
    ignored.
  - While files are dragged over the window a translucent overlay with the attach icon and the
    text "Drop files to attach" shows; it hides on drop or when the drag leaves.
- **Out of scope**: dragging out of the message view (saving attachments by drag), dragging
  inline images into the body, dropping onto the main window to start a new message.

### T167 — Compose window: drop files to attach
- status: done
- depends on: T156
- parallel-safe with: none
- files: src/qml/ComposeWindow.qml
- inputs: `addAttachments(urls)`, `attachDialog`, `MailController.fileInfo(url)`.
- output: a `DropArea` filling the window content (`keys` not needed; `onEntered` accepts only
  when `drag.hasUrls` and every URL is a `file:` URL, else `drag.accepted = false`; `onDropped`
  calls `composer.addAttachments(drop.urls)` unless sending), an overlay as described above bound
  to `dropArea.containsDrag`, with theme colours and `Accessible` text; the text fields keep
  working for text drops.
- verify: real lint (`cmake --build --preset dev --target all_qmllint`) adds no warnings over the
  baseline (2); build succeeds; `ctest --preset dev` passes; offscreen start with a throw-away
  profile prints no new QML warnings; a test hosting the real `ComposeWindow.qml` with a stand-in
  controller (as in the earlier T156 test) sends a synthetic drop (`QTest`/`QQuickWindow` drag
  events or `Drag` simulation, whichever works offscreen) with two files and checks two chips;
  with a text-only drop checks no chip; if a synthetic drop is not possible offscreen, say so in
  the notes and rely on the user's check.
- notes: Done 2026-10-09. DropArea (z 100, fills the Page content incl. attachment row and footer) with
  `acceptsFiles(urls, hasUrls)`: only file: URLs, not while waiting/sending; `accept(Qt.CopyAction)`
  (`dropAction` is flagged by qmllint); onDropped → addAttachments. Overlay: Qt.alpha(background, .85),
  highlight border, mail-attachment icon, "Drop files to attach", Accessible StaticText. Synthetic drop
  works offscreen: scratch C++ harness sending QDragEnter/Move/Drop/Leave events to the real
  ComposeWindow with a stub controller, 33/33: two files → 2 chips; text, mixed file+https, while
  waiting/sending, under the modal discard dialog → rejected; folder → no chip (silently); overlay
  shows/hides; input not blocked. Not verifiable offscreen: text dropped into the body (offscreen
  TextEdit takes no drops on HEAD either) → T168. Lint 2, ctest 17/17. Probe note: since
  qt6-declarative 6.12 + ki18n 6.30, QML probes that stub org.kde.ki18n must list the stub import
  paths last (`-import /usr/lib/qt6/qml -import <ki18nstub> -import <stubs>`).

### T168 — Manual check: drag and drop attachments
- status: done
- depends on: T167
- parallel-safe with: none
- files: none
- inputs: the user's desktop and file manager (Dolphin).
- output: confirmed: dragging one and several files from Dolphin onto the compose window attaches
  them (chips and total), the overlay shows while dragging, duplicates are not added twice, a file
  over 25 MB in total gives the red message and disables Send, and dropping text still works in
  the body.
- verify: manual, with the user.
- notes:
  Closed 2026-10-09: the user tried dragging files from Dolphin onto the compose window and said "works" (a general confirmation; the text-drop and over-25-MB cases were not reported separately).


# Milestone 6 — IMAP IDLE (push mail) (user request 2026-10-09)

- **Goal**: new mail shows up within seconds instead of at the next timer tick (today
  `CheckMinutes`, 1 minute in the user's config). Works for Yahoo and Gmail, and falls back to the
  timer wherever IDLE is not possible, so mail is never missed.
- **How it works today**: `MailController` has a `QTimer` that calls `checkInbox(session)` for
  each connected account (T125); `ImapClient::checkInbox` sends a fresh SELECT + SEARCH UNSEEN
  and answers `inboxChecked`; `NewMailTracker` decides what is new; the tray, notification and the
  open list react (T126–T127, `refreshInboxView`). IDLE only has to *trigger* that same path.
- **Decisions** (made while planning; change only with the user):
  - **A second connection per account** just for IDLE (KIMAP `IdleJob` occupies its session until
    it is stopped, so the normal session cannot be shared). It logs in the same way as the main
    session (password, or the OAuth token for Google), selects the INBOX read-only
    (`EXAMINE`), runs `IdleJob`, and has no other job. One IDLE connection per account, never more.
  - When the server pushes any change (new message, expunge, flags), the watcher only emits
    `inboxChanged()`; the controller then runs the existing `checkInbox(session)`, coalescing
    bursts (at most one check at a time and one more queued; a short delay of about 1 s to merge
    several pushes). No mail data is read on the IDLE connection.
  - IDLE is restarted every 20 minutes (servers drop it after about 29–30 minutes) and the
    connection is re-made after a loss with a growing wait (30 s, 1 min, 2 min, 5 min, then stays
    at 5 min), reset when it works for a while. No reconnect storm.
  - No IDLE capability, a failed login, or a connection that keeps failing → the watcher reports
    `Unavailable` / `Failed` and the polling timer keeps covering that account.
  - **The timer stays** as a safety net: for an account whose watcher is idling, the timer
    checks only every 15 minutes (a silent dead connection is found at the latest then); for the
    others it uses `CheckMinutes` as now.
  - A setting "Push mail (IDLE) when the server supports it", on by default, saved in
    `plasmamailrc` next to the other notification settings. Off = polling only, as today.
  - The IDLE connection is also closed when the account is removed, when the app quits, and when
    the main session fails or is closed.
  - Logs: states only ("push active for <account>"), never message data; the existing rule about
    tokens in logs stays.
- **Out of scope**: IDLE on folders other than the INBOX, NOTIFY (RFC 5465), push while the app
  is not running, Gmail threads, a setting for the restart interval.

### T169 — Idle policy: restart and reconnect timing
- status: done
- depends on: T125
- parallel-safe with: none
- files: src/engine/idlepolicy.h, src/engine/idlepolicy.cpp, src/CMakeLists.txt (if sources are
  listed), autotests/idlepolicytest.cpp, autotests/CMakeLists.txt
- inputs: the decisions above.
- output: a small pure class `IdlePolicy` (no Qt event loop needed): `restartInterval()` (20 min),
  `nextReconnectDelay()` (30 s, 1 min, 2 min, 5 min, 5 min, …), `reportConnected()` /
  `reportStable()` (reset after the connection worked for 2 minutes), `reportFailure()`;
  `pollInterval(bool idling, int checkMinutes)` (15 min while idling, else `checkMinutes`).
- verify: build succeeds; the test covers the delay sequence, the reset after stability, a
  failure right after connecting (no reset), and `pollInterval`; `ctest --preset dev` passes.
- notes: Done 2026-10-09. `IdlePolicy` (pure, time passed in): `restartInterval()` 20 min,
  `stableDuration()` 2 min, `pollInterval(idling, checkMinutes)` (15 min / max(1, checkMinutes)),
  `reportConnected(now)`, `reportFailure(now)` (resets the sequence only if the connection had been up
  ≥ 2 min; always clears the connected state), `nextReconnectDelay()` 30/60/120/300/300 s (call
  reportFailure first). idlepolicytest; ctest 18/18.

### T170 — Engine: `ImapIdleWatcher`
- status: done
- depends on: T169
- parallel-safe with: none
- files: src/engine/imapidlewatcher.h, src/engine/imapidlewatcher.cpp, src/CMakeLists.txt,
  autotests/imapidlewatchertest.cpp, autotests/CMakeLists.txt
- inputs: how `ImapClient::session()` creates and logs in a `KIMAP::Session` (password and
  OAuth), `KIMAP::IdleJob`, `IdlePolicy`.
- output: class `ImapIdleWatcher : QObject` with `start(const Account &, credentials)` / `stop()`,
  states `Stopped, Connecting, Idling, Unavailable, Failed`, signals `stateChanged()` and
  `inboxChanged()`. Behaviour as in the decisions: second session, login like the main session
  (share the code if it can be done without a big refactor; otherwise a small duplicate that is
  recorded in the notes), CAPABILITY check for IDLE (else `Unavailable`), EXAMINE INBOX, IdleJob
  with a 20-minute restart, reconnect with `IdlePolicy` delays, every KIMAP job tied to a
  generation counter like `ImapClient` so late answers of an old connection are ignored. The
  watcher never reads messages.
- verify: build succeeds; `ctest --preset dev` passes; a test for the parts that run without a
  server (start with an unreachable host → `Failed` then retries with the delays using a fake
  clock or a short override; `stop()` while connecting emits nothing afterwards); real IMAP is
  checked in T173 (corrected 2026-10-09: the plan said T174).
- notes: Done 2026-10-09. KIMAP IdleJob (header `<KIMAP/IdleJob>`, file kimap/idlejob.h): signals
  `mailBoxStats` and `mailBoxMessageFlagsChanged` (both → inboxChanged), `stop()` sends DONE, no
  "idling started" signal (state Idling right after start), no expunge signal. IdleJob disables the
  session timeout while idling and restores it on stop (KIMAP 26.08.2, checked by disassembly; a 70 s
  quiet IDLE held). `start(account, CredentialsProvider)` asks for the secret before every login (OAuth
  token refresh); null provider → Failed, no retry. Login code duplicated from ImapClient (tied to its
  members), noted in a comment. reportConnected once per login, not per 20-min restart. Logs: state
  names only. Tests (19/19): closed-port failures + retries, empty secret, stop semantics, and a
  fake implicit-TLS IMAP server on 127.0.0.1 (throw-away self-signed cert embedded in the test):
  EXAMINE + IDLE + `* 3 EXISTS` → inboxChanged; no IDLE capability → Unavailable, no retry; server
  drop → reconnect and idle again; 300 ms restart keeps one connection. Not testable here: a real
  server (T173).

### T171 — Controller: use the watcher, poll less
- status: done
- depends on: T170
- parallel-safe with: none
- files: src/app/accountsession.h, src/app/accountsession.cpp, src/app/mailcontroller.h,
  src/app/mailcontroller.cpp
- inputs: T170; `AccountSession` (owns the client and connection state), the check timer and
  `checkInbox(session)`.
- output: each `AccountSession` owns a watcher that starts when the main session is connected and
  its folders are listed (with the same credentials) and stops with the session; on
  `inboxChanged()` the controller runs the existing `checkInbox(session)` with the coalescing
  rule; the timer uses `IdlePolicy::pollInterval` per account (accounts with an idling watcher
  every 15 minutes, others every `CheckMinutes`); the watcher is stopped when an account is
  removed or the app quits and its states are logged without data.
- verify: build succeeds; `ctest --preset dev` passes; offscreen start with a throw-away profile
  and no account shows no new warnings; real lint stays at the baseline (2) if QML changed.
- notes: Done 2026-10-09. AccountSession owns one watcher (startWatcher/stopWatcher); provider returns
  `ImapClient::secret()` (in-memory password, or the current OAuth access token, renewed ~5 min
  before expiry by OAuthClient → setAccessToken). Start: onSessionFolders → startPush (gated by
  `m_pushEnabled`, used by T172), and on a session that becomes Connected again with known folders
  (tester fix: recovery without a new folder list). Stop: sessionLost, Failed state, reconnect
  (connectWithSecret), discardSession/removeAccount, aboutToQuit, destructor. Non-fatal NO and
  token renewal don't stop it (no churn). inboxChanged → 1 s debounce → checkInbox, or queued once
  if a check runs (onInboxChecked runs it). Check timer ticks every 30 s; per account
  `IdlePolicy::pollInterval(idling, CheckMinutes)` with m_lastCheck (worst case CheckMinutes + 30 s).
  Log "Push state for account #n: <State>" (no address). Checked by reading + ctest 19/19 + offscreen;
  needs a real server: start/Idling, push → check ~1 s, 15-min poll, one watcher after reconnect,
  OAuth token in the watcher login → T173. Follow-ups → T174.

### T172 — Setting and status: push on/off, push state in the chip tooltip
- status: done
- depends on: T171
- parallel-safe with: none
- files: src/engine/appsettings.{h,cpp} + autotests/appsettingstest.cpp (the settings class),
  src/models/accountlistmodel.{h,cpp} + autotests/accountlistmodeltest.cpp (live role),
  src/app/mailcontroller.h, src/app/mailcontroller.cpp, src/qml/NotificationSettingsPage.qml,
  src/qml/StatusBar.qml
- inputs: T171; `AppSettings` with `saveAppSettings(bool keepInTray, int checkMinutes)`.
- output: the setting "Use push (IDLE) when the server supports it" (default on) in
  Settings → Notifications; turning it off stops all watchers at once and the timer goes back to
  `CheckMinutes` for everyone; turning it on starts them. The account chip tooltip in the status
  bar gets one more line: "Push: active / connecting / not supported by the server / off".
- verify: build succeeds; real lint stays at the baseline (2); `ctest --preset dev` passes;
  offscreen start with a throw-away profile shows no new warnings.
- notes: Done 2026-10-09. `[General] PushEnabled` (default true, appsettingstest round-trip).
  MailController: `pushEnabled` property (NOTIFY appSettingsChanged) + `setPushEnabled(bool)` (saves;
  off stops all watchers so polling falls back to CheckMinutes; on starts them for connected sessions
  with folders; no second watcher). AccountListModel role `pushText` ("Push: active / connecting / not
  supported by the server / off"), dataChanged per row on pushStateChanged and for all rows on
  setPushEnabled (testPushText). Settings → Notifications switch "Use push (IDLE) when the server
  supports it" (screenshots on/off, throw-away profile); StatusBar chip tooltip gets the push line
  (not hoverable offscreen; binding read). ctest 19/19, lint 2. Non-"off" texts need a server (T173).

### T173 — Manual check: push mail
- status: waiting (manual check with the user)
- depends on: T172
- parallel-safe with: none
- files: none
- inputs: the user's desktop; the Yahoo and Gmail accounts; a second way to send mail to
  them (the other account, or webmail).
- output: confirmed: the tooltip says "Push: active" for both accounts a few seconds after start;
  a message sent from webmail shows up (list, notification, tray count) within about 10 seconds
  although `CheckMinutes` is longer; turning the network off and on makes the chips recover and
  push come back by itself; the setting off falls back to polling.
- verify: manual, with the user.
- notes:

### T174 — Push follow-ups found in T171
- status: done
- depends on: T171
- parallel-safe with: none
- files: src/app/mailcontroller.cpp, src/engine/imapclient.h, src/engine/imapclient.cpp
- inputs: tester notes of T171.
- output: (a) when `ImapClient::setAccount()`/`disconnectFromServer()` drop an in-flight
  checkInbox silently (generation bump), MailController clears `m_checking` for that account (e.g.
  extend T145's `jobsDropped()` to all pending requests, not only moves) so polling and push don't
  stop for that account; (b) a push that arrived during a check that then failed is retried once
  after a short delay instead of waiting for the 15-minute poll; (c) the push-state log line in
  ~AccountSession prints "#0" — skip logging for a session no longer in the list.
- verify: build; `ctest --preset dev`; offscreen start clean.
- notes: Done 2026-10-10. (a) ImapClient keeps `m_checkGeneration` (set in checkInbox, cleared when
  inboxChecked is emitted); disconnectFromServer/setAccount emit `jobsDropped()` also when a check of the
  current generation was pending; the controller's jobsDropped handler clears `m_checking` for the
  account (and updates the activity). (b) `retryQueuedCheck()`: a push queued behind a check that failed
  or was dropped runs checkInbox once after 5 s. (c) the push-state log is skipped for a session that is
  not in `m_sessions` (deleted, or still pending verification). Build, ctest 19/19, offscreen demo start
  clean. No unit test: ImapClient has no fake-server test harness in autotests/.
  Verification 2026-10-10 (review of 36bbd87): (a) `m_checkGeneration` is cleared on every path that
  ends a check: inboxChecked, the empty-result path, and `fail()` (the generation then no longer
  `isCurrent`, so a later disconnect does not emit a spurious jobsDropped). A non-fatal error in
  checkJob() (errorOccurred, session kept) leaves it set; the controller's onSessionError already
  clears `m_checking`, so the only effect is one harmless jobsDropped() at the next disconnect.
  Fixed: `~ImapClient()` now clears `m_checkGeneration` before disconnectFromServer(), as it already
  does for `m_pendingMoves`; otherwise a check pending at app quit (sessions are children of
  MailController) emitted jobsDropped() from the destructor into a half-destroyed AccountSession
  (`session->account()` after its `m_account` member is gone). (b) `retryQueuedCheck()` runs only
  when `m_checkQueued` held the session, so it is one-shot: a failed retry queues nothing, so it
  cannot loop; the 5 s singleShot has the session as context, so it is dropped when the session is
  deleted, and `isKnown()` covers removal. (c) the "#0" skip is in place. Build clean; ctest 19/19
  passed after the fix; offscreen demo start (timeout 20, isolated XDG dirs) exits by timeout with no
  warnings from the changed code (only Qt offscreen/Vulkan/QML messages at startup).

# Milestone 3i — Mimestream look (user request 2026-10-10: "match Mimestream's look as closely as possible")

Reference: the user's Mimestream dark-mode screenshot (session image `2.png`, kept outside the repo).
Sampled colours (dark):

| Element | Mimestream | Notes |
|---|---|---|
| Sidebar background | `#4c4951` | macOS vibrancy over the wallpaper; we use it solid |
| Sidebar selected row | `#68656b` | rounded, inset ~8 px |
| Sidebar text / section header / icons | `#f3eff5` / `#8c8991` / `#cfc8ba`-ish | header small, semi-bold, not uppercase |
| Title bar band (list header + reading toolbar) | `#251e2c` (`#241d29`) | the deep purple is only here, not in the list |
| List and reading background | `#1e1e1e` | neutral, not purple |
| List selected row | `#464646` | neutral grey, rounded, inset ~12 px; no row separators |
| Sender / subject / preview / date | `#ececec` bold / `#e8e8e8` / `#aaaaaa` / `#a4a4a4` | three lines; date top right |
| Secondary text (reading pane dates) | `#ababab` | |
| Column dividers | `#010101`–`#060606`, 1 px | black hairline |
| Toolbar icons | `#9f99a2` | |
| Accent (menu selection) | `#1866c5` | |
| Label chips (bg, text white) | Inquiries `#b4d1f8` (dark text), Travel `#b69dfe`, Home `#1ba26b` | |
| Label tag icons | Awaiting `#f78671`, Delegated `#ffbd66`, Follow Up `#f8d075`, Home `#2ecb94`, Inquiries `#a9c6e6`, Travel `#bba3fd`, Website `#bebbc1` | |

Light mode (user addition 2026-10-10: light must match too) has no reference; use Mimestream's
known light look and derive it (macOS light: sidebar `#e8e6ee`-ish, band `#f3f1f6`, list
`#ffffff`, selection `#dcdcdc`, text `#1c1b22`, secondary `#6e6e73`, accent `#0a60d6`).

- **Open questions**:
  1. Labels: the app has no labels. Mimestream's chips and the "Personal" tag list are Gmail labels.
     Real support (Gmail `X-GM-LABELS`, IMAP keywords for Yahoo) is a feature, not styling — build it?
     Until answered, T180 only adds the chip colours to MimeTheme; nothing shows them.
  2. Task IDs: the user asked to start at T174, but T174 already exists (push follow-ups), so this
     milestone starts at T175.
  3. `PLASMAMAIL_SCREENSHOT_OPEN=1` opens the newest message and may mark it read on the server.
     Is that fine for checking the reading pane, or should the hook get a no-mark-read mode?
  4. The sidebar is translucent (vibrancy) in Mimestream. KWin blur behind the sidebar is possible
     but out of scope unless the user wants it.

### T175 — First Mimestream palette pass (work already done)
- status: done
- depends on: none
- parallel-safe with: none
- files: src/qml/MimeTheme.qml (new), src/CMakeLists.txt, src/qml/FolderSidebar.qml,
  src/qml/StatusBar.qml, src/qml/MessageListPage.qml, src/qml/MessagePage.qml,
  src/app/mailcontroller.h, src/app/mailcontroller.cpp, src/engine/messagerenderer.h,
  src/engine/messagerenderer.cpp, autotests/messagerenderertest.cpp
- inputs: the uncommitted working tree of 2026-10-10.
- output: MimeTheme singleton (dark/light from `Qt.styleHints.colorScheme`) applied through
  `Kirigami.Theme.colorSet: Custom`; `MailController::setMessageColours` feeds text, background and
  link colours to the body HTML; `readableLinkColour` minimum contrast 4.5:1; unread rows keep bold
  sender and dot, lose accent bar and tint.
- verify: `cmake --build build -j8`; `ctest --test-dir build` all pass.
- notes: committed as-is 2026-10-10; build clean, ctest 19/19.

### T176 — Palette: sampled Mimestream colours
- status: done
- depends on: T175
- parallel-safe with: none
- files: src/qml/MimeTheme.qml
- inputs: the table above.
- output: dark values replaced with the sampled ones (list `#1e1e1e`, reading `#1e1e1e`, sidebar
  `#4c4951`, selections `#68656b`/`#464646`, accent `#1866c5`, link stays readable `#5ea2ff`-ish
  ≥ 4.5:1 on `#1e1e1e`); new tokens `headerBackground`, `columnDivider`, `sectionText`,
  `previewText`, `iconTint`, `toolbarIcon`; light values derived. Existing token names kept.
- verify: build; ctest; screenshot shows neutral list/reading background and grey sidebar.
- notes: committed 2026-10-10; text #ececec, secondary #a4a4a4, section #8c8991, preview #aaaaaa, iconTint #cfc8ba, toolbarIcon #9f99a2, header #251e2c, columnDivider #050505; light: header #f3f1f6, listSelected #dcdcdc, columnDivider #d0d0d4. Tokens not used elsewhere yet. Build clean, ctest 19/19.

### T177 — Title bar band and column dividers
- status: done
- depends on: T176
- parallel-safe with: none
- files: src/qml/MessageListPage.qml, src/qml/MessagePage.qml, src/qml/FolderSidebar.qml,
  src/qml/Main.qml (only if the dividers live there)
- inputs: `MimeTheme.headerBackground`, `columnDivider`, `toolbarIcon`.
- output: the list page title bar and the reading-pane toolbar use `headerBackground`; the sidebar
  header stays on the sidebar colour (as in Mimestream); 1 px `columnDivider` between the three
  columns; toolbar icons tinted `toolbarIcon`.
- verify: build; ctest; screenshot: purple band across list and reading headers only, black hairlines.
- notes: list and message pages replace the page row header's background (ToolBarPageHeader, via the ColumnView.globalHeader loader) with a headerBackground rectangle; page row separators off, own 1 px columnDivider on sidebar and list right edges (list also in the header band); Action/ToolButton icon.color toolbarIcon, 40% alpha when disabled, flag keeps orange. Not checked on screen.

### T178 — Message list rows like Mimestream
- status: done
- depends on: T186
- parallel-safe with: none
- files: src/qml/MessageListPage.qml
- inputs: palette tokens.
- output: three lines: sender (bold, `text`) with date right (`secondaryText`, small); subject
  (normal weight, `text`); preview (`previewText`, one line, elided). No row separators. Selection
  and hover are rounded rects inset from the list edges (about `largeSpacing`), selection
  `listSelected`. Unread dot kept, unread sender bold, read sender DemiBold. Existing icons
  (attachment, flag) kept, tinted `secondaryText`.
- verify: build; ctest; screenshot compared with the reference list column.
- notes: rows are three lines (sender MimeTheme.text bold/DemiBold with small secondaryText date, subject text, one-line elided previewText); hairline dividers removed; background rect radius 6 inset 12 px, listSelected / hover; flag icon tinted secondaryText (orange when flagged). The row has no attachment indicator (not in the model), so none added. Screenshots checked dark and light; clicks, menu and drag untouched.

### T179 — Sidebar like Mimestream
- status: done
- depends on: T186
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml
- inputs: palette tokens.
- output: account/section headers small, DemiBold, `sectionText`; rows a bit taller, rounded
  selection `sidebarSelected` inset ~`smallSpacing`; selected row text stays normal weight; folder
  icons tinted `iconTint` (selected: `text`); unread counts `secondaryText`.
- verify: build; ctest; screenshot compared with the reference sidebar.
- notes: done 2026-10-10. Section labels ("All Accounts") and account header rows (arrow, avatar kept, email) are small DemiBold `sectionText`; folder rows ~30 px (padding smallSpacing+3), text `MimeTheme.text` normal weight even when selected, icons monochrome (role/shared colours and roleColor/sharedColor removed) `iconTint`, `text` when selected; hover `MimeTheme.hover`. Selection was already `sidebarSelected` inset smallSpacing. The sidebar shows no unread counts yet (model has none), so none added. Screenshots: scratchpad t179-dark.png, t179-light.png.

### T180 — Reading pane header and label chip colours
- status: done
- depends on: T186
- parallel-safe with: none
- files: src/qml/MessagePage.qml, src/qml/MimeTheme.qml
- inputs: palette tokens; open question 1.
- output: subject as a large bold heading; sender line `text`, date right-aligned `secondaryText`;
  header separator uses `divider`; MimeTheme gets `labelColours` (the seven tag colours above) and a
  `chipColour(name)` helper for later — not shown anywhere until labels exist.
- verify: build; ctest; offscreen start without QML warnings.
- notes: done 2026-10-10. Subject large bold in `text`; sender name Bold `text`, address and "To:" `secondaryText`, date right-aligned `secondaryText`; separator is a 1 px `divider` Rectangle (was an opacity-0.5 Kirigami.Separator); avatar kept; toolbar untouched. MimeTheme: `labelColours` (tag colours, deeper in light), `chipBackgrounds` (Home/Inquiries/Travel chip bg), `chipColour(name)` (known name, else stable hash into the palette), `chipTextColour(bg)` (white/dark by luminance). Unused until labels exist (open question 1). Screenshots: scratchpad t180-dark.png, t180-light.png.

### T181 — Visual check, dark mode
- status: superseded
- depends on: T178, T179, T180, T182, T183
- parallel-safe with: none
- files: none (fixes go back to the owning task as new tasks)
- inputs: reference image; `PLASMAMAIL_SCREENSHOT=<png> build/bin/plasma-mail` with
  `PLASMAMAIL_COLOR_SCHEME=dark` and the plain sample (T183); no `_OPEN` (open question 3).
- output: a screenshot of the app next to the reference with sampled colours of sidebar, band,
  list, selection, text; differences listed; contrast of text, secondary, link, selected row, hover, dividers, unread dot
  measured as in T184.
- verify: sampled colours within about 6 per channel of the table (dark mode).
- notes: superseded by T196 (system colours); the 2026-10-10 tester run was interrupted, nothing verified.

### T182 — Dev hook: force the colour scheme
- status: done
- depends on: T176
- parallel-safe with: T183
- files: src/app/main.cpp, src/qml/MimeTheme.qml (only if it needs a property to follow)
- inputs: user addition 2026-10-10: "both dark AND light mode must match Mimestream".
- output: `PLASMAMAIL_COLOR_SCHEME=light|dark` (dev only, like the screenshot hooks) forces the
  whole app into that scheme: `QGuiApplication::styleHints()->setColorScheme()` and, so that
  Kirigami's non-Custom colour sets follow too, the matching KDE colour scheme (BreezeLight /
  BreezeDark via KColorSchemeManager or the palette) if that is how the app gets its colours.
  `MimeTheme.dark` follows. Unset: unchanged behaviour.
- verify: build; ctest; two screenshots with the hook, light and dark, show the two palettes.
- notes:
  Done 2026-10-10. `PLASMAMAIL_COLOR_SCHEME=light|dark` in main.cpp: `styleHints()->setColorScheme()`
  plus `KColorSchemeManager::instance()->activateScheme("Breeze Light"/"Breeze Dark")` (new link:
  KF6::ColorScheme). Findings: with the KDE platform theme (xcb) `setColorScheme` is ignored (stays
  at the system scheme; the gtk3 theme honours it), and Kirigami's non-Custom sets (title band,
  buttons) read the KDE scheme, not the QPalette, so only the manager moves them. Hence
  `MimeTheme.dark` now reads a `SystemPalette` window lightness (< 0.5) instead of
  `Qt.styleHints.colorScheme`. Also fixed in a separate commit: the Custom colour property names
  (`Kirigami.Theme.customTextColor` etc. do not exist in Kirigami 6.30; they are `textColor`,
  `backgroundColor`, `disabledTextColor`, `highlightColor`), which had made the app fail to load
  since T175. Screenshots: scratchpad t182-light.png, t182-dark.png; ctest 19/19. Observation for
  T184/T181: in both schemes the sidebar and title band come out as the Breeze colours, not
  MimeTheme.sidebarBackground / headerBackground (the drawer ignores the Custom background).

### T183 — Dev hook: sample message in the reading pane
- status: done
- depends on: T175
- parallel-safe with: T182
- files: src/app/main.cpp, src/app/mailcontroller.h, src/app/mailcontroller.cpp,
  autotests/data/ (new sample files)
- inputs: open question 3 (opening a real message may mark it read).
- output: `PLASMAMAIL_SCREENSHOT_SAMPLE=<file.eml>` shows that local message in the reading pane
  through the normal parse + render path (headers, body, links, attachments list) without touching
  any account, server or flag. Two samples in the repo: a plain/simple HTML mail with links, and an
  HTML newsletter with its own colours (white/coloured backgrounds, dark text, coloured links).
- verify: build; ctest; screenshot with each sample in both schemes.
- notes:
  Done 2026-10-10. `PLASMAMAIL_SCREENSHOT_SAMPLE=<file.eml>` (main.cpp, a third of the way into the
  delay) calls the new `MailController::showSampleMessage(raw)`: parses like openMessage and sets the
  same properties as showMessage() (subject/from/to/date/html/remote images/attachments) but no
  session, store, mark-seen or move; currentUid is set to 1 so the pane shows it, and as no account
  or folder is set, the message actions are no-ops. Samples: autotests/data/sample-plain.eml and
  sample-newsletter.eml (example.com). Screenshots (scratchpad t183-<plain|newsletter>-<dark|light>.png):
  the pane shows both. Observation: the newsletter in dark mode keeps its own white/coloured cards on
  the dark pane (readable); the plain mail takes the theme colours. ctest 19/19.

### T184 — Visual check, light mode
- status: superseded
- depends on: T178, T179, T180, T182, T183
- parallel-safe with: none
- files: none (fixes become new tasks)
- inputs: Mimestream's known light look (white list and reading pane, light grey sidebar, blue
  accent, dark text); `PLASMAMAIL_COLOR_SCHEME=light`.
- output: screenshots (list, sidebar, reading pane with the plain sample); contrast of text,
  secondary text, link, selected row, hover, dividers and unread dot measured (WCAG ratio).
- verify: text ≥ 7:1, secondary and link ≥ 4.5:1, selected row and dividers visible (≥ 1.2:1
  against the background), unread dot ≥ 3:1.
- notes: superseded by T196 (system colours); the 2026-10-10 tester run was interrupted, nothing verified.

### T185 — Visual check, HTML mail with its own colours (dark)
- status: superseded
- depends on: T183, T182
- parallel-safe with: T184
- files: none (fixes become new tasks)
- inputs: the newsletter sample; `PLASMAMAIL_COLOR_SCHEME=dark`.
- output: screenshot; the mail's own colours stay readable (no dark-on-dark text, no unreadable
  links, no white-on-white), both for the newsletter and the plain sample.
- verify: sampled text/background pairs ≥ 4.5:1.
- notes: superseded by T196 (system colours); the 2026-10-10 tester run was interrupted, nothing verified.

### T186 — Fix: sidebar and title band ignore MimeTheme
- status: done
- depends on: T177, T182
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml, src/qml/MessageListPage.qml, src/qml/MessagePage.qml,
  src/qml/Main.qml, src/qml/StatusBar.qml
- inputs: T182 notes and screenshots: in both schemes the sidebar drawer and the title band show
  the Breeze colours, not `sidebarBackground` / `headerBackground` (T177's globalHeader override has
  no visible effect).
- output: sidebar (incl. its header row "Mail") painted `sidebarBackground`; list title bar and
  reading-pane toolbar painted `headerBackground`; header text/icons readable on the band (set the
  band's Kirigami.Theme text colour from MimeTheme); status bar on `sidebarBackground`. Works in
  both forced schemes and survives narrow/wide switches.
- verify: build; ctest; screenshots with `PLASMAMAIL_COLOR_SCHEME=dark` and `=light`; sampled
  pixels: dark sidebar ≈ #4c4951, dark band ≈ #251e2c, light sidebar ≈ #e8e6ee, light band ≈ #f3f1f6.
- notes:
  Done 2026-10-10. Root causes (found with QT_FORCE_STDERR_LOGGING=1, since QML warnings otherwise
  go to the journal): `Kirigami.Theme.Custom` is undefined in 6.30 ("Unable to assign [undefined]
  to ColorSet"), so the colorSet lines are removed (the colour overrides with inherit false work
  without it); and T177's `as T.Control` cast of the header loader item was null (the item is a
  ToolBarPageHeader, an Item), now `as Kirigami.AbstractApplicationHeader`. The drawer template's
  background does not follow the instance's Theme, so FolderSidebar has an explicit background
  Rectangle (and a transparent-on-sidebar one for its "Mail" ToolBar). Sampled (dark): sidebar
  #4c4951, band #251e2c incl. search side, status #4c4951, list/reading #1e1e1e; (light): sidebar
  #e8e6ee, band #f3f1f6, status #e8e6ee, list/reading #ffffff. Header text keeps Breeze Header
  text colours, readable in both. Screenshots: scratchpad t186-dark.png, t186-light.png.

### T192 — Palette: sidebar text and light divider
- status: done
- depends on: T179
- parallel-safe with: none
- files: src/qml/MimeTheme.qml, src/qml/FolderSidebar.qml
- inputs: tweak left uncommitted by the interrupted T181/T184 check run.
- output: `sidebarText` token (dark #f3eff5, as sampled) used for sidebar names; `secondaryText`
  dark #a8a8a8; light `divider` alpha 0.10 (0.08 gave 1.19:1 on white, under the 1.2:1 floor).
- verify: build; ctest 19/19; screenshots in both schemes (t192-dark/light.png) clean, no QML warnings.
- notes: reviewed and committed 2026-10-10 by the planner.

### T199 — Fix: unread dot contrast and title band on schemes without Header colours
- status: done
- depends on: T196 (numeric part)
- parallel-safe with: none
- files: src/qml/MimeTheme.qml, src/qml/MessageListPage.qml, src/qml/MessagePage.qml
- inputs: T196 notes: unread dot (raw highlight) 2.49:1 on Breeze Light, 2.43:1 on Breeze Classic
  (floor 3:1); Breeze Classic has no `[Colors:Header]`, so Kirigami's Header set falls back to a
  dark band (41,44,48) on an otherwise light window.
- output: MimeTheme helpers `contrast(a, b)` (WCAG) and `readableOn(fg, bg, ratio)` (darken or
  lighten fg in HSL until the ratio is met; no hex); the unread dot uses
  `readableOn(activeHighlight, viewBackground, 3)`. The title bands use the Header set unless its
  background is on the other side of light/dark from the Window set (contrast of Header bg vs Window
  text < 4.5), then the Window set.
- verify: build; ctest; offscreen screenshots (`QT_QPA_PLATFORM=offscreen`, no window) for
  dark, light and "Breeze Classic"; dot ≥ 3:1 sampled; Classic band light.
- notes: MimeTheme gets luminance/contrast/readableOn (HSL lightness steps of 0.05, toward the side
  away from the background). Dot: readableOn(active highlight, View background, 3). Bands: a
  `useWindow` flag (Header bg vs Window text < 4.5) picks the Window set's background (and divider
  colour); a Binding on `pageStack.globalToolBar.colorSet` also switches the header contents to the
  Window set (a Binding on the header's own Theme was overridden by the toolbar's colorSet binding,
  leaving white text on the light band). Offscreen: Classic band (239,240,241), dark (41,44,48),
  light (222,224,226), unchanged; Classic title and icons now dark. No unread mail in the sample
  list, so the dot was computed in Python from the .colors files: Breeze Light 2.49 -> 3.21:1,
  Breeze Classic 2.43 -> 3.13:1 (25,152,218). Build, ctest 19/19. The real-window check stays with T196.

### T200 — Fix: sidebar header on schemes without Header colours
- status: done
- depends on: T199, T198
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml
- inputs: T196/T199 notes: on Breeze Classic (no `[Colors:Header]`) the sidebar's "Mail" row
  (logo, title, hide and menu buttons) stayed on Kirigami's dark Header fallback (41,44,48) while the
  sidebar body and list were light.
- output: the "Mail" ToolBar takes the Window set when the Header set's background would not read
  against the Window text (contrast < 4.5, same rule as the T199 bands), otherwise the Header set.
  Dark and Light unchanged.
- verify: build; ctest; offscreen screenshots for dark, light, "Breeze Classic"; sampled header
  pixel: Classic ≈ Window bg (239,240,241); dark and light equal to the T199 values.
- notes: `headerUseWindow` on the drawer root, from a `headerProbe` Item (Header set) in the
  column. The probe must stay visible (zero height, Layout.preferredHeight/maximumHeight 0): with
  `visible: false` Kirigami.Theme resolved the background to black, which wrongly switched Light to
  the Window set (239,240,241 instead of 222,224,226). The ToolBar sets `Kirigami.Theme.colorSet`
  itself (overriding the desktop style's Header default), so its background and row contents follow
  one set; T199's Binding-on-colorSet trick was not needed here since the ToolBar is ours.
  Sampled (1350x810 offscreen, point (150,8) and (150,38) in the sidebar row): dark (41,44,48),
  light (222,224,226), Breeze Classic (239,240,241). Dark and light equal the T199 baseline; list
  band unchanged (dark 41,44,48, light 222,224,226, Classic 239,240,241). Visual strip checked:
  white on dark, dark on grey and on Classic light. Build, ctest 19/19, no new QML warnings.
  Not seen in a real window (GUI runs paused).

### T201 — Sidebar headings at the folder rows' scale
- status: done
- depends on: T200
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml
- inputs: user feedback 2026-10-10: "All Accounts" and the account-name headings look oversized next
  to the folder rows (dark shot gui-dark-plain.png). Reference schemes from now on: Breeze Light and
  Breeze Dark (Classic only has to stay readable).
- output: "All Accounts" and account header rows use the folder rows' height and spacing; the
  account avatar is no larger than the folder icons (`iconSizes.small`); heading text small and
  DemiBold in `disabledTextColor`, aligned with the folder text column.
- verify: build; ctest; GUI shots via scripts/desktop3-run.sh in Breeze Dark and Light.
- notes: Account header rows take the folder rows' padding (left largeSpacing, top/bottom
  smallSpacing + 3) and mediumSpacing between arrow, avatar and text; avatar `iconSizes.small`
  (was smallMedium). SectionLabel's left margin is the folder text column (row margin + largeSpacing
  + small icon + mediumSpacing). Text was already smallFont DemiBold disabledTextColor. Verified:
  build, ctest 19/19, dark and light shots (t201-dark.png, t201-light.png) show the heading and
  account row at folder scale.

### T202 — Hairline between sidebar and list
- status: done
- depends on: T201
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml (or src/qml/Main.qml)
- inputs: T196 issue 3: no hairline on the sidebar/list edge (light: only a 1.14 background step).
- output: 1 px divider (text colour at `MimeTheme.columnDividerAlpha`) on the docked sidebar's right
  edge, like the list/reading divider; not shown when collapsed or modal.
- verify: build; ctest; GUI shots in Breeze Dark and Light, divider ≥ 1.2:1.
- notes: no code change: the divider T177 added in FolderSidebar.qml (1 px, right edge, full height
  incl. header row, text colour at MimeTheme.columnDividerAlpha, `visible: !sidebar.modal`, drawn above the
  content and under the PanelResizer, a transparent drag strip) is already on screen; the T196 "no
  hairline" reading did not reproduce. Collapsed (T187) the drawer is closed, so none. Real-window shots
  (sample-plain, 1350x810, x=266): Light 1.47:1 vs sidebar, 1.68:1 vs list (header row 1.45:1 both sides);
  Dark 1.89:1 vs sidebar, 2.17:1 vs list; Light collapsed: no line at the edge. Build ok, ctest 19/19.
  The desktop index changed between runs (outside them); no further runs made.

# Milestone 3j — Sidebar control (user request 2026-10-10: "a collapse option for the left panel;
# I don't need to see the whole tree of each account, I'd like to choose")

Queued after the Mimestream palette tasks (T175–T186); same file (FolderSidebar.qml).

Decisions 2026-10-10: hidden folders are display-only (still counted in All Inboxes, tray count and
notifications); Inbox can't be hidden. Visual checks: offscreen only (`QT_QPA_PLATFORM=offscreen`,
`PLASMAMAIL_COLOR_SCHEME=BreezeLight|BreezeDark`, `PLASMAMAIL_DEMO=1`, isolated XDG dirs); never
scripts/desktop3-run.sh or a visible window; never act on the user's real folders.

- **Open questions**:
  1. Hidden folders: do they still count in shared views (All Inboxes, Unread, Flagged), the tray
     count and notifications? Answered 2026-10-10: yes, hiding is display-only.

### T187 — Collapse the whole sidebar
- status: done
- depends on: T179
- parallel-safe with: none
- files: src/engine/appsettings.h, src/engine/appsettings.cpp, autotests/appsettingstest.cpp,
  src/app/mailcontroller.h, src/app/mailcontroller.cpp, src/qml/Main.qml, src/qml/FolderSidebar.qml,
  src/qml/MessageListPage.qml (for the "show sidebar" button while collapsed)
- inputs: panel widths T130–T131 as the pattern for persisted UI state.
- output: a sidebar toggle button (icon `sidebar-collapse`/`sidebar-expand`-style, Mimestream-like)
  in the sidebar header, and while collapsed in the list page's title band; shortcut Ctrl+B (check
  no conflict) hides/shows the docked sidebar column; `AppSettings::sidebarCollapsed` persisted and
  restored at start. Narrow windows keep the modal drawer behaviour.
- verify: build; ctest (new AppSettings test); screenshots collapsed/expanded in both schemes;
  restart keeps the state.
- notes: `AppSettings::sidebarCollapsed` ("SidebarCollapsed"), `MailController.sidebarCollapsed`
  (read/write property, saves at once). Icons sidebar-collapse-left / sidebar-expand-left in
  MimeTheme.toolbarIcon; Ctrl+B in Main.qml (no conflict) via `sidebar.toggle()`: wide flips the
  persisted flag, narrow opens/closes the drawer without persisting. The show button is in
  MessageListPage's titleDelegate, only while wide and collapsed. Dev env
  PLASMAMAIL_SIDEBAR_COLLAPSED=0|1 overrides for one run without saving. Screenshots: scratchpad
  t187-{expanded,collapsed}-{dark,light}.png. ctest 19/19.
  visual check done 2026-10-10: real window on Desktop 2 (runs via desktop3-run.sh). Expanded
  shows the sidebar and logo in dark, light and Breeze Classic; collapsed (dark and light) hides
  the sidebar and logo and shows the show-sidebar icon at the top left of the list band. Light
  collapsed captured during "Loading..." (busy stripe under the band), transient.

### T205 — Dev hook: demo accounts for headless checks
- status: done
- depends on: T202
- parallel-safe with: none
- files: src/app/main.cpp, src/app/mailcontroller.h, src/app/mailcontroller.cpp (models it fills)
- inputs: user 2026-10-10: visual checks only offscreen, never touch the real mailbox state; the
  installed app (~/.local/bin/plasma-mail) is running. A second instance on the same data dir could
  race the user's store, and the screenshot instance also registers a tray icon and can notify.
- output: `PLASMAMAIL_DEMO=1` (dev only) fills the sidebar and list with two fictional accounts
  (`alice@example.com`, `bob@example.com`) and folder trees (Inbox, Drafts, Sent, Trash, Archive,
  Projects/Alpha, Projects/Beta, Receipts, a few unread/flagged messages) in memory: no network, no
  secret store, no message store writes. While `PLASMAMAIL_SCREENSHOT` or `PLASMAMAIL_DEMO` is set,
  no tray icon and no notifications. Checks run with isolated
  `XDG_CONFIG_HOME/XDG_DATA_HOME/XDG_CACHE_HOME/XDG_STATE_HOME` under the scratchpad, so settings
  written during a check never reach the user's config.
- verify: build; ctest; offscreen shot (`QT_QPA_PLATFORM=offscreen`, isolated XDG dirs,
  `PLASMAMAIL_COLOR_SCHEME=BreezeDark`/`BreezeLight`) shows both demo accounts; no
  `org.kde.StatusNotifierItem` registration from the test process.
- notes: done in one piece (~130 lines). `PLASMAMAIL_DEMO=1`: MailController::loadDemo() builds two
  demo sessions (AccountSession got a `demo` ctor flag: the store is never opened, so no files),
  fills their folder models and a per-account demo message map; `loadAll`, legacy store move,
  check timer, keychain reads and connections are skipped. selectFolder() in demo mode lists from
  the demo map (INBOX only has messages; Unread/Flagged/All Inboxes filter it). Opening a message or
  other actions still reach for a server: not covered. main.cpp: tray and notifier are not created
  while PLASMAMAIL_DEMO=1 or PLASMAMAIL_SCREENSHOT is set. How to run a check: `QT_QPA_PLATFORM=offscreen`
  + isolated XDG_* dirs + `PLASMAMAIL_DEMO=1 PLASMAMAIL_COLOR_SCHEME=dark|light
  PLASMAMAIL_SCREENSHOT=<png>`; `dark`/`light`, `BreezeDark`/`BreezeLight` and "Breeze Dark" all work
  (see the 2026-10-10 line below; an unknown name warns on stderr and keeps the system scheme).
  2026-10-10 (T195 fix): the scheme file id now matches too (`BreezeDark`/`BreezeLight`, any case),
  and the unknown-scheme warning is written to stderr directly. Verified offscreen: `t205-dark.png`
  (list bg (20,22,24)) and `t205-light.png` (list bg (255,255,255)) both show alice@ and bob@
  example accounts, folder trees, unread dots and a flag; no StatusNotifierItem from the test pid;
  nothing written under the scratchpad xdg data dir; kdeglobals and plasma-mailrc mtimes unchanged.


### T188 — Account trees: persisted collapse, Collapse all / Expand all
- status: done
- depends on: T187, T194, T195, T199, T202, T205 (Milestone 3k code first)
- parallel-safe with: none
- files: src/engine/appsettings.h, src/engine/appsettings.cpp, autotests/appsettingstest.cpp,
  src/app/mailcontroller.h, src/app/mailcontroller.cpp, src/qml/FolderSidebar.qml
- inputs: FolderSidebar's per-account `expanded` property (default true, not persisted).
- output: each account's collapsed state stored in AppSettings keyed by account email and restored
  at start; "Collapse All" / "Expand All" in the sidebar header menu (or a context menu on the
  account header).
- verify: build; ctest (new test); toggling + restart keeps the states.
- notes: `AppSettings::collapsedAccounts` (QStringList, key "CollapsedAccounts" in [General]; KConfig
  escapes commas/backslashes, tested). Absent = open, so new accounts start open. MailController:
  read-only property `collapsedAccounts` (NOTIFY collapsedAccountsChanged, what the sidebar binds to),
  invokables isAccountCollapsed / setAccountCollapsed / setAllAccountsCollapsed, each saves at once.
  FolderSidebar: `expanded` is now derived from the list; header click toggles via the controller;
  "Collapse All" / "Expand All" at the top of the hamburger menu (Collapse All needs an account,
  Expand All needs a collapsed one). Screenshots (offscreen, demo, config pre-seeded with
  `CollapsedAccounts=bob@example.com` in $XDG_CONFIG_HOME/plasmamailrc): scratchpad
  t188-expanded-dark.png, t188-bob-collapsed-{dark,light}.png. ctest 19/19.
- verification 2026-10-10 (tester): build up to date, no warnings; ctest 19/19 (before and after
  the fix below). Offscreen demo shots, isolated XDG dirs, `PLASMAMAIL_DEMO=1`, no window:
  scratchpad `t188v-expanded-dark.png` (both open, down arrows), `t188v-bob-dark.png` and
  `t188v-bob-light.png` (bob folded: header only, right arrow; alice open; header text readable),
  `t188v-all-light.png` (both folded, right arrows). Logs (`QT_FORCE_STDERR_LOGGING=1`) show no
  QML warnings, binding loops or errors. Seeded `CollapsedAccounts` was read correctly.
  Toggle save: not checked by a test. MailController is compiled only into the plasma-mail
  executable (src/CMakeLists.txt), so no autotest can construct it without moving it into a library
  (new code, not done). Reviewed instead: header click passes the current `expanded` as the new
  collapsed flag; setAccountCollapsed / setAllAccountsCollapsed return early on no change, edit
  `m_app.collapsedAccounts`, call `AppSettings::save()` (writes and syncs "CollapsedAccounts") and
  emit collapsedAccountsChanged, which the sidebar binding follows. The save/load round trip with
  commas and backslashes is covered by appsettingstest.
  Collapse All is enabled with any account; Expand All only with a folded one (sensible; Collapse
  All on an already folded set is a no-op).
  Bug found and fixed: `removeAccount` did not drop the account from `CollapsedAccounts`, so a
  removed-then-re-added account came back folded, and Expand All stayed enabled for a ghost entry.
  Fix: `removeAccount` in src/app/mailcontroller.cpp removes the email and saves (5 lines).
  Remaining: header contrast uses `disabledTextColor` (grey); readable in both shots but lower
  contrast than folder rows. Not measured; left as is.

### T189 — Hidden folders: storage and controller
- status: done
- depends on: T188
- parallel-safe with: none
- files: src/engine/appsettings.h, src/engine/appsettings.cpp, autotests/appsettingstest.cpp,
  src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: open question 1 (proposal: display-only).
- output: `AppSettings` hidden folder paths per account; MailController `setFolderHidden(account,
  path, bool)`, `isFolderHidden`, `showHiddenFolders` property (persisted); Inbox can never be
  hidden. No change to counts, sync or notifications.
- verify: build; ctest (new tests incl. Inbox refusal).
- notes: `AppSettings::hiddenFolders` (QMap<email, QStringList paths>) and `showHiddenFolders`
  (bool, default false, key "ShowHiddenFolders" in [General]). Stored in subgroup
  [General][HiddenFolders]: key = percent-encoded email, value = path list (KConfig escapes commas
  and backslashes); stale keys are removed on save; tested with commas, slashes, backslashes, dots,
  brackets, "=". Refusal rule: `canHideFolder(folders, path)` in engine/folderutils (false for empty
  path, "INBOX" in any case, or the folder with role "inbox"); tested in folderutilstest.
  MailController API for T190: read-only property `hiddenFolders` (QVariantMap email -> list of
  paths, NOTIFY hiddenFoldersChanged; bind to this so the view updates, e.g.
  `(MailController.hiddenFolders[email] || []).indexOf(path) >= 0`), property `showHiddenFolders`
  (read/write, NOTIFY showHiddenFoldersChanged), invokables `isFolderHidden(account, path)` and
  `setFolderHidden(account, path, hidden)` (returns false and does nothing for Inbox or an unknown
  account; hidden=false always allowed). Display-only: counts, sync, IDLE, notifications and the
  All Inboxes / Unread / Flagged views are untouched. removeAccount drops the account's list. Saved
  at once. No UI yet. ctest 19/19; offscreen demo start shows only the usual offscreen messages.
- verification (2026-10-10, check of c6153d2):
  - build clean; ctest 19/19 (also after the fixes below).
  - Fixed: `setFolderHidden`/`isFolderHidden` keyed the map by the raw argument, but `sessionFor`
    matches the email case-insensitively, so "Bob@Example.com" stored a key the view (canonical
    email) never found, and removeAccount left it behind. Both now use the session's email.
  - Fixed: `canHideFolder` refuses everything while the folder list is empty (not loaded yet), since
    an inbox with a non-standard name cannot be recognised then. Test added in folderutilstest.
  - Checked by reading: `hiddenFolders` is not consulted by counts, sync, IDLE, notifications or the
    shared views (grep); removeAccount clears the list (and saves); setFolderHidden and
    setShowHiddenFolders emit only on an actual change; save() rewrites the whole subgroup, so stale
    keys go; keys decode back to the same email (percent-encoding round trip covered by the test);
    unknown account returns false without touching state.
  - Not covered by ctest: MailController itself has no harness (needs sessions), so the case fix is
    checked by reading only.
  - Offscreen demo start (`QT_QPA_PLATFORM=offscreen PLASMAMAIL_DEMO=1`, isolated XDG, timeout 20),
    once without and once with a seeded `[General][HiddenFolders]` (incl. a bad key): only the usual
    offscreen messages in both logs, and the seeded file was not rewritten. Isolated config removed.
  - Remaining, for T190/later: a folder list is only known after the account connects, so the sidebar
    can offer hiding only once folders are shown (matches the guard above). A path like "INBOX/" or
    a server with another delimiter is not refused (not the inbox path, so allowed); decide if that
    matters.

### T190 — Hidden folders: sidebar UI
- status: done
- depends on: T189
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml
- inputs: T189 API.
- output: folder row context menu "Hide from Sidebar" / "Show in Sidebar" (not offered for Inbox);
  hidden folders are left out of the tree unless "Show Hidden Folders" (header menu, checkable) is
  on, then shown dimmed (`secondaryText`).
- verify: build; ctest; screenshots in both schemes with show-hidden on and off (use a throwaway
  hide on a harmless folder and undo it afterwards; that is local UI state only, not a mail change).
- notes: FolderSidebar.qml only. FolderRow got `canHide` / `hidden`; right-click (TapHandler) opens a
  menu with "Hide from Sidebar" (view-hidden) or "Show in Sidebar" (view-visible); no menu for the
  shared views or Inbox (folderRole "inbox" or path INBOX, as canHideFolder). "Show Hidden Folders"
  (checkable) sits in the hamburger menu under Expand All, bound to MailController.showHiddenFolders.
  `hidden` reads MailController.hiddenFolders[email], so the view re-evaluates. Hidden rows have
  `visible: false` (the ColumnLayout skips them) unless show-hidden is on, then icon and text use
  disabledTextColor. Parent rule: hiding a folder hides only its own row; subfolders stay at their
  usual depth-based indent. The currently selected folder stays listed even when hidden (dimmed), so
  the selection never jumps. Offscreen demo shots with alice hiding Receipts, Projects, Projects/Beta:
  scratchpad t190-{hidden,showhidden}-{dark,light}.png; only the usual offscreen "not placed in the
  graphics scene" messages. ctest 19/19.
- verification (2026-10-10, tester, check of b919b7c): build up to date; ctest 19/19. Code review
  of the diff (the menu cannot be clicked offscreen): the row menu is offered only when
  `canHide || hidden`, so the shared views (account "", never hidden) and Inbox (role "inbox" or
  path INBOX) get no menu; `folderRole` is a real FolderModel role (foldermodel.cpp). The
  show-hidden toggle is bound to MailController.showHiddenFolders (checked: binding; the
  onToggled write keeps it in step). Hidden rows use `visible: false`, which ColumnLayout skips;
  `visible` keeps the selected folder (`highlighted`). Drag and drop is untouched (no drop
  handlers in FolderSidebar). Subfolders of a hidden parent keep their depth indent. The
  right-button TapHandler does not take the left click (onClicked). Offscreen demo with a seeded
  hidden list (Projects, Projects/Beta, Receipts for alice): shows Projects and Receipts gone
  and Projects/Alpha still listed at depth 1; with show-hidden on, the three rows are dimmed.
  Logs: no binding loop, no QML warning.
  Not exercised live: the right-click menu and the hamburger toggle (no input offscreen); the
  C++ setFolderHidden path is covered only by reading (T189).
  Design points for the planner (not changed): (1) a hidden parent's visible child sits right
  under the previous sibling, which can read as belonging to that sibling (seen in alice's
  Archive / Alpha shot); (2) with show-hidden on, a hidden row uses disabledTextColor, which is
  3.69:1 on the light sidebar (see T191); (3) with PLASMAMAIL_SIDEBAR_COLLAPSED set, toggling
  does not visibly change the sidebar for that run (the env override wins over the saved flag).

### T191 — Check: sidebar control in both schemes
- status: done (2026-10-10, tester; with the caveats in the notes)
- depends on: T190
- parallel-safe with: none
- files: none (fixes become new tasks)
- inputs: `PLASMAMAIL_COLOR_SCHEME=light|dark`, screenshot hook.
- output: collapse/expand, account collapse, hidden folders, all persisted across restart; menus
  and dimmed rows readable in both schemes.
- verify: screenshots + restart checks pass.
- notes: 2026-10-10 (tester, offscreen only: `QT_QPA_PLATFORM=offscreen`, `PLASMAMAIL_DEMO=1`,
  isolated XDG dirs under the scratchpad, timeout 30, no window). Build clean; ctest 19/19.
  Each case in BreezeDark and BreezeLight; PNGs in scratchpad `t191-<case>-<dark|light>.png`:
  - (a) default: `default`. Logo top left, drawn (146 / 142 distinct colours in the logo box).
    Code: FolderSidebar.qml `source: "org.topke.plasmamail"`, `fallback:
    ":/icons/sc-apps-org.topke.plasmamail.svg"`; the qrc alias exists (src/CMakeLists.txt:88,
    prefix /icons, data/icons/sc-apps-org.topke.plasmamail.svg). The theme icon is installed in
    ~/.local/share/icons, which the isolated XDG_DATA_HOME does not reach, so this run probably
    drew the bundled fallback (not proven). Note: the T198 text says theme name `plasma-mail`;
    the code uses `org.topke.plasmamail` (the text is out of date, the code is right).
  - (b) `seed-collapsed-env` (PLASMAMAIL_SIDEBAR_COLLAPSED=1): sidebar and logo hidden, show-sidebar
    button top left of the list band. Both schemes.
  - (c) `seed-collapsed-cfg` ($X/config/plasmamailrc, `[General] SidebarCollapsed=true`): same as
    (b). Read from the file at start; the toggle-and-restart write path was not clicked.
  - (d) `seed-bob-collapsed` (`CollapsedAccounts=bob@example.com`): bob folded with a right arrow,
    alice open. Both schemes.
  - (e) `seed-hidden-off` (`ShowHiddenFolders=false`, `[General][HiddenFolders]`
    `alice%40example.com=Projects,Projects/Beta,Receipts`): the three rows are gone; Projects/Alpha
    stays listed at depth 1. Both schemes.
  - (f) `seed-hidden-on` (same seed, `ShowHiddenFolders=true`): the three rows dimmed and readable.
    Both schemes.
  - Contrast (WCAG ratio, glyph colour against the sidebar background, measured with python3 -I
    and PIL on the PNGs; text colour = the glyph pixel farthest from the background):

    | scheme | sidebar text ("Inbox") | heading ("Mail") | dimmed text (Projects, Beta, Receipts) |
    |---|---|---|---|
    | dark  | 15.00:1 | 13.67:1 (vs header bar) | 6.64:1 |
    | light | 12.82:1 | 11.49:1 (vs header bar) | 3.69:1 |

    The light dimmed text is below 4.5:1 (the theme's disabledTextColor). Reported to the planner,
    not changed: a design choice (hidden rows should look quieter) against the readability goal.
  - Logs (`t191-*.log`): only the usual offscreen messages (Vulkan, "not placed in the graphics
    scene"); no QML warning, no binding loop.
  - Not done: menus (readability not shot, per the brief); a live restart after toggling.
  - Config reset: scratchpad xdg/config emptied after the runs. Nothing running; the user's
    installed app and their config were not touched.

# Milestone 3k — Follow the system colour scheme (user direction 2026-10-10: "the app's looks must
# follow the global (Plasma) theme, not a fixed Mimestream palette")

Supersedes the fixed hex palette of T176/T192. Keep the Mimestream-like structure (sidebar vs list
vs reading pane distinct, one accent, bold sender + dot for unread, hairline dividers, rounded inset
selection, chip style); the colour scheme supplies every colour. Runs before T188–T191.

GUI runs are paused (2026-10-10: a test window landed on the user's video desktop). Until the user
lifts that, verify with build, ctest, qmllint and `QT_QPA_PLATFORM=offscreen` runs only (no window);
screenshot checks stay `todo`.

- **Open questions**:
  1. Does the user also want an optional "Mimestream palette" choice in Settings (default: follow
     the system)? Not built unless answered.
  2. When may GUI screenshot runs resume, and on which desktop / with which KWin rule?

### T193 — Colours from the system colour sets
- status: done
- depends on: T187
- parallel-safe with: none
- files: src/qml/MimeTheme.qml, src/qml/FolderSidebar.qml, src/qml/StatusBar.qml,
  src/qml/MessageListPage.qml, src/qml/MessagePage.qml, src/qml/Main.qml
- inputs: user direction above.
- output: no hex colours in MimeTheme or the four views (label chip colours excepted). Sidebar,
  status bar and title band use `Kirigami.Theme.colorSet: Window` (band may use `Header`), list and
  reading pane `View`, with `inherit: false` only where a set changes, so runtime scheme changes
  are picked up. Selected rows = `Kirigami.Theme.highlightColor` (or text at alpha) of that set;
  hover and dividers = text colour with alpha; column hairline = text at a higher alpha; secondary
  text = `disabledTextColor`; unread dot = highlight. MimeTheme keeps only derived helpers (alphas,
  chip colours) or is removed if nothing remains. `MimeTheme.dark` is not used to choose colours.
- verify: build; ctest; `qmllint` on the changed files no new warnings; offscreen start
  (`QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 timeout 10 build/bin/plasma-mail`) without
  new QML warnings; `grep -n '"#' ` in the changed QML only finds chip colours.
- notes: Mapping: sidebar (drawer, its "Mail" ToolBar row), status bar = Window set; list page and
  reading page = View set; both title bands (header background override, T177) = Header set,
  explicit `colorSet` + `inherit: false` on the band Rectangle; `inherit: false` only on items whose
  set changes. Selected rows (folders and messages) = highlightColor at MimeTheme.selectedAlpha 0.3;
  hover textColor 0.06; reading-pane divider 0.10; column hairlines 0.2; secondary text, preview and
  sidebar section/account text = disabledTextColor; unread dot = highlightColor (so it dulls with an
  inactive window, as the system's highlight does); flag = neutralTextColor; toolbar icons default.
  MimeTheme keeps the four alphas and the label chip colours; `dark`, SystemPalette and the other
  colours removed (chip tag colours now one palette, the deeper variants). MessagePage's
  syncBodyColours passes View textColor/backgroundColor/linkColor; the old onDarkChanged re-sync
  is gone, T194 adds the runtime re-render. main.cpp comment no longer names MimeTheme.dark.
  Verified: build, ctest 19/19, qmllint warning counts lower than before (no new), offscreen start
  only the known "not placed in the graphics scene" lines, no hex outside chip colours. Not seen
  in a window (GUI runs paused).

### T194 — Message body colours from the system, re-render on scheme change
- status: done
- depends on: T193
- parallel-safe with: none
- files: src/qml/MessagePage.qml, src/app/mailcontroller.h, src/app/mailcontroller.cpp
  (src/engine/messagerenderer.* only if needed), autotests/messagerenderertest.cpp
- inputs: `MailController::setMessageColours`, `readableLinkColour` (4.5:1 floor).
- output: MessagePage passes the View set's text, background and link colour
  (`Kirigami.Theme.linkColor`) to `setMessageColours`; when any of them changes at runtime (scheme
  switch) the open message is re-rendered and reloaded; link floor stays 4.5:1.
- verify: build; ctest (a test that a dark base gets a light link and vice versa stays/added).
- notes: MessagePage has readonly bodyText/bodyBackground/bodyLink from the View set; each
  onChanged does Qt.callLater(syncBodyColours) (several changes in one tick collapse into one
  call, and setMessageColours is a no-op when nothing differs). setMessageColours already re-renders
  the stored message (no fetch, no flags, remote-image state kept) and emits currentMessageChanged,
  so html changes and showHtml() reloads the view. Test testLinkFollowsBase added (dark base ->
  lighter link, light base -> darker link, both >= 4.5:1). Extra: unread dot in MessageListPage uses
  a SystemPalette (Active group) highlight so it stays clear in an inactive window. Verified: build,
  ctest 19/19, offscreen start only the two known graphics-scene lines. Not seen in a window.

### T195 — Colour-scheme hook forces a real scheme
- status: done
- depends on: T193
- parallel-safe with: T194
- files: src/app/main.cpp, src/qml/MimeTheme.qml
- inputs: T182 (KColorSchemeManager activateScheme "Breeze Light"/"Breeze Dark").
- output: `PLASMAMAIL_COLOR_SCHEME` accepts `light`, `dark` or any installed scheme name
  (e.g. "Breeze Classic"); it only activates that KDE scheme for this process (never writes
  kdeglobals); nothing depends on MimeTheme's own dark flag.
- verify: build; ctest; offscreen start with two scheme names logs the activated scheme, no
  warnings; `~/.config/kdeglobals` unchanged (compare mtime before/after).
- notes: Done 2026-10-10. main.cpp: `light`/`dark` map to Breeze Light/Dark, any other value is
  matched case-insensitively against the KColorSchemeManager model's display names; unknown names
  warn on stderr with the available list and change nothing. `setAutosaveChanges(false)` so the
  choice is never written to config. QStyleHints colour scheme is set from the active palette's
  window lightness after activation. One qInfo line logs the activated scheme. MimeTheme.qml needed
  no change (no dark flag left, T193). Verified: build, ctest 19/19, offscreen runs for light,
  "Breeze Classic", nosuchscheme; kdeglobals and plasma-mailrc mtimes unchanged, plasma-mailrc
  ColorScheme key count unchanged (1, pre-existing).

### T196 — Check: two real schemes (screenshots)
- status: done (real-window check 2026-10-10, see notes; unread dot not visible, no unread rows)
- depends on: T194, T195, and open question 2 (GUI runs paused)
- parallel-safe with: none
- files: none (fixes become new tasks)
- inputs: Breeze Dark and Breeze Light via the hook; samples from T183.
- output: screenshots per scheme; contrast measured: text ≥ 7:1, secondary and link ≥ 4.5:1,
  selected row and dividers ≥ 1.2:1, unread dot ≥ 3:1; HTML newsletter readable in dark.
  Replaces the pending T181/T184/T185 checks (they measured the fixed palette).
- verify: as output.
- notes: waits until GUI runs are allowed again. Numeric part done 2026-10-10 (offscreen only,
  no window): colours from /usr/share/color-schemes, alphas from MimeTheme.qml, WCAG ratios.
  Results (dark / light / Breeze Classic):
  - text on View 17.7 / 15.2 / 14.9 (>=7 ok); on Window 15.4 / 13.3 / 13.4 ok.
  - secondary (ForegroundInactive) on View 7.6 / 4.2 / 3.4; on Window 6.6 / 3.7 / 3.1. Breeze's
    own value, below 4.5 on the light schemes: theme's choice, reported only.
  - link on View: raw 6.0 / 4.3 / 4.2; readableLinkColour gives 6.0 / 5.2 (10% towards black) /
    5.0 (10%). All >= 4.5 after the app's floor.
  - selected row (highlight@0.3 on View): bg vs View 1.75 / 1.30 / 1.29 (>=1.2 ok); row text
    (View text) on selected 10.1 / 11.7 / 11.5 ok. Folder selection vs Window 1.75 / 1.26 / 1.26.
  - dividers (text@0.10 on View): 1.31 / 1.21 / 1.21 (>=1.2 ok, light is marginal); column
    hairline (0.2) 1.85-1.90 / 1.45-1.49 / 1.47-1.49 ok.
  - hover (0.06, reported only): 1.17-1.19 dark, 1.12 light/classic.
  - unread dot (highlightColor) vs View: 7.3 dark ok; 2.49 light and 2.43 classic: BELOW 3:1.
    Design issue, not fixed: the dot is the raw highlight (not an alpha), so no alpha tweak can fix
    it; needs a planner decision (e.g. a darker dot tint on light schemes).
  - distinctness: sidebar (Window) vs list (View) dE76 6.5 / 5.3 / 4.3 (lum ratio 1.15 / 1.14 /
    1.11); band (Header) vs list 10.8 / 11.0 / 4.3; band vs sidebar 4.4 / 5.7 / 0.0.
    Breeze Classic has no [Colors:Header] section, so band = Window there.
  - Offscreen screenshots render (PLASMAMAIL_SCREENSHOT works offscreen):
    dark: title band (41,44,48), list (20,22,24), selection visible; light; Breeze Classic;
    newsletter dark. Logo (T198) drawn top-left in all runs, no fallback needed.
  - Observed, not fixed: in Breeze Classic the title bands stay dark (41,44,48) while sidebar
    and list are light (the scheme has no Header set, so Kirigami's Header falls back to a dark
    default). Planner decision: band fallback to Window for schemes without [Colors:Header].
    Newsletter dark: the mail's own white/grey HTML backgrounds are kept (readable, not inverted).
  - Remaining: the real-window check (screenshots by eye), T196 status stays todo.
  - Real-window check done 2026-10-10 via scripts/desktop3-run.sh, six sequential runs, desktop 2
    before and after each, no plasma-mail left running. Screenshots in the scratchpad:
    gui-dark-plain, gui-dark-newsletter, gui-dark-collapsed, gui-light-plain, gui-light-newsletter,
    gui-light-collapsed, gui-classic-plain. Pixel ratios measured from the PNGs:
    - text: title band 13.7 / 11.5 / 13.4, sidebar item 15.4 / 13.3 / 13.4, list text 17.7 / 15.2 /
      14.9 (dark / light / classic) all >= 7.
    - secondary: dark sidebar 6.6, list preview 7.6; light sidebar 3.7, list preview 4.2; classic
      3.1, 3.4. Light and classic below 4.5 are Breeze's own ForegroundInactive, report-only.
    - links in reading pane: 6.0 / 5.1 / 5.0; newsletter links 5.9 (blue), 5.9 (magenta),
      5.7 (on yellow) all >= 4.5. Newsletter body keeps its own white HTML background, readable.
    - selected row vs sidebar: 1.73 dark, 1.27 light, 1.27 classic (>= 1.2).
    - dividers: reading header rule 1.31 / 1.21 / 1.22 (>= 1.2, light marginal); list/reading
      column 1.85 dark, 1.49 light. Sidebar/list edge has no hairline (background step only,
      light 1.14 lum ratio), so it is not a divider measure: flagged, not a failure.
    - unread dot: NOT visible (status bar shows 0 unread, and clicking is not allowed). Covered
      only by the numeric T199 result (readableOn to 3:1), not by a screenshot.
    - Not verified: the logo fallback ":/icons/sc-apps-plasma-mail.svg" (the theme icon
      `plasma-mail` is installed, so the theme path was used).
    - Issue (design, planner): Breeze Classic's sidebar body and title band are both (239,240,241),
      so sidebar and band are not told apart; sidebar vs list 1.11 lum (only bg). Expected from
      T200's fix, but it misses the "sidebar vs list vs band distinct" check.
    - Issue (design, planner): light and classic secondary text (sidebar account labels, list
      preview) is 3.1-4.2, below 4.5, as Breeze ships it.

### T197 — Dev script: GUI runs on Desktop 3
- status: done
- depends on: none
- parallel-safe with: any
- files: scripts/desktop3-run.sh
- inputs: script written in the user's session 2026-10-10.
- output: `scripts/desktop3-run.sh [ENV=VALUE…]` runs build/bin/plasma-mail with a temporary KWin
  rule (Desktop 3, focus stealing prevention "extreme") and restores ~/.config/kwinrulesrc on exit.
- verify: committed as-is; not run (GUI runs paused until the user confirms it doesn't pull them to
  Desktop 3).
- notes: review points for the user: while it runs, `[General] rules/count` list only the test
  rule, so the user's other KWin rules are inactive for that time (restored on exit); the rule
  matches wmclass `plasma-mail`, so it would also apply to the user's own instance if one opens a
  window during the run.

### T198 — App logo in the sidebar header
- status: done
- depends on: T187
- parallel-safe with: none
- files: src/qml/FolderSidebar.qml (src/app/main.cpp only if the icon needs exposing to QML)
- inputs: user: "the icon on the upper left corner of the app window is still generic".
- output: a Kirigami.Icon (`iconSizes.smallMedium`, not a mask/monochrome) left of the "Mail"
  heading, source the app icon: theme name `org.topke.plasmamail` (renamed in T204) with fallback to the
  bundled `:/icons/sc-apps-org.topke.plasmamail.svg` (as main.cpp's QIcon::fromTheme fallback), so it works when run
  from build/bin. Hidden with the sidebar when collapsed (T187's "Show Sidebar" button in the list
  band stays the way back). Correct in light and dark.
- verify: build; ctest; offscreen start without new QML warnings. Visual check done 2026-10-10
  (open question 2 of Milestone 3k): the logo is drawn top-left in dark, light and Breeze Classic
  with no new QML warnings in the run logs. It came from the theme icon `plasma-mail`; the bundled
  fallback was not exercised in this run.
- notes: the title bar icon is a separate matter: `_NET_WM_ICON` and `_KDE_NET_WM_DESKTOP_FILE` are
  already correct, so a generic title-bar icon would be a KWin/panel cache issue, not an app bug.

# Milestone 7 — Undo send, command bar, conversations (user request 2026-10-10: "ok do it")

Planned only; nothing here is built yet. Order: undo send (small), command bar (medium),
conversations (largest, split so no task exceeds ~100 lines). Checks run headless as in Milestone 3j
(`QT_QPA_PLATFORM=offscreen`, `PLASMAMAIL_DEMO=1`, isolated XDG dirs); never a visible window,
never the user's real accounts.

Answered 2026-10-10 (user: "all proposals OK, start with undo send"): every proposal in the open
questions below is accepted as written — undo send 10 s default (Off/5/10/20/30, global), quit
during the delay sends at once; command bar via Kirigami Addons StatefulApp; single-key shortcuts on
by default (no two-key sequences for now); conversations off/on as one global setting, current
folder only, actions on the whole thread in the folder, flags per message. Build order: T206–T210
first, then stop.

- **Open questions**:
  1. Undo send — default delay: proposal 10 s (choices Off / 5 / 10 / 20 / 30 s), one global setting.
  2. Undo send — quitting during the delay: proposal: send at once and keep the process alive until
     SMTP finishes (with a "Sending…" tray tooltip), rather than a persistent outbox. OK?
  3. Command bar — use Kirigami Addons StatefulApp (`AbstractKirigamiApplication` + KActionCollection):
     gives the command bar, a shortcuts editor and saved custom shortcuts, but adds the KF6 XmlGui
     dependency. Proposal: yes. Alternative: a hand-rolled Ctrl+K palette (no new dependency, no
     shortcut editor).
  4. Command bar — Gmail-style single-key shortcuts (j/k, e, #, r, a, f, c, /, u) on by default, or
     behind a setting? Two-key "g then i" go-to sequences wanted?
  5. Conversations — on by default? One setting for all folders (proposal) or per folder?
  6. Conversations — scope: messages of the current folder only (proposal for the first version),
     or Gmail-style across folders (include Sent replies in the reading pane)?
  7. Conversations — archive/delete/move on a conversation row act on all its messages in the
     current folder (proposal); flag stays per message, the row shows flagged if any is. OK?

## a. Undo send

### T206 — Setting: undo-send delay
- status: done
- depends on: none
- parallel-safe with: T211, T217
- files: src/engine/appsettings.h, src/engine/appsettings.cpp, autotests/appsettingstest.cpp
- inputs: open question 1.
- output: `AppSettings::undoSendSeconds` (0 = off; default 10), key `UndoSendSeconds`; values
  outside {0,5,10,20,30} snap to the nearest choice on load.
- verify: build; ctest (defaults, round trip, snapping).
- notes: `int undoSendSeconds = 10`, key `UndoSendSeconds`. Snapping (load and save): nearest of
  {0,5,10,20,30}, negative -> 0, above 30 -> 30, ties -> the smaller (7 -> 5,
  8 -> 10, 15 -> 10, 25 -> 20); an unparsable value -> the default 10. Tests: default, round trip per
  choice, snapping, garbage.
- verification (2026-10-10): build clean, ctest 19/19 pass; added an early clamp in `snapUndoSend` so INT_MIN/INT_MAX cannot overflow, with tests.

### T207 — Controller: delayed hand-off and undo
- status: done
- depends on: T206
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: `MailController::sendMessage(QVariantMap)` (hands straight to SMTP today).
- output: with a delay > 0, `sendMessage` queues the message (id, map, deadline, QTimer) instead
  of sending; properties `pendingSend` (id, seconds left, subject) and signals `sendQueued`,
  `sendUndone(QVariantMap message)`; `undoSend(id)` cancels and emits the original map; the timer
  then calls the existing send path. Quit with a pending send follows open question 2. In demo
  mode the hand-off only logs "demo: would send" (no SMTP).
- verify: build; ctest; offscreen demo run with a dev hook `PLASMAMAIL_DEMO_SEND=1` (queues a
  fictional message at start) logs queued → sent after the delay; with `PLASMAMAIL_DEMO_UNDO=1` it
  logs undone and no send.
- notes: keep `sending` true only during the real SMTP hand-off.
  API for T208: `sendMessage(map)` queues when `undoSendSeconds > 0` (account and recipient are
  checked first: `sendFailed` at once), signal `sendQueued(int id)`; `pendingSends` (list of
  {id, subject, secondsLeft}, oldest first) and `pendingSend` (newest, empty map when none), both
  with `pendingSendChanged` (also fired every 1 s while pending); `Q_INVOKABLE bool undoSend(int id = -1)`
  (-1 = newest) emits `sendUndone(QVariantMap original)`; the original map is exactly what was
  passed to sendMessage (incl. attachments). After the delay: existing send path, `messageSent` /
  `sendFailed` as before; hand-offs run one at a time (queued behind a running SMTP send, so no
  "Another message is still being sent" for delayed ones). `sending` true only during real SMTP.
  Delay 0: as before. Quit: `aboutToQuit` handler calls `flushPendingSends()` and runs a nested
  event loop until all hand-offs finished/failed (max 60 s, logged), so every quit path
  (window close, tray Quit, QCoreApplication::quit) is covered. Demo: hand-off logs
  "demo: would send <subject>" and emits messageSent. Dev hooks (PLASMAMAIL_DEMO=1 only):
  `PLASMAMAIL_DEMO_SEND=1` (queue at 1 s), `PLASMAMAIL_DEMO_UNDO=1` (undo at 2 s),
  `PLASMAMAIL_DEMO_QUIT_AFTER=<ms>`. Logs (qInfo): "send queued id=N in Ns", "send undone id=N",
  "send handed off id=N". Verified offscreen: queued -> handed off after 10 s; undo: no send;
  UndoSendSeconds=0: immediate; quit at 3 s: flushed and handed off before exit.
- verification (2026-10-10): build clean, ctest 19/19. Headless (offscreen, isolated XDG): delay 10 s:
  "send queued id=1 in 10s", "send handed off id=1" at ~11 s, "demo: would send" at ~12.5 s; undo: "send undone id=1",
  no hand-off; UndoSendSeconds=0: immediate hand-off; quit at 3 s: "quit: sending 1 pending message(s) now",
  "send handed off", "quit: waiting", "demo: would send", "quit: all messages handed off", process exits by itself
  after ~5 s. Demo hand-off is now asynchronous (1.5 s), so the quit wait is exercised. Review fixes: quit wait
  pumps events by hand (a nested QEventLoop returns at once after a second quit request), re-entered aboutToQuit
  ignored, timers stopped before deleteLater.
  Open, not fixed (planner): (a) a second quit request during the wait segfaults in QGuiApplicationPrivate::quit
  / QtWebEngine (reproduced with PLASMAMAIL_DEMO_SEND=1 + a second quit 300 ms after the first; gdb backtrace
  in scratchpad); fixing it needs the controller to own quitting (requestQuit before QCoreApplication::quit,
  routing tray/window quit through it). (b) ComposeWindow: two composers waiting: the first messageSent closes
  both; sendFailed reaches every waiting composer; a composer closed during the delay gets no failure report.
  For T208. (c) Not verifiable headless: real SMTP hand-off at quit; close-to-tray with keepInTray (the demo
  disables the tray); a real sendFailed.

### T227 — Fix: the controller owns quitting
- status: done
- depends on: T207
- parallel-safe with: none
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp, src/app/trayicon.cpp, src/app/main.cpp,
  src/qml/Main.qml (quit action / window close)
- inputs: T207 verification: a second quit request while the quit wait runs crashes (SIGSEGV in
  QGuiApplicationPrivate::quit → WebEngine); the wait currently runs inside aboutToQuit with
  processEvents polling.
- output: `MailController::requestQuit()`: first call flushes pending sends, waits (normal event
  loop, no nested loop / processEvents in aboutToQuit) until every hand-off finished or failed
  (cap 60 s), then calls QCoreApplication::quit(); later calls while waiting are ignored. Tray
  Quit, Ctrl+Q / the Quit action, and the last-window close when it should quit route through it
  (close-to-tray unchanged). aboutToQuit keeps only a last-resort synchronous flush log.
- verify: build; ctest; demo runs: PLASMAMAIL_DEMO_SEND=1 + PLASMAMAIL_DEMO_QUIT_AFTER=3000 → flush,
  wait, exit 0; same with a second quit request 300 ms later (dev hook) → exit 0, no crash.
- notes: `MailController::requestQuit()` (Q_INVOKABLE): flushes pending sends,
  waits in the normal event loop via messageSent/sendFailed (60 s QTimer cap), then quits (the
  final quit() is deferred with singleShot(0): quitting from inside the signal emission hung the
  shutdown); repeat calls are ignored and logged. Routed: KStatusNotifierItem::quitRequested
  (abortQuit() + requestQuit), Ctrl+Q Shortcut (StandardKey.Quit) in Main.qml, Main window
  onClosing when keepInTray is off; close-to-tray unchanged. aboutToQuit only warns if something is
  still pending. Dev hooks: PLASMAMAIL_DEMO_QUIT_AFTER calls requestQuit();
  PLASMAMAIL_DEMO_QUIT_TWICE=1 calls it again 300 ms later. Verified headless: send+quit 3 s exits
  rc 0 in ~5 s (also with QUIT_TWICE, second ignored), no-pending quit rc 0. Unverified headless:
  tray menu Quit and a real window close.
  Window-close rule (fix, 2026-10-10): Qt's quitOnLastWindowClosed is off (set by MailController, so
  Qt never quits by itself). Closing any window only hides or destroys it. When the LAST window
  closes (QGuiApplication::lastWindowClosed) and keepInTray is off, MailController calls
  requestQuit(), which waits for pending sends. With keepInTray on, nothing quits (tray). Main.qml
  onClosing and TrayIcon::setKeepInTray were removed (they fought Qt's quit). Tray Quit and Ctrl+Q
  still call requestQuit() directly.
- verification (2026-10-10, offscreen, isolated XDG, timeout-bounded): build clean; ctest 19/19.
  Builder: SEND+QUIT_AFTER=3000 rc 0 (queued, "quit: sending 1 pending message(s) now", handed off,
  "quit: all messages handed off"); +QUIT_TWICE rc 0 (second request "ignored"); no pending
  QUIT_AFTER=2000 rc 0 (no log line: quits at once); SEND only rc 124 (timeout kill; the app has no
  quit hook, handed off at 10 s). Window close: main closed with a send pending rc 0 (flushed,
  handed off, exit); main closed, no composer, nothing pending rc 0; main closed with a composer open
  keeps running (rc 124 at the timeout, no quit lines: the regression is fixed); composer closed
  later with a send pending rc 0 (flushed, handed off, exit); keepInTray=true, main closed: keeps
  running (rc 124). Not verified headless: the 60 s cap (no hand-off that hangs in demo mode), tray
  menu Quit, Ctrl+Q keypress, a real window-manager close, real SMTP. Hook-only QML message: a
  composer loaded for the PLASMAMAIL_DEMO_COMPOSE hook is a root object, so its destroy() logs
  "Invalid attempt to destroy() an indestructible object"; real composers use createObject.

### T208 — UI: "Sending… Undo" and reopening the message
- status: done
- depends on: T207, T227
- parallel-safe with: none
- files: src/qml/StatusBar.qml, src/qml/Main.qml, src/qml/ComposeWindow.qml
- inputs: T207 API; ComposeWindow's existing prefill (used by reply/forward).
- output: the compose window closes when the message is queued; the status bar shows "Sending in
  N s — Undo" (countdown, Undo button) and a passive notification with an Undo action; Undo reopens
  ComposeWindow filled with the same from/to/cc/bcc/subject/body/attachments; Ctrl+Z in the main
  window also undoes while pending.
- verify: build; ctest; offscreen demo shots (`PLASMAMAIL_DEMO_SEND=1`) of the status bar in
  BreezeDark and BreezeLight; undo path checked with `PLASMAMAIL_DEMO_UNDO=1` (compose window
  shot).
- notes: from T207 verification: once the compose window closes on queue, a later SMTP failure must
  reopen a compose window with that message's content and the error (today sendFailed reaches every
  waiting composer and a closed composer loses its content); messageSent must only close the
  composer it belongs to (use the queue id).
- verification (2026-10-10): build clean, ctest 19/19; offscreen demo shots in the scratchpad
  (t208-pending-<dark|light>, t208-undo-<dark|light>, t208-fail-dark), logs free of QML warnings.
  Implementation: `sendMessage` returns the queue id; signals are now `messageSent(int id)` and
  `sendFailed(int id, QString message, QVariantMap original, bool queued)` (queued = came through the
  queue, its window is gone); `isPendingSend(id)` lets ComposeWindow close itself right after a queue.
  ComposeWindow matches answers by its `sendId` (0 while still inside sendMessage), gained
  initialBcc / initialAttachments / initialError / restored (closing a restored message asks first).
  Main.qml `reopenMessage(map, error)` is used for sendUndone and for a queued failure; StatusBar
  "Sending in N s (+k more)" + Undo; passive notification with Undo; Ctrl+Z Shortcut enabled only while
  pending (a focused text field handles its own Ctrl+Z first). Dev hooks: PLASMAMAIL_DEMO_FAIL=1 (demo
  hand-off fails), PLASMAMAIL_SCREENSHOT_NEWEST=1 (grab the newest visible window).
  Not verifiable headless: real clicks on Undo / Ctrl+Z keypress, the compose window closing on a real
  Send press, a real SMTP failure.
- verification 2 (2026-10-10, offscreen, isolated XDG, timeout-bounded; hooks in commit "T208: UI:
  verification hooks and fixes"): build clean, ctest 19/19. Screenshots (scratchpad): t208-pending-dark/light,
  t208-undo-dark/light, t208-fail-dark viewed: status bar and composer readable in both schemes; no hex
  colours in StatusBar.qml. Also viewed: t208-undo-attach-dark.png (reopened with attachment, fields and
  body identical), t208-restored-close-dialog-light.png (Discard Message? on a restored composer).
  Checks (logs from the demo hooks):
  - queue closes the composer: 2 -> 1 visible windows, message window objects 1 -> 0 (no leak);
    delay 0: composer stays open (waiting=1) until the demo hand-off, then closes (objects 0).
  - Ctrl+Z through the platform key path: undoes ("send undone id=1"), the composer reopens with
    to/subject/body and the attachment (attachments=1 initial=1 restored=1); with focus in the search
    field (PLASMAMAIL_DEMO_KEY_UNDO_SEARCH=1) the field gets the key, the message stays pending and is handed off.
  - delay 0 + PLASMAMAIL_DEMO_FAIL=1: composer stays open with "Demo: the server refused the message."
    (waiting=0).
  - two queued composers (delay 5 s, fail): each closes on queue; each failure reopens its own message
    with its own subject, attachment and the error (message 1 then message 2).
  - two composers, delay 0: the second gets "Another message is still being sent." and stays open;
    the first closes at its own messageSent; the second is unaffected.
  - restored composer closed by the window manager: refused (discard prompt shown, object kept).
  Fixes: none to product code. Dev hooks added (above); the Ctrl+Z hook needs the Qt private
  module (Qt6::GuiPrivate), since QCoreApplication::sendEvent does not reach the shortcut map.
  Remaining (planner): (a) a restored composer whose attachment file was removed silently drops it
  (MailController::fileInfo returns {} and addAttachments skips it); the user is not told.
  (b) cosmetic: in the dark scheme the attach and Send icons of ComposeWindow are dark on dark in
  the offscreen theme (same for a plain reply window, so not T208-specific). (c) Not verifiable
  headless: a real click on Undo, a real Ctrl+Z through the window manager, the Send button press
  closing the composer, real SMTP failure.

### T209 — Settings: undo-send choice
- status: done
- depends on: T206
- parallel-safe with: T208
- files: src/qml/Main.qml (settings modules), new src/qml/SendingSettingsPage.qml (+ CMake/qrc)
- inputs: KirigamiSettings modules as used for notifications/storage.
- output: a "Sending" settings page with "Undo send: Off / 5 / 10 / 20 / 30 seconds".
- verify: build; ctest; offscreen shot via `PLASMAMAIL_SCREENSHOT_SETTINGS=sending`.
- notes: new `SendingSettingsPage.qml` (FormCard, `FormComboBoxDelegate` Off/5/10/20/30 seconds, explanation as description), module id `sending` (icon mail-send) between Notifications and Storage in Main.qml; `MailController.undoSendSeconds` read/write property (saves AppSettings, snaps via its save/load, emits appSettingsChanged; pending sends keep their deadline). Build clean, ctest 19/19; offscreen shots dark/light, seeded UndoSendSeconds=20 shows "20 seconds".

### T228 — Dev key hook without Qt private API
- status: todo
- depends on: T208
- parallel-safe with: T209
- files: CMakeLists.txt, src/CMakeLists.txt, src/app/main.cpp
- inputs: T208 verification added `QT_FIND_PRIVATE_MODULES` + `Qt6::GuiPrivate` to the app only so a
  demo hook can inject Ctrl+Z through QWindowSystemInterface; a private Qt module in the shipped
  binary breaks on Qt updates.
- output: the hook uses public API (e.g. `QTest::keyClick` on the QWindow from Qt6::Test, which is
  already found, or a synthesized QKeyEvent delivered via QGuiApplication::sendEvent to the focus
  object/window after activation) or the hook moves behind a dev-only CMake option that is off by
  default; `Qt6::GuiPrivate` and `QT_FIND_PRIVATE_MODULES` removed from the default build.
- verify: build; ctest; the T208 Ctrl+Z demo check still passes (undo fires; focused search field
  keeps its own Ctrl+Z).
- notes:

### T229 — Undo: tell the user when a restored attachment is gone
- status: todo
- depends on: T208
- parallel-safe with: T209, T228
- files: src/qml/ComposeWindow.qml
- inputs: T208 verification: a restored composer silently drops an attachment whose file was
  deleted meanwhile.
- output: the restored composer shows an inline warning naming the missing files.
- verify: build; ctest; demo run with a deleted attachment file shows the warning (offscreen shot).
- notes: not urgent.

### T210 — Check: undo send
- status: todo
- depends on: T208, T209, T228
- parallel-safe with: none
- files: none (fixes become new tasks)
- inputs: demo hooks.
- output: queue → countdown → send; undo → compose reopened with identical content (attachments
  too); delay Off sends at once; quit during delay follows the decision.
- verify: offscreen demo runs and logs, both schemes.
- notes:

## b. Command bar and keyboard shortcuts

Fit with KDE: actions become named entries in one action collection (Kirigami Addons StatefulApp,
open question 3): each has a stable id, text, icon and default shortcut; QML uses them as
`Kirigami.Action { fromQAction: … }`, so toolbar buttons, context menus, the command bar and the
shortcuts editor share one definition and user-changed shortcuts are saved by KConfig. Today the
app has about 20 scattered `Shortcut {}`/`Kirigami.Action` definitions in Main.qml and
MessagePage.qml.

### T211 — Action collection
- status: todo
- depends on: none (open question 3 answered: StatefulApp)
- parallel-safe with: T206, T217
- files: CMakeLists.txt, src/CMakeLists.txt, new src/app/mailapplication.h/.cpp, src/app/main.cpp
- inputs: Kirigami Addons 1.15 statefulapp (installed).
- output: `MailApplication : AbstractKirigamiApplication` registers actions with ids: compose,
  reply, reply_all, forward, archive, delete, move, mark_read, mark_unread, flag, search,
  refresh, toggle_sidebar, settings, shortcuts_help, next_message, previous_message,
  go_to_folder; each forwards to MailController (enabled state bound to it).
- verify: build; ctest; offscreen demo start without warnings.
- notes:

### T212 — Views use the actions
- status: todo
- depends on: T211
- parallel-safe with: none
- files: src/qml/Main.qml, src/qml/MessagePage.qml, src/qml/MessageListPage.qml,
  src/qml/FolderSidebar.qml
- inputs: T211 ids.
- output: existing toolbar actions and `Shortcut {}` blocks replaced by `fromQAction` of the
  collection; behaviour and current shortcuts unchanged (Ctrl+B, Ctrl+,, R, Shift+R, …).
- verify: build; ctest; offscreen shots unchanged; grep: no leftover `Shortcut {` duplicates.
- notes:

### T213 — Keyboard set
- status: todo
- depends on: T212
- parallel-safe with: none
- files: src/app/mailapplication.cpp, src/qml/MessageListPage.qml
- inputs: proposal: j/k and Down/Up next/previous message, e archive, # delete, r reply, a reply
  all, f forward, c compose, / search, u mark unread, s flag, Ctrl+K command bar, ? shortcuts help.
- output: default shortcuts set; single-key shortcuts don't fire while a text field (search,
  compose) has focus; list navigation moves the selection without marking read until opened.
- verify: build; ctest; review the focus guard; offscreen start without "ambiguous shortcut" logs.
- notes:

### T214 — Command bar (Ctrl+K)
- status: todo
- depends on: T212
- parallel-safe with: T215
- files: src/qml/Main.qml, src/app/mailapplication.cpp
- inputs: StatefulApp's QuickCommandBar (fuzzy search over the collection).
- output: Ctrl+K opens the command bar; besides the actions it offers "Go to folder: <name>" and
  "Move to folder: <name>" entries generated from the current folder list (refreshed when folders
  change).
- verify: build; ctest; offscreen demo shot with the bar open (dev hook
  `PLASMAMAIL_SCREENSHOT_COMMANDBAR=1`) in both schemes.
- notes: if the bar can't take dynamic entries, register per-folder actions in a separate
  collection.

### T215 — Shortcuts help and editor
- status: todo
- depends on: T213
- parallel-safe with: T214
- files: src/qml/Main.qml, new src/qml/ShortcutsHelp.qml
- inputs: the collection.
- output: "?" opens a dialog listing actions grouped (Messages, Navigation, App) with their current
  shortcuts; a "Shortcuts" settings page uses StatefulApp's shortcut editor.
- verify: build; ctest; offscreen shots of the dialog and the settings page in both schemes.
- notes:

### T216 — Check: command bar and shortcuts
- status: todo
- depends on: T214, T215
- parallel-safe with: none
- files: none
- output: every action reachable from the bar; shortcuts match the help dialog; no shortcut fires in
  text fields; a changed shortcut survives a restart (isolated config).
- verify: offscreen demo runs + review.
- notes:

## c. Conversations (largest item)

Approach: thread ids are computed in the engine and stored, the list model groups by them.
- Gmail (server has `X-GM-EXT-1`): `X-GM-THRID` from the fetch is the thread id.
- Otherwise (Yahoo): a JWZ-style pass: References / In-Reply-To link messages by Message-ID
  (union-find); messages with no links fall back to the normalised subject (strip Re:/Fwd:/AW:/
  SV: prefixes, case-folded) within 30 days.
- List row: newest message's date, participants ("Alice, Bob, me"), count badge "3", latest
  subject and preview; unread dot if any message is unread, flag if any is flagged.
- Reading pane: the messages stacked oldest → newest; read ones collapsed to one line (sender,
  date, snippet), unread and the newest expanded; Reply/Reply all/Forward per message.
- Flags/actions: see open question 7; mark read on open marks all expanded messages read.

### T217 — Threading headers in MessageSummary
- status: todo
- depends on: none
- parallel-safe with: T206, T211
- files: src/engine/imapclient.h, src/engine/imapclient.cpp, autotests/ (parser test)
- output: `MessageSummary` gains `messageId`, `inReplyTo`, `references` (QStringList), `gmThreadId`
  (qint64, 0 = none); a helper fills them from a KMime header block; test with sample headers.
- verify: build; ctest.
- notes:

### T218 — Fetch the headers (and X-GM-THRID)
- status: todo
- depends on: T217
- parallel-safe with: none
- files: src/engine/imapclient.cpp, src/engine/syncmanager.cpp
- output: list fetches request Message-ID, In-Reply-To, References (header fields, peek) and, when
  the server lists X-GM-EXT-1, X-GM-THRID; parsed into the summary. No server-side changes.
- verify: build; ctest; review the fetch scope; one manual check against the user's Gmail is a
  separate manual task (T226).
- notes: KIMAP's FetchJob may not expose X-GM-THRID; if not, a raw fetch for that item only.

### T219 — Store: thread columns
- status: todo
- depends on: T217
- parallel-safe with: T218
- files: src/engine/messagestore.h, src/engine/messagestore.cpp, autotests/messagestoretest.cpp
- output: schema migration adds message_id, in_reply_to, refs, gm_thrid, thread_id (existing
  databases upgrade in place); `addMessage`/`messages` persist and load them.
- verify: build; ctest (fresh DB and upgrade from the old schema).
- notes: never touch the user's DB in tests; temp dirs only.

### T220 — ThreadBuilder
- status: todo
- depends on: T217
- parallel-safe with: T218, T219
- files: new src/engine/threadbuilder.h/.cpp, autotests/threadbuildertest.cpp
- output: pure function: list of summaries → thread id per message (Gmail id first, then
  References/In-Reply-To union-find, then subject fallback within 30 days); stable ids.
- verify: build; ctest with fixtures (broken References, missing parents, same subject months
  apart, Re:/Fwd: variants).
- notes:

### T221 — List model: conversation rows
- status: todo
- depends on: T219, T220
- parallel-safe with: none
- files: src/models/messagelistmodel.h/.cpp, src/engine/appsettings.* (ConversationView), tests
- output: when `conversationView` is on, one row per thread with roles threadCount, participants,
  threadUnread, threadFlagged, threadUids; off = today's rows.
- verify: build; ctest.
- notes:

### T222 — List row UI for conversations
- status: todo
- depends on: T221
- parallel-safe with: T223
- files: src/qml/MessageListPage.qml
- output: participants instead of a single sender, count badge, unread dot / flag from the thread
  roles; settings toggle "Group messages into conversations".
- verify: build; offscreen demo shots (demo data gains two threads) in both schemes.
- notes:

### T223 — Controller: open a conversation
- status: todo
- depends on: T221
- parallel-safe with: T222
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- output: `openConversation(account, threadId)` builds an ordered list (uid, from, date, snippet,
  unread, html on demand) exposed as a model; bodies rendered with the existing renderer, fetched
  lazily; marks read per open question 7.
- verify: build; ctest; offscreen demo run logs.
- notes:

### T224 — Reading pane: stacked messages
- status: todo
- depends on: T223
- parallel-safe with: none
- files: src/qml/MessagePage.qml (+ new src/qml/ConversationView.qml)
- output: stacked cards as in the approach; one WebEngine view per expanded message (or one view
  with all expanded bodies — pick by memory cost); Reply/Reply all/Forward per card.
- verify: build; offscreen demo shots in both schemes.
- notes: watch WebEngine memory with many expanded messages.

### T225 — Conversation actions
- status: todo
- depends on: T223
- parallel-safe with: T224
- files: src/app/mailcontroller.cpp
- output: archive/delete/move/mark read on a conversation act on all its messages in the current
  folder; flag per message; undo of these follows today's behaviour.
- verify: build; ctest; demo run logs (no real server).
- notes:

### T226 — Check: conversations
- status: todo
- depends on: T222, T224, T225, T218
- parallel-safe with: none
- files: none
- output: offscreen demo checks in both schemes; then a manual check with the user on the real
  Gmail and Yahoo accounts (read-only browsing, no actions) that threads group sensibly.
- verify: as output.
- notes:

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
