#include "ChatUtils.h"

#include <QClipboard>
#include <QGuiApplication>

namespace QodeAssist::Chat {

void ChatUtils::copyToClipboard(const QString &text)
{
    qDebug() << "call clipboard" << text;
    QGuiApplication::clipboard()->setText(text);
}

} // namespace QodeAssist::Chat
