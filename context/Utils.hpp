// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>

namespace QodeAssist::Context {

inline QString extractFilePathFromRequest(const QJsonObject &request)
{
    QJsonObject params = request["params"].toObject();
    QJsonObject doc = params["doc"].toObject();
    QString uri = doc["uri"].toString();
    return QUrl(uri).toLocalFile();
}

} // namespace QodeAssist::Context
