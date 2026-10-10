<!--
SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
SPDX-License-Identifier: CC0-1.0
-->

# Plasma Mail

A simple IMAP mail client for KDE Plasma, written in C++20 with Qt 6 and Kirigami.
It is a personal project: it does not use Akonadi, and it talks IMAP/SMTP directly.

- **Accounts:** Yahoo (app password) and Gmail (Google sign-in, OAuth2). Several accounts at once.
  Outlook is not available yet.
- **Mail:** folders, conversations, local storage with import date / keep time / size limits,
  IMAP IDLE (push) and new-mail notifications, compose / reply / forward over SMTP.
- **Contacts:** recipient autocomplete and sender names (Google People API for Gmail, addresses
  seen in cached mail otherwise).
- **Safe viewing:** messages are shown without remote content and without JavaScript.

Passwords and tokens are stored in the system keychain (KWallet or Secret Service), never in
the config file.

## Requirements

- A Linux system running Qt 6.6+ and KDE Frameworks 6 (developed on CachyOS with Qt 6.11,
  KF 6.30, KDE Gear 26.08).
- A running KWallet or another Secret Service provider (for QtKeychain).
- A C++20 compiler, CMake 3.22+ and Ninja.

### Packages

**Arch / CachyOS / Manjaro**

```sh
sudo pacman -S --needed base-devel cmake ninja extra-cmake-modules \
    qt6-base qt6-declarative qt6-webengine qt6-networkauth qt6-tools \
    kirigami kirigami-addons qqc2-desktop-style ki18n kcoreaddons kconfig kcolorscheme \
    kmime knotifications knotifyconfig kstatusnotifieritem \
    kimap ksmtp qtkeychain-qt6 cyrus-sasl libkgapi
```

`libkgapi` is needed at runtime only: it ships the `libkdexoauth2` SASL plugin that Gmail's
XOAUTH2 login uses. Without it, Gmail sign-in succeeds but the IMAP login fails.

**Other distributions** — install the development packages for these CMake packages:

| CMake package | What it is |
|---|---|
| `ECM` ≥ 6.0 | extra-cmake-modules |
| `Qt6` ≥ 6.6: Core, Gui, Widgets, Network, Sql, Qml, Quick, QuickControls2, WebEngineQuick, NetworkAuth, Test | Qt base, declarative, webengine, networkauth |
| `KF6`: Kirigami, I18n, CoreAddons, Config, ColorScheme, Mime (KMime), Notifications, NotifyConfig, StatusNotifierItem | KDE Frameworks 6 |
| `KF6KirigamiAddons` | kirigami-addons |
| `KPim6IMAP`, `KPim6SMTP` | KIMAP and KSMTP (KDE PIM libraries) |
| `Qt6Keychain` | qtkeychain for Qt 6 |

Runtime: `qqc2-desktop-style`, Cyrus SASL, SQLite (Qt's `QSQLITE` driver), and the
`libkdexoauth2` SASL plugin from libkgapi.

## Build

```sh
git clone https://github.com/topke63/Plasma-Mail.git
cd Plasma-Mail
cmake --preset release            # or: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build --preset release
```

Presets: `dev` (Debug, `build/`), `release` (RelWithDebInfo, `build-release/`),
`asan` (Debug with AddressSanitizer and UBSan, `build-asan/`).

### Run the tests (optional)

```sh
ctest --preset release
```

### Run without installing

```sh
./build-release/bin/plasma-mail
```

## Install

Per user (no root; make sure `~/.local/bin` is in your `PATH`):

```sh
cmake --install build-release --prefix ~/.local
```

System-wide:

```sh
sudo cmake --install build-release --prefix /usr
```

This installs the `plasma-mail` binary, the application icon (normal and "unread" variants),
the launcher entry (`org.topke.plasmamail` desktop file) and the notification definitions.
Log out and in, or run `kbuildsycoca6`, if the launcher entry does not show up at once.

To update: `git pull`, rebuild and install again.

## Set up accounts

Start Plasma Mail and use **Settings → Accounts → Add Account…**.

### Yahoo

Yahoo rejects your normal password over IMAP. Create an app password first:
Yahoo → Account security → turn on 2-step verification → **Generate app password**.
Enter your address and that app password in the app.

### Gmail

Gmail uses Google sign-in, which needs an OAuth client of your own (this repository does not
ship one). You only create it once.

1. Open <https://console.cloud.google.com> and create a project.
2. **Google Auth Platform → Branding / OAuth consent screen:** user type *External*, app name
   "Plasma Mail". Add your Gmail address as a **test user**, or publish the app "In production"
   (unverified is fine for personal use).
3. **Clients → Create client:** type **Desktop app**. Copy the client ID and secret.
4. Give them to Plasma Mail, either in `~/.config/plasmamailrc`:

   ```ini
   [GoogleOAuth]
   ClientId=…apps.googleusercontent.com
   ClientSecret=…
   ```

   or as environment variables `PLASMAMAIL_GOOGLE_CLIENT_ID` and
   `PLASMAMAIL_GOOGLE_CLIENT_SECRET`. Restart Plasma Mail afterwards.
5. In the app choose **Gmail** and sign in in your browser. The redirect goes to a temporary
   `http://127.0.0.1:<port>/` that only the app listens on.

The client is never committed anywhere; keep the secret out of version control.

#### Gmail contacts (optional)

For recipient autocomplete from your Google contacts:

1. In the same Google Cloud project, enable the **People API**.
2. Add the scopes `https://www.googleapis.com/auth/contacts.readonly` and
   `https://www.googleapis.com/auth/contacts.other.readonly` to the OAuth consent screen
   (in "Testing" mode the account must be a test user).
3. In **Settings → Contacts**, click **Allow Access to Contacts…** for the account
   (existing tokens do not carry the new scopes).

## Files

| Path | Contents |
|---|---|
| `~/.config/plasmamailrc` | accounts (no passwords), storage settings, `[GoogleOAuth]` client |
| system keychain | app passwords and OAuth tokens |
| `~/.local/share/plasma-mail/` | local message cache |

## Troubleshooting

- **"No Google OAuth client is set up"** — step 4 of the Gmail setup is missing; restart the app
  after editing `plasmamailrc`.
- **Gmail signs in but cannot log in to IMAP** — the `libkdexoauth2` SASL plugin is missing
  (`/usr/lib/sasl2/libkdexoauth2.so`; package `libkgapi` on Arch).
- **Yahoo says the password is wrong** — you need an app password, not your web login password.
- **CMake cannot find `KPim6IMAP` / `KPim6SMTP`** — install KIMAP and KSMTP development
  packages from KDE Gear 26.08 or newer.

## Development

- Source layout: `src/engine` (no UI), `src/models`, `src/app` (application), `src/qml` (UI),
  `autotests`, `tools` (`imap-probe`, manual IMAP checks; not installed).
- QML lint: `cmake --build --preset dev --target all_qmllint`.
- Formatting: `cmake --build build --target clang-format`; a pre-commit hook checks changed lines.
- The task list and design notes are in [`PLAN.md`](PLAN.md).

## License

GPL-2.0-or-later. See [`LICENSES/GPL-2.0-or-later.txt`](LICENSES/GPL-2.0-or-later.txt).
