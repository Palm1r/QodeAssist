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

#include "OpenRouterAIProvider.hpp"

#include "settings/ProviderSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace QodeAssist::Providers {

QString OpenRouterProvider::name() const
{
    return "OpenRouter";
}

QString OpenRouterProvider::url() const
{
    return "https://openrouter.ai/api";
}

QString OpenRouterProvider::apiKey() const
{
    return Settings::providerSettings().openRouterApiKey();
}

LLMCore::ProviderID OpenRouterProvider::providerID() const
{
    return LLMCore::ProviderID::OpenRouter;
}

} // namespace QodeAssist::Providers
