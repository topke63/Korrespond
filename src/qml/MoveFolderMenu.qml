// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQml
import QtQuick.Controls as QQC2
import PlasmaMail

// The folders of the open message's account; picking one moves the message there. Used as the
// toolbar's menu and as a submenu of the message list's context menu.
QQC2.Menu {
    id: menu

    Instantiator {
        model: MailController.messageFolderModel

        delegate: QQC2.MenuItem {
            required property var model

            text: " ".repeat(model.depth * 2) + model.name
            icon.name: model.iconName
            enabled: model.path !== MailController.currentMessageFolder
            onTriggered: MailController.moveMessage(model.path)
        }
        onObjectAdded: (index, object) => menu.insertItem(index, object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
    }
}
