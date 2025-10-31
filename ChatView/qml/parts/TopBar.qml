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
    property alias rulesButton: rulesButtonId
    property alias agentModeSwitch: agentModeSwitchId
    property alias activeRulesCount: activeRulesCountId.text

    color: palette.window.hslLightness > 0.5 ?
               Qt.darker(palette.window, 1.1) :
               Qt.lighter(palette.window, 1.1)

    Flow {
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            margins: 5
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

        QoATextSlider {
            id: agentModeSwitchId

            leftText: "chat"
            rightText: "AI Agent"

            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: {
                if (!agentModeSwitchId.enabled) {
                    return qsTr("Tools are disabled in General Settings")
                }
                return checked
                        ? qsTr("Agent Mode: AI can use tools to read files, search project, and build code")
                        : qsTr("Chat Mode: Simple conversation without tool access")
            }
        }

        Item {
            height: agentModeSwitchId.height
            width: recentPathId.width

            Text {
                id: recentPathId

                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, root.width)
                elide: Text.ElideMiddle
                color: palette.text
                font.pixelSize: 12

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true

                    ToolTip.visible: containsMouse
                    ToolTip.delay: 500
                    ToolTip.text: recentPathId.text
                }
            }
        }

        RowLayout {
            Layout.preferredWidth: root.width

            spacing: 10

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

            QoAButton {
                id: rulesButtonId

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/rules-icon.svg"
                    height: 15
                    width: 15
                }
                text: " "

                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: root.activeRulesCount > 0
                              ? qsTr("View active project rules (%1)").arg(root.activeRulesCount)
                              : qsTr("View active project rules (no rules found)")

                Text {
                    id: activeRulesCountId

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 2
                        right: parent.right
                        rightMargin: 4
                    }

                    color: palette.text
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Badge {
                id: tokensBadgeId

                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: qsTr("Current amount tokens in chat and LLM limit threshold")
            }
        }
    }
}
