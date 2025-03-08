/*
 * Copyright (C) 2025 Povilas Kanapickas  <povilas@radix.lt>
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

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
