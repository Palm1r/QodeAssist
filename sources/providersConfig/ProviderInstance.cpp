// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProviderInstance.hpp"

#include <QUrl>

namespace QodeAssist::Providers {

QString ProviderInstance::validate(
    const ProviderInstance &inst, const QStringList &knownClientApis)
{
    if (inst.name.isEmpty())
        return QStringLiteral("Provider instance has no name");
    if (inst.clientApi.isEmpty())
        return QStringLiteral("Provider instance '%1' has no client_api").arg(inst.name);
    if (!knownClientApis.isEmpty() && !knownClientApis.contains(inst.clientApi)) {
        return QStringLiteral("Provider instance '%1' references unknown client_api '%2'")
            .arg(inst.name, inst.clientApi);
    }
    if (inst.url.isEmpty())
        return QStringLiteral("Provider instance '%1' has no URL").arg(inst.name);
    const QUrl parsed(inst.url);
    if (!parsed.isValid()
        || (parsed.scheme() != QLatin1StringView{"http"}
            && parsed.scheme() != QLatin1StringView{"https"})) {
        return QStringLiteral("Provider instance '%1' has an invalid or unsafe URL: %2")
            .arg(inst.name, inst.url);
    }
    return {};
}

QString ProviderInstance::warnings(const ProviderInstance &inst)
{
    const QUrl parsed(inst.url);
    if (parsed.scheme() == QLatin1StringView{"http"} && !inst.apiKeyRef.isEmpty()) {
        const QString host = parsed.host();
        const bool isLoopback = host == QLatin1StringView{"localhost"}
                                || host == QLatin1StringView{"127.0.0.1"}
                                || host == QLatin1StringView{"::1"};
        if (!isLoopback) {
            return QStringLiteral(
                       "URL uses plaintext http:// to '%1' but the provider has an API key. "
                       "Any request will transmit the key unencrypted — prefer https://.")
                .arg(host);
        }
    }
    return {};
}

} // namespace QodeAssist::Providers
