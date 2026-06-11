// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QMetaType>
#include <QString>

#include <utility>

namespace QodeAssist {

enum class ErrorCategory {
    Config,
    Auth,
    Network,
    Provider,
    Validation,
    Tool,
};

struct ErrorInfo
{
    ErrorCategory category = ErrorCategory::Provider;
    QString message;
    QString providerDetail;

    bool isEmpty() const noexcept { return message.isEmpty(); }
};

[[nodiscard]] inline ErrorInfo makeError(
    ErrorCategory category, QString message, QString providerDetail = QString())
{
    return ErrorInfo{category, std::move(message), std::move(providerDetail)};
}

[[nodiscard]] inline ErrorCategory categorizeProviderError(const QString &raw)
{
    const QString text = raw.toLower();

    const auto contains = [&text](const char *needle) {
        return text.contains(QLatin1String(needle));
    };

    if (contains("401") || contains("403") || contains("unauthorized")
        || contains("forbidden") || contains("api key") || contains("apikey")
        || contains("authentication") || contains("invalid token"))
        return ErrorCategory::Auth;

    if (contains("timeout") || contains("timed out") || contains("connection")
        || contains("could not resolve") || contains("unreachable")
        || contains("network") || contains("ssl") || contains("refused"))
        return ErrorCategory::Network;

    return ErrorCategory::Provider;
}

} // namespace QodeAssist

Q_DECLARE_METATYPE(QodeAssist::ErrorInfo)
