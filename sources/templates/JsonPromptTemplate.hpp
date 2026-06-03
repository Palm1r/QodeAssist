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

// Renderer for the request-body jinja template embedded in an
// AgentConfig. One per Agent — built inline from the config (no shared
// template registry, no model/provider filtering).
class JsonPromptTemplate : public PromptTemplate
{
public:
    // Build a renderer from an already-parsed agent config. Compiles
    // the jinja source via inja once. On failure returns nullptr and
    // populates `*error` (existing content preserved; pass nullptr to
    // discard).
    static std::unique_ptr<JsonPromptTemplate> fromConfig(
        const AgentConfig &cfg, QString *error = nullptr);

    QString name() const override { return m_name; }
    QString description() const override { return m_description; }

    // Standalone-template filters are gone — each template is built for
    // exactly one agent, so it always matches its owner's provider/model.
    bool isSupportProvider(Providers::ProviderID) const override { return true; }
    bool isSupportModel(const QString &) const override { return true; }
    PromptShape shape() const override { return PromptShape::Chat; }

    void prepareRequest(QJsonObject &request, const ContextData &context) const override;

    [[nodiscard]] bool buildFullRequest(
        QJsonObject &request,
        const ContextData &context,
        bool thinkingEnabled = false) const override;

private:
    JsonPromptTemplate() = default;

    std::optional<QJsonObject> renderBody(const ContextData &context) const;

    QString m_name;
    QString m_description;

    // The literal request body, as a deep-mergeable object. String values
    // that contain jinja are rendered and spliced as JSON at request time;
    // literal strings and scalars pass through unchanged.
    QJsonObject m_body;

    // Roots searched (in order) by the `{% include %}` resolver. The first
    // is the bundled qrc partials prefix; an optional second is the user
    // agent's own directory, so user profiles can ship their own partials.
    std::vector<QString> m_partialRoots;

    // m_env is populated once in fromConfig() and never mutated again.
    // It is `mutable` only because inja::Environment::render() is not a
    // const member; m_renderMutex serialises those render() calls since
    // inja's render path is not internally re-entrant on one Environment.
    mutable inja::Environment m_env;
    mutable std::mutex m_renderMutex;
};

} // namespace QodeAssist::Templates
