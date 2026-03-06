/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "ValidationUtils.hpp"

#include <QJsonArray>

namespace QodeAssist::LLMCore {

QStringList ValidationUtils::validateRequestFields(
    const QJsonObject &request, const QJsonObject &templateObj)
{
    QStringList errors;
    validateFields(request, templateObj, errors);
    validateNestedObjects(request, templateObj, errors);
    return errors;
}

void ValidationUtils::validateFields(
    const QJsonObject &request, const QJsonObject &templateObj, QStringList &errors)
{
    for (auto it = request.begin(); it != request.end(); ++it) {
        if (!templateObj.contains(it.key())) {
            errors << QString("unknown field '%1'").arg(it.key());
        }
    }
}

void ValidationUtils::validateNestedObjects(
    const QJsonObject &request, const QJsonObject &templateObj, QStringList &errors)
{
    for (auto it = request.begin(); it != request.end(); ++it) {
        if (templateObj.contains(it.key()) && it.value().isObject()
            && templateObj[it.key()].isObject()) {
            validateFields(it.value().toObject(), templateObj[it.key()].toObject(), errors);
            validateNestedObjects(it.value().toObject(), templateObj[it.key()].toObject(), errors);
        }
    }
}

} // namespace QodeAssist::LLMCore
