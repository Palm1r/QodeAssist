// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <memory>

#include <QFuture>
#include <QList>
#include <QObject>
#include <QString>

#include "AgentConfig.hpp"

namespace QodeAssist {

namespace Providers {
class Provider;
}
namespace Templates {
class JsonPromptTemplate;
class PromptTemplate;
}

class Agent : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Agent)
public:
    Agent(AgentConfig config, Providers::Provider *providerOwned, QObject *parent = nullptr);
    ~Agent() override;

    const AgentConfig &config() const noexcept { return m_config; }

    Providers::Provider *provider() noexcept { return m_provider; }
    const Providers::Provider *provider() const noexcept { return m_provider; }

    Templates::PromptTemplate *promptTemplate() noexcept;
    const Templates::PromptTemplate *promptTemplate() const noexcept;

    bool isValid() const noexcept { return m_invalidReason.isEmpty(); }
    QString invalidReason() const { return m_invalidReason; }

    QFuture<QList<QString>> installedModels();

private:
    AgentConfig m_config;
    std::unique_ptr<Templates::JsonPromptTemplate> m_promptTemplate; // owned
    Providers::Provider *m_provider = nullptr;                        // child of this
    QString m_invalidReason;
};

} // namespace QodeAssist
