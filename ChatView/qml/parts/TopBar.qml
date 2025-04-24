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

import QtQuick
import QtQuick.Layouts
import ChatView

Rectangle {
    id: root

    property alias saveButton: saveButtonId
    property alias loadButton: loadButtonId
    property alias clearButton: clearButtonId
    property alias tokensBadge: tokensBadgeId
    property alias recentPath: recentPathId
    property alias openChatHistory: openChatHistoryId
    property alias expandScrollbar: expandScrollbarId

    color: palette.window.hslLightness > 0.5 ?
               Qt.darker(palette.window, 1.1) :
               Qt.lighter(palette.window, 1.1)

    RowLayout {
        anchors {
            left: parent.left
            leftMargin: 5
            right: parent.right
            rightMargin: 5
            verticalCenter: parent.verticalCenter
        }

        spacing: 10

        QoAButton {
            id: saveButtonId

            text: qsTr("Save")
        }

        QoAButton {
            id: loadButtonId

            text: qsTr("Load")
        }

        QoAButton {
            id: clearButtonId

            text: qsTr("Clear")
        }

        Text {
            id: recentPathId

            elide: Text.ElideMiddle
            color: palette.text
        }

        QoAButton {
            id: openChatHistoryId

            text: qsTr("Show in system")
        }

        Item {
            Layout.fillWidth: true
        }

        Badge {
            id: tokensBadgeId
        }

        QoAButton {
            id: expandScrollbarId

            width: 16
            height: 16
        }
    }
}
