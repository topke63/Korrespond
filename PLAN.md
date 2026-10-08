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
  - **Q1** (answered 2026-10-08: yes, ready; the user types it in themselves): Does the user have a Yahoo app password ready (Yahoo → Account security →
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
- Tests live in `autotests/`, use QTest, are added with ECM's `ecm_add_test`, and must call
  `QStandardPaths::setTestModeEnabled(true)` before touching config.
- Installed library facts (verified on this machine, Qt 6.11, KF 6.30, KPim 26.08):
  - KMime is `KF6::Mime`, headers `<KMime/Message>`; messages are `std::shared_ptr<KMime::Message>`;
    header getters take `KMime::CreatePolicy` (`DontCreate`/`Create`); const getters return
    `nullptr` when absent. Useful: `mainBodyPart("text/html")`, `decodedText()`,
    `Content::attachments()`, `contentDisposition()->filename()`, `contentType()->name()`.
  - KIMAP is `KPim6::IMAP`, headers `<KIMAP/LoginJob>` etc. Jobs are KJobs queued per
    `KIMAP::Session`. `ListJob::IncludeFolderRoleFlags` returns RFC 6154 flags (`\Sent`…).
    `FetchJob::FetchScope::Headers` fetches flags + UID + common headers;
    `FetchScope::Full` fetches the whole message.
  - QtKeychain: `find_package(Qt6Keychain)`, target `Qt6Keychain::Qt6Keychain`,
    header `<qt6keychain/keychain.h>`.
  - QML i18n: `KLocalization::setupLocalizedContext(&engine)` from `<KLocalizedQmlContext>`,
    then `KI18n.i18nc("@context", "text")` in QML. Needs `TRANSLATION_DOMAIN` defined (done in
    root CMake).
  - QtWebEngine in QML needs `QtWebEngineQuick::initialize()` before the `QGuiApplication`
    is constructed.
- Commit after each finished task: `T<n>: <title>` (git repo is initialized, nothing
  committed yet).
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
- status: todo
- depends on: T1
- parallel-safe with: T3, T7, T14
- files: autotests/CMakeLists.txt, autotests/accounttest.cpp
- inputs: src/engine/account.h (presets: yahoo/gmail/outlook; `isValid`; `load/save/remove`
  using `plasmamailrc` group `Account`).
- output: `accounttest` checks: each preset fills correct hosts/ports, unknown provider returns
  false, `isValid` rules, save→load round trip, `remove()` clears it. Uses test mode paths.
- verify: `cmake --build build && ctest --test-dir build -R accounttest --output-on-failure`
- notes:

### T3 — Folder helpers: `FolderInfo`, role detection, sorting
- status: todo
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
- notes:

### T4 — Unit test for folder helpers
- status: todo
- depends on: T2, T3
- parallel-safe with: T5, T8, T10, T15
- files: autotests/folderutilstest.cpp
- inputs: src/engine/folderutils.h
- output: data-driven tests for role detection (incl. `inbox` lowercase, `\Sent` vs `\SENT`,
  no flags) and sort order.
- verify: `ctest --test-dir build -R folderutilstest --output-on-failure`
- notes:

### T5 — `ImapClient` compiles against folder helpers
- status: todo
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
- notes:

### T6 — `imap-probe` command-line tool for manual engine checks
- status: todo
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
- notes:

### T7 — Message body to safe HTML
- status: todo
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
- notes:

### T8 — Unit test for `messageBodyHtml`
- status: todo
- depends on: T2, T7
- parallel-safe with: T4, T5, T10, T15
- files: autotests/messagerenderertest.cpp
- inputs: src/engine/messagerenderer.h; build test messages from raw RFC 822 strings via
  `KMime::Message::setContent()` + `parse()`.
- output: tests: plain-text only (escapes `<script>`, links a URL), multipart/alternative
  picks HTML, CSP meta always present, quoted-printable + non-UTF-8 charset decoded.
- verify: `ctest --test-dir build -R messagerenderertest --output-on-failure`
- notes:

### T9 — Attachment names from a message
- status: todo
- depends on: T8
- parallel-safe with: T10–T15
- files: src/engine/messagerenderer.{h,cpp}, autotests/messagerenderertest.cpp
- inputs: `Content::attachments()`; name from `contentDisposition()->filename()`, falling back
  to `contentType()->name()`, then a translated "Unnamed attachment".
- output: `QStringList attachmentNames(KMime::Message &msg)` plus a test with one named and one
  unnamed attachment.
- verify: `ctest --test-dir build -R messagerenderertest --output-on-failure`
- notes:

### T10 — `FolderModel` in the models library
- status: todo
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
- notes:

### T11 — Unit test for `FolderModel`
- status: todo
- depends on: T2, T10
- parallel-safe with: T12–T15
- files: autotests/foldermodeltest.cpp
- inputs: src/models/foldermodel.h
- output: test runs `QAbstractItemModelTester` while calling `setFolders`, checks role values,
  `inboxPath()` (inbox role, fallback to first, empty model → empty string), `displayName()`.
- verify: `ctest --test-dir build -R foldermodeltest --output-on-failure`
- notes:

### T12 — `MessageListModel` in the models library
- status: todo
- depends on: T5, T10
- parallel-safe with: T11, T14, T15
- files: src/models/messagelistmodel.h, src/models/messagelistmodel.cpp
- inputs: draft at src/app/messagelistmodel.{h,cpp} — move, drop QML macros. Uses
  `MessageSummary` from imapclient.h.
- output: roles `uid, subject, from, date, dateText, unread`; `setMessages`, `clear`,
  `setUnread(uid, bool)` (emits `dataChanged` for that row only), `message(uid)`.
  Remove the leftover draft files in src/app/.
- verify: `cmake --build build --target plasmamail_models` succeeds.
- notes:

### T13 — Unit test for `MessageListModel`
- status: todo
- depends on: T2, T12
- parallel-safe with: T14–T16
- files: autotests/messagelistmodeltest.cpp
- inputs: src/models/messagelistmodel.h
- output: `QAbstractItemModelTester`; empty subject → "(No subject)"; `setUnread` emits one
  `dataChanged` with `UnreadRole`; `message(unknownUid)` returns uid 0.
- verify: `ctest --test-dir build -R messagelistmodeltest --output-on-failure`
- notes:

### T14 — `SecretStore`: async keychain wrapper
- status: todo
- depends on: T1
- parallel-safe with: T2, T3, T7
- files: src/engine/secretstore.h, src/engine/secretstore.cpp
- inputs: QtKeychain facts above; service name `plasma-mail` (`AccountSecrets::service` in
  account.h), key = account email.
- output: `SecretStore` QObject with `writePassword(key, password)`, `readPassword(key)`,
  `deletePassword(key)` and signals `passwordWritten(key, error)`, `passwordRead(key, password, error)`,
  `passwordDeleted(key, error)` (empty error = success). Link Qt6Keychain into engine lib.
- verify: `cmake --build build --target plasmamail_engine` succeeds.
- notes:

### T15 — Test for `SecretStore` against the session keychain
- status: todo
- depends on: T2, T14
- parallel-safe with: T4, T5, T8, T10–T13
- files: autotests/secretstoretest.cpp
- inputs: src/engine/secretstore.h
- output: write → read → delete → read-fails round trip with key `plasma-mail-autotest`,
  using `QSignalSpy::wait`. `QSKIP` if the keychain reports "no backend available".
- verify: `ctest --test-dir build -R secretstoretest --output-on-failure` (run inside the
  Plasma session so KWallet/Secret Service is reachable).
- notes:

### T16 — `MailController`: account setup and folder listing
- status: todo
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
- notes:

### T17 — `MailController`: folder selection and message list
- status: todo
- depends on: T12, T16
- parallel-safe with: T13, T15
- files: src/app/mailcontroller.h, src/app/mailcontroller.cpp
- inputs: `MessageListModel`, `ImapClient::fetchHeaders`.
- output: properties `messageModel` (`QAbstractListModel*`), `currentFolder`,
  `currentFolderName`; invokables `selectFolder(path)` (clears list, fetches newest 50) and
  `refresh()`. After the first folder list arrives, auto-select `inboxPath()`. Ignore
  `headersFetched` for a folder that is no longer current.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes:

### T18 — `MailController`: open a message
- status: todo
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
- notes:

### T19 — App entry point and main window
- status: todo
- depends on: T16
- parallel-safe with: T17, T18
- files: src/app/main.cpp, src/qml/Main.qml
- inputs: conventions above (WebEngine init, localized context, `KAboutData` with
  component name `plasma-mail`, `QQuickStyle` fallback to `org.kde.desktop`).
- output: `Kirigami.ApplicationWindow` titled "Plasma Mail"; shows an empty placeholder page
  for now; `MailController.errorOccurred` shows `showPassiveNotification`.
- verify: `cmake --build build && QT_QPA_PLATFORM=offscreen timeout 5 build/bin/plasma-mail; test $status -eq 124`
  (still running after 5 s = no startup crash) and no `qrc:` / `QML` warnings in the output.
- notes:

### T20 — Account setup page
- status: todo
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
- notes:

### T21 — Folder list in the global drawer
- status: todo
- depends on: T17, T19
- parallel-safe with: T20, T22
- files: src/qml/Main.qml
- inputs: `MailController.folderModel` roles, `selectFolder`, `currentFolder`, `removeAccount`.
- output: `Kirigami.GlobalDrawer` (modal on narrow windows) listing folders with icon,
  indentation by `depth`, highlight of the current folder; footer action "Remove account".
- verify: offscreen launch as in T19 prints no QML warnings.
- notes:

### T22 — Message list page
- status: todo
- depends on: T17, T19
- parallel-safe with: T20, T21
- files: src/qml/MessageListPage.qml, src/qml/Main.qml
- inputs: `MailController.messageModel` roles, `currentFolderName`, `busy`, `refresh`,
  `openMessage`.
- output: `Kirigami.ScrollablePage` titled with the folder name; delegates show sender, subject,
  date, bold when unread; refresh action (F5); busy indicator; placeholder message when empty.
  Clicking a row calls `openMessage(uid)` and pushes the message page (T23; until then, no push).
- verify: offscreen launch as in T19 prints no QML warnings.
- notes:

### T23 — Message page with sandboxed HTML view
- status: todo
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
- notes:

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
- notes:

### T26 — `StorageSettings`: values, load/save, cutoff and byte limit
- status: todo
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
- notes:

### T27 — Unit test for `StorageSettings`
- status: todo
- depends on: T2, T26
- parallel-safe with: T4, T5, T8, T28
- files: autotests/storagesettingstest.cpp
- inputs: src/engine/storagesettings.h
- output: tests: MB/GB/Unlimited byte math; cutoff for each keep unit (incl. month-end, e.g.
  Mar 31 − 1 month); cutoff takes the later of start date and keep-for; Forever + null start
  → null cutoff; save→load round trip; defaults.
- verify: `ctest --test-dir build -R storagesettingstest --output-on-failure`
- notes:

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

### T39 — `MailController`: storage settings
- status: todo
- depends on: T18, T37
- parallel-safe with: T38
- files: src/app/mailcontroller.{h,cpp}
- inputs: `StorageSettings`, `MessageStore`, `SyncManager`, `applyRetention`.
- output: properties `storageEnabled`, `importSince`, `keepValue`, `keepUnit`, `sizeValue`,
  `sizeUnit`, `storageUsedBytes`, `syncing`; invokables `saveStorageSettings(...)` (saves,
  applies retention, starts sync) and `cleanUpNow()`. Sync runs after connecting when enabled.
- verify: `cmake --build build --target plasma-mail` succeeds.
- notes:

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
- status: todo
- depends on: T21, T39
- parallel-safe with: T38, T40
- files: src/qml/StorageSettingsPage.qml, src/qml/Main.qml
- inputs: T39 properties.
- output: `Kirigami.ScrollablePage` + `FormLayout`: switch "Store mail on this computer";
  "Import mail from" with a date field and an "All mail" option; "Keep mail for" spin box +
  combo (days/weeks/months/years/forever); "Size limit" spin box + combo (MB/GB/unlimited);
  "Currently used: X" (formatted with `KFormat`/`Format.formatByteSize`); note that only
  local copies are removed; "Clean up now" and "Apply" buttons. Drawer action opens it.
- verify: offscreen launch as in T19 prints no QML warnings; manual: changing units keeps
  the number and the page reloads saved values after restart.
- notes:

### T42 — Manual end-to-end check of local storage
- status: todo
- depends on: T24, T40, T41, Q1
- parallel-safe with: none
- files: none (record results; file new tasks for bugs)
- inputs: the user's Yahoo account.
- output: confirmed behaviour list in notes.
- verify: (manual, with the user) enable storage with a start date 1 month back and 50 MB →
  only mail since that date is stored and used size ≤ 50 MB → lower to 10 MB → oldest local
  copies removed, still visible on the server/webmail → offline (network off) the stored Inbox
  still lists and opens messages.
- notes:

### T25 — Global verify
- status: todo
- depends on: T1–T24, T26–T42
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
