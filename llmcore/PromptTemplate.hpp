/* 
 * Copyright (C) 2024 Petr Mironychev
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
#include <QList>
#include <QString>

#include "ContextData.hpp"
#include "ProviderID.hpp"

namespace QodeAssist::LLMCore {

enum class TemplateType { Chat, FIM };

class PromptTemplate
{
public:
    virtual ~PromptTemplate() = default;
    virtual TemplateType type() const = 0;
    virtual QString name() const = 0;
    virtual QStringList stopWords() const = 0;
    virtual void prepareRequest(QJsonObject &request, const ContextData &context) const = 0;
    virtual QString description() const = 0;
    virtual bool isSupportProvider(ProviderID id) const = 0;
};
} // namespace QodeAssist::LLMCore
