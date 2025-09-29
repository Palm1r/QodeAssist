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
import QtQuick.Controls
import ChatView
import UIControls

Rectangle {
    id: root

    property alias saveButton: saveButtonId
    property alias loadButton: loadButtonId
    property alias clearButton: clearButtonId
    property alias tokensBadge: tokensBadgeId
    property alias recentPath: recentPathId
    property alias openChatHistory: openChatHistoryId
    property alias pinButton: pinButtonId

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
            id: pinButtonId

            checkable: true

            icon {
                source: checked ? "qrc:/qt/qml/ChatView/icons/window-lock.svg"
                                : "qrc:/qt/qml/ChatView/icons/window-unlock.svg"
                color: palette.window.hslLightness > 0.5 ? "#000000" : "#FFFFFF"
                height: 15
                width: 15
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: checked ? qsTr("Unpin chat window")
                                  : qsTr("Pin chat window to the top")
        }

        QoAButton {
            id: saveButtonId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/save-chat-dark.svg"
                height: 15
                width: 8
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Save chat to *.json file")
        }

        QoAButton {
            id: loadButtonId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/load-chat-dark.svg"
                height: 15
                width: 8
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Load chat from *.json file")
        }

        QoAButton {
            id: clearButtonId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/clean-icon-dark.svg"
                height: 15
                width: 8
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Clean chat")
        }

        Text {
            id: recentPathId

            elide: Text.ElideMiddle
            color: palette.text
        }

        QoAButton {
            id: openChatHistoryId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/file-in-system.svg"
                height: 15
                width: 15
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Show in system")
        }

        Item {
            Layout.fillWidth: true
        }

        Badge {
            id: tokensBadgeId

            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Current amount tokens in chat and LLM limit threshold")
        }
    }
}
