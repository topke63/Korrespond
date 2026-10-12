// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtQml/qqmlregistration.h>

#include "formathandler.h"

// Makes FormatHandler (in the models library) available to QML as `FormatHandler`.
struct FormatHandlerForeign {
    Q_GADGET
    QML_FOREIGN(FormatHandler)
    QML_NAMED_ELEMENT(FormatHandler)
};
