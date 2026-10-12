// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtQml/qqmlregistration.h>

#include "emojimodel.h"

// Makes EmojiModel (in the models library) available to QML as `EmojiModel`.
struct EmojiModelForeign {
    Q_GADGET
    QML_FOREIGN(EmojiModel)
    QML_NAMED_ELEMENT(EmojiModel)
};
