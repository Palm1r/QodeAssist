// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProviderSecretsStore.hpp"

#include <coreplugin/icore.h>
#include <utils/qtcsettings.h>

namespace QodeAssist::Providers {

namespace {

constexpr auto kGroup = "QodeAssist/Keychain";

Utils::Key settingsKey(const QString &ref)
{
    return Utils::Key(QStringLiteral("%1/%2")
                          .arg(QLatin1StringView(kGroup))
                          .arg(ref)
                          .toUtf8());
}

} // namespace

ProviderSecretsStore::ProviderSecretsStore(QObject *parent)
    : QObject(parent)
{}

ProviderSecretsStore::~ProviderSecretsStore() = default;

QString ProviderSecretsStore::readKeySync(const QString &ref) const
{
    if (ref.isEmpty())
        return {};
    auto *s = Core::ICore::settings();
    if (!s)
        return {};
    return s->value(settingsKey(ref)).toString();
}

void ProviderSecretsStore::writeKey(const QString &ref, const QString &value)
{
    if (ref.isEmpty())
        return;
    auto *s = Core::ICore::settings();
    if (!s)
        return;
    s->setValue(settingsKey(ref), value);
    s->sync();
    emit keyChanged(ref);
}

void ProviderSecretsStore::eraseKey(const QString &ref)
{
    if (ref.isEmpty())
        return;
    auto *s = Core::ICore::settings();
    if (!s)
        return;
    s->remove(settingsKey(ref));
    s->sync();
    emit keyChanged(ref);
}

bool ProviderSecretsStore::hasKey(const QString &ref) const
{
    if (ref.isEmpty())
        return false;
    auto *s = Core::ICore::settings();
    if (!s)
        return false;
    return s->contains(settingsKey(ref));
}

} // namespace QodeAssist::Providers
