// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentBinding.hpp"

#include <QCoreApplication>

namespace QodeAssist::Acp {

namespace {

bool isUsableId(const QString &id)
{
    if (id.size() > maxAgentBindingIdLength)
        return false;

    for (const QChar c : id) {
        if (c.category() == QChar::Other_Control || c.category() == QChar::Other_Format)
            return false;
    }

    return true;
}

} // namespace

QString AgentBinding::displayId() const
{
    return agentId.size() <= maxAgentBindingIdLength
               ? agentId
               : agentId.first(maxAgentBindingIdLength - 1) + QChar(0x2026);
}

QJsonObject AgentBinding::toJson() const
{
    QJsonObject json;
    json["agentId"] = agentId;
    if (!sessionId.isEmpty())
        json["sessionId"] = sessionId;
    return json;
}

AgentBinding AgentBinding::fromJson(const QJsonValue &value, QString *error)
{
    const auto fail = [error](const QString &reason) {
        if (error)
            *error = reason;
        return AgentBinding{};
    };

    if (value.isUndefined() || value.isNull())
        return {};

    if (!value.isObject()) {
        return fail(
            QCoreApplication::translate("QodeAssist", "the agent binding is not an object"));
    }

    const QJsonObject json = value.toObject();

    if (!json.value("agentId").isString() && json.contains("agentId")) {
        return fail(
            QCoreApplication::translate("QodeAssist", "the agent id is not a string"));
    }

    if (!json.value("sessionId").isString() && json.contains("sessionId")) {
        return fail(
            QCoreApplication::translate("QodeAssist", "the agent session id is not a string"));
    }

    AgentBinding binding{json["agentId"].toString(), json["sessionId"].toString()};

    if (binding.agentId.isEmpty() && !binding.sessionId.isEmpty()) {
        return fail(
            QCoreApplication::translate(
                "QodeAssist", "the agent binding names a session but no agent"));
    }

    if (!isUsableId(binding.agentId) || !isUsableId(binding.sessionId)) {
        return fail(
            QCoreApplication::translate("QodeAssist", "the agent binding holds an unusable id"));
    }

    return binding;
}

} // namespace QodeAssist::Acp
