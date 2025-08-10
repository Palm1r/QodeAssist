/*
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include <QQuickView>

namespace QodeAssist::Chat {

class ChatView : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(bool isPin READ isPin WRITE setIsPin NOTIFY isPinChanged FINAL)
public:
    ChatView();

    bool isPin() const;
    void setIsPin(bool newIsPin);

signals:
    void isPinChanged();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void saveSettings();
    void restoreSettings();

    bool m_isPin;
};

} // namespace QodeAssist::Chat
