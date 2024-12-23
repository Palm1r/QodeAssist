/*
 * Copyright (C) 2024 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QQuickItem>

#include "ChatModel.hpp"
#include "ClientInterface.hpp"

namespace QodeAssist::Chat {

class ChatRootView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QodeAssist::Chat::ChatModel *chatModel READ chatModel NOTIFY chatModelChanged FINAL)
    Q_PROPERTY(QString currentTemplate READ currentTemplate NOTIFY currentTemplateChanged FINAL)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor CONSTANT FINAL)
    Q_PROPERTY(QColor primaryColor READ primaryColor CONSTANT FINAL)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor CONSTANT FINAL)
    Q_PROPERTY(QColor codeColor READ codeColor CONSTANT FINAL)
    Q_PROPERTY(bool isSharingCurrentFile READ isSharingCurrentFile NOTIFY
                   isSharingCurrentFileChanged FINAL)
    QML_ELEMENT

public:
    ChatRootView(QQuickItem *parent = nullptr);

    ChatModel *chatModel() const;
    QString currentTemplate() const;

    QColor backgroundColor() const;
    QColor primaryColor() const;
    QColor secondaryColor() const;

    QColor codeColor() const;

    bool isSharingCurrentFile() const;

    void saveHistory(const QString &filePath);
    void loadHistory(const QString &filePath);

    Q_INVOKABLE void showSaveDialog();
    Q_INVOKABLE void showLoadDialog();

public slots:
    void sendMessage(const QString &message, bool sharingCurrentFile = false) const;
    void copyToClipboard(const QString &text);
    void cancelRequest();

signals:
    void chatModelChanged();
    void currentTemplateChanged();

    void isSharingCurrentFileChanged();

private:
    void generateColors();
    QColor generateColor(const QColor &baseColor,
                         float hueShift,
                         float saturationMod,
                         float lightnessMod);

    QString getChatsHistoryDir() const;
    QString getSuggestedFileName() const;

    ChatModel *m_chatModel;
    ClientInterface *m_clientInterface;
    QString m_currentTemplate;
    QColor m_primaryColor;
    QColor m_secondaryColor;
    QColor m_codeColor;
};

} // namespace QodeAssist::Chat
