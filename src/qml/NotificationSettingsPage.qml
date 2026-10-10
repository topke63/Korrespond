// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import PlasmaMail

// Settings → Notifications: how often to look for new mail and what closing the window does.
// Changes apply at once.
FormCard.FormCardPage {
    id: page

    title: KI18n.i18nc("@title", "Notifications")

    FormCard.FormHeader {
        title: KI18n.i18nc("@title:group", "New Mail")
    }

    FormCard.FormCard {
        FormCard.FormSpinBoxDelegate {
            id: minutesDelegate

            label: KI18n.i18nc("@label:spinbox", "Check for new mail every (minutes)")
            description: KI18n.i18nc("@info", "Plasma Mail looks at your inboxes this often and shows a notification when new mail arrived.")
            from: 1
            to: 60
            value: MailController.checkMinutes
            onValueChanged: {
                if (value !== MailController.checkMinutes) {
                    MailController.saveAppSettings(MailController.keepInTray, value);
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: KI18n.i18nc("@option:check", "Use push (IDLE) when the server supports it")
            description: KI18n.i18nc("@info", "New mail shows up at once instead of at the next check. Accounts without push still use the interval above.")
            checked: MailController.pushEnabled
            onToggled: MailController.setPushEnabled(checked)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: KI18n.i18nc("@option:check", "Keep running in the tray when the window is closed")
            description: KI18n.i18nc("@info", "Closing the window hides it; use the tray icon to bring it back or to quit. New-mail notifications keep working.")
            checked: MailController.keepInTray
            onToggled: MailController.saveAppSettings(checked, MailController.checkMinutes)
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormButtonDelegate {
            text: KI18n.i18nc("@action:button", "Configure Notifications…")
            icon.name: "preferences-desktop-notification"
            description: MailController.notificationsConfigurable
                ? KI18n.i18nc("@info", "Choose how the desktop shows and sounds new-mail notifications.")
                : KI18n.i18nc("@info", "Available once Plasma Mail is installed (cmake --install).")
            enabled: MailController.notificationsConfigurable
            onClicked: MailController.configureNotifications()
        }
    }
}
