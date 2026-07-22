// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>
#include <QStringList>

#include "context/IDocumentReader.hpp"
#include "logger/IRequestPerformanceLogger.hpp"
#include "providers/IProviderRegistry.hpp"
#include "providers/Provider.hpp"

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace QodeAssist {

class FakeCompletionRegistry : public Providers::IProviderRegistry
{
public:
    explicit FakeCompletionRegistry(Providers::Provider *provider)
        : m_provider(provider)
    {}

    Providers::Provider *getProviderByName(const QString &) override { return m_provider; }
    QStringList providersNames() const override
    {
        return m_provider ? QStringList{m_provider->name()} : QStringList{};
    }

    void setProvider(Providers::Provider *provider) { m_provider = provider; }

private:
    Providers::Provider *m_provider = nullptr;
};

class FakeCompletionDocumentReader : public Context::IDocumentReader
{
public:
    Context::DocumentInfo readDocument(const QString &path) const override
    {
        if (!document)
            return {};
        return {document, QStringLiteral("text/x-c++src"), path};
    }

    QTextDocument *document = nullptr;
};

class FakePerformanceLogger : public IRequestPerformanceLogger
{
public:
    void startTimeMeasurement(const QString &) override {}
    void endTimeMeasurement(const QString &) override {}
    void logPerformance(const QString &, qint64) override {}
};

} // namespace QodeAssist
