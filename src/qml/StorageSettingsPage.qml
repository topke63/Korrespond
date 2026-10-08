// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.coreaddons as KCoreAddons
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.dateandtime as DateAndTime
import PlasmaMail

Kirigami.ScrollablePage {
    id: page

    // The values being edited; saved with Apply. Unit numbers match StorageSettings' enums.
    property bool enabledValue
    property string importSince // ISO date, empty = all mail
    property int keepValue
    property int keepUnit
    property int sizeValue
    property int sizeUnit
    // Last date picked, so switching to "All mail" and back keeps it.
    property string pickedDate

    readonly property int keepForever: 4
    readonly property int sizeUnlimited: 2
    readonly property bool changed: enabledValue !== MailController.storageEnabled
        || importSince !== MailController.importSince
        || keepValue !== MailController.keepValue || keepUnit !== MailController.keepUnit
        || sizeValue !== MailController.sizeValue || sizeUnit !== MailController.sizeUnit

    function load() {
        enabledValue = MailController.storageEnabled;
        importSince = MailController.importSince;
        keepValue = MailController.keepValue;
        keepUnit = MailController.keepUnit;
        sizeValue = MailController.sizeValue;
        sizeUnit = MailController.sizeUnit;
        pickedDate = importSince.length > 0 ? importSince : MailController.defaultStorageSettings().importSince;
    }

    function resetToDefaults() {
        const defaults = MailController.defaultStorageSettings();
        importSince = defaults.importSince;
        pickedDate = defaults.importSince;
        keepValue = defaults.keepValue;
        keepUnit = defaults.keepUnit;
        sizeValue = defaults.sizeValue;
        sizeUnit = defaults.sizeUnit;
    }

    // "yyyy-MM-dd" ⇄ local Date, without going through UTC (which can shift the day).
    function toDate(iso: string): date {
        const parts = iso.split("-");
        return new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]));
    }

    function apply() {
        MailController.saveStorageSettings(enabledValue, importSince, keepValue, keepUnit, sizeValue, sizeUnit);
    }

    title: KI18n.i18nc("@title", "Local Storage")
    Component.onCompleted: load()

    Connections {
        target: MailController
        function onStorageSettingsChanged() {
            page.load();
        }
    }

    Kirigami.FormLayout {
        QQC2.Switch {
            Kirigami.FormData.label: KI18n.i18nc("@option:check", "Store mail on this computer:")
            checked: page.enabledValue
            onToggled: page.enabledValue = checked
        }

        ColumnLayout {
            Kirigami.FormData.label: KI18n.i18nc("@label", "Import mail from:")
            enabled: page.enabledValue
            spacing: 0

            QQC2.RadioButton {
                text: KI18n.i18nc("@option:radio", "All mail")
                checked: page.importSince.length === 0
                onToggled: page.importSince = ""
            }
            RowLayout {
                QQC2.RadioButton {
                    id: sinceRadio

                    text: KI18n.i18nc("@option:radio", "Since:")
                    checked: page.importSince.length > 0
                    onToggled: page.importSince = page.pickedDate
                }
                QQC2.Button {
                    enabled: sinceRadio.checked
                    icon.name: "view-calendar"
                    text: page.pickedDate.length > 0
                        ? page.toDate(page.pickedDate).toLocaleDateString(Qt.locale(), Locale.ShortFormat) : ""
                    onClicked: datePopup.open()
                }
            }
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18nc("@label", "Keep mail for:")
            enabled: page.enabledValue

            QQC2.SpinBox {
                from: 1
                to: 999
                value: page.keepValue
                enabled: page.keepUnit !== page.keepForever
                onValueModified: page.keepValue = value
            }
            QQC2.ComboBox {
                model: [
                    KI18n.i18nc("@item:inlistbox keep mail for", "days"),
                    KI18n.i18nc("@item:inlistbox keep mail for", "weeks"),
                    KI18n.i18nc("@item:inlistbox keep mail for", "months"),
                    KI18n.i18nc("@item:inlistbox keep mail for", "years"),
                    KI18n.i18nc("@item:inlistbox keep mail for", "forever")
                ]
                currentIndex: page.keepUnit
                onActivated: index => page.keepUnit = index
            }
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18nc("@label", "Size limit:")
            enabled: page.enabledValue

            QQC2.SpinBox {
                from: 1
                to: 9999
                value: page.sizeValue
                enabled: page.sizeUnit !== page.sizeUnlimited
                textFromValue: (value, locale) => value.toString() // no thousands separator
                onValueModified: page.sizeValue = value
            }
            QQC2.ComboBox {
                model: [
                    KI18n.i18nc("@item:inlistbox size unit", "MB"),
                    KI18n.i18nc("@item:inlistbox size unit", "GB"),
                    KI18n.i18nc("@item:inlistbox size limit", "unlimited")
                ]
                currentIndex: page.sizeUnit
                onActivated: index => page.sizeUnit = index
            }
        }

        RowLayout {
            Kirigami.FormData.label: KI18n.i18nc("@label", "Currently used:")

            QQC2.Label {
                text: MailController.storageReady
                    ? KCoreAddons.Format.formatByteSize(MailController.storageUsedBytes)
                    : KI18n.i18nc("@info", "Not available: the local store could not be opened")
                enabled: MailController.storageReady
            }
            QQC2.BusyIndicator {
                // Same height as the text, so the row doesn't grow while syncing.
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                running: MailController.syncing
                visible: running
            }
            QQC2.Label {
                text: KI18n.i18nc("@info", "Downloading…")
                visible: MailController.syncing
            }
        }

        QQC2.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            text: KI18n.i18nc("@info", "Limits only remove the copies on this computer. Nothing is ever deleted on the server.")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
        }

        RowLayout {
            QQC2.Button {
                text: KI18n.i18nc("@action:button", "Clean Up Now")
                icon.name: "edit-clear-history"
                // Uses the saved limits; Apply already cleans up with the new ones.
                enabled: MailController.storageReady && MailController.storageEnabled
                onClicked: MailController.cleanUpNow()
            }
            QQC2.Button {
                text: KI18n.i18nc("@action:button", "Defaults")
                icon.name: "edit-reset"
                enabled: page.enabledValue
                onClicked: page.resetToDefaults()
            }
            QQC2.Button {
                text: KI18n.i18nc("@action:button", "Apply")
                icon.name: "dialog-ok-apply"
                enabled: page.changed
                onClicked: page.apply()
            }
        }
    }

    DateAndTime.DatePopup {
        id: datePopup

        // No maximumDate: in Kirigami Addons 1.15 setting it hangs the app when the page opens.
        value: page.pickedDate.length > 0 ? page.toDate(page.pickedDate) : new Date()
        onAccepted: {
            const today = new Date();
            page.pickedDate = Qt.formatDate(value > today ? today : value, "yyyy-MM-dd"); // no future dates
            page.importSince = page.pickedDate;
        }
    }
}
