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

import QtQuick

Rectangle {
    id: root

    property alias text: badgeText.text
    property alias fontColor: badgeText.color

    implicitWidth: badgeText.implicitWidth + root.radius
    implicitHeight: badgeText.implicitHeight + 6
    color: "lightgreen"
    radius: root.height / 2
    border.width: 1
    border.color: "gray"

    Text {
        id: badgeText

        anchors.centerIn: parent
    }
}
