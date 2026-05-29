// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "providers/OpenAIResponsesProvider.hpp"

namespace QodeAssist::Providers {

class QwenResponsesProvider : public OpenAIResponsesProvider
{
    Q_OBJECT
public:
    explicit QwenResponsesProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    QString apiKey() const override;
    QFuture<QList<QString>> getInstalledModels(const QString &url) override;
    PluginLLMCore::ProviderCapabilities capabilities() const override;
};

} // namespace QodeAssist::Providers
