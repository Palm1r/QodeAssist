// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <QJsonObject>
#include <QString>

#include <inja/inja.hpp>

#include "PromptTemplate.hpp"

namespace QodeAssist {
struct AgentConfig;
}

namespace QodeAssist::Templates {

class JsonPromptTemplate : public PromptTemplate
{
public:
    static std::unique_ptr<JsonPromptTemplate> fromConfig(
        const AgentConfig &cfg, QString *error = nullptr);

    QString name() const override { return m_name; }
    QString description() const override { return m_description; }

    bool isSupportProvider(Providers::ProviderID) const override { return true; }

    [[nodiscard]] bool buildFullRequest(
        QJsonObject &request, const ContextData &context) const override;

private:
    JsonPromptTemplate() = default;

    std::optional<QJsonObject> renderBody(const ContextData &context) const;

    QString m_name;
    QString m_description;
    QJsonObject m_body;
    std::vector<QString> m_partialRoots;
    mutable inja::Environment m_env;
    mutable std::mutex m_renderMutex;
};

} // namespace QodeAssist::Templates
