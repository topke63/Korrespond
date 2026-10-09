# SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Manual IMAP check against a real account (not installed).
add_executable(imap-probe imapprobe.cpp)
# plasmamail_engine links Qt6::Gui and Qt6::NetworkAuth publicly, which OAuthClient needs.
target_link_libraries(imap-probe PRIVATE plasmamail_engine Qt6::Gui)
