/* 
 * Copyright (C) 2025 Petr Mironychev
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

#include "CodestralProvider.hpp"

#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

QString CodestralProvider::name() const
{
    return "Codestral";
}

QString CodestralProvider::url() const
{
    return "https://codestral.mistral.ai";
}

bool CodestralProvider::supportsModelListing() const
{
    return false;
}

QString CodestralProvider::apiKey() const
{
    return Settings::providerSettings().codestralApiKey();
}

} // namespace QodeAssist::Providers
