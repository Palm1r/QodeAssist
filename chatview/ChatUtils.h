#pragma once

#include <qobject.h>
#include <qqmlintegration.h>

namespace QodeAssist::Chat {
// Q_NAMESPACE

class ChatUtils : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ChatUtils)

public:
    explicit ChatUtils(QObject *parent = nullptr)
        : QObject(parent) {};

    Q_INVOKABLE void copyToClipboard(const QString &text);
};

} // namespace QodeAssist::Chat
