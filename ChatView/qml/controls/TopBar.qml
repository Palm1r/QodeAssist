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
    property alias compressButton: compressButtonId
    property alias cancelCompressButton: cancelCompressButtonId
    property alias tokensBadge: tokensBadgeId
    property alias recentPath: recentPathId
    property alias openChatHistory: openChatHistoryId
    property alias pinButton: pinButtonId
    property alias contextButton: contextButtonId
    property alias toolsButton: toolsButtonId
    property alias thinkingMode: thinkingModeId
    property alias settingsButton: settingsButtonId
    property alias configSelector: configSelectorId
    property alias roleSelector: roleSelector

    property bool isCompressing: false

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

        Row {
            id: firstRow

            spacing: 10

            QoAButton {
                id: pinButtonId

                anchors.verticalCenter: parent.verticalCenter
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

            QoAComboBox {
                id: configSelectorId

                implicitHeight: 25

                model: []
                currentIndex: 0

                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: qsTr("Switch AI configuration")
            }

            QoAComboBox {
                id: roleSelector

                implicitHeight: 25

                model: []
                currentIndex: 0

                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: qsTr("Switch agent role (different system prompts)")
            }

            QoAButton {
                id: toolsButtonId

                anchors.verticalCenter: parent.verticalCenter

                checkable: true
                opacity: enabled ? 1.0 : 0.2

                icon {
                    source: checked ? "qrc:/qt/qml/ChatView/icons/tools-icon-on.svg"
                                    : "qrc:/qt/qml/ChatView/icons/tools-icon-off.svg"
                    color: palette.window.hslLightness > 0.5 ? "#000000" : "#FFFFFF"
                    height: 15
                    width: 15
                }

                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: {
                    if (!toolsButtonId.enabled) {
                        return qsTr("Tools are disabled in General Settings")
                    }
                    return checked
                            ? qsTr("Tools enabled: AI can use tools to read files, search project, and build code")
                            : qsTr("Tools disabled: Simple conversation without tool access")
                }
            }

            QoAButton {
                id: thinkingModeId

                anchors.verticalCenter: parent.verticalCenter

                checkable: true
                opacity: enabled ? 1.0 : 0.2

                icon {
                    source: checked ? "qrc:/qt/qml/ChatView/icons/thinking-icon-on.svg"
                                    : "qrc:/qt/qml/ChatView/icons/thinking-icon-off.svg"
                    color: palette.window.hslLightness > 0.5 ? "#000000" : "#FFFFFF"
                    height: 15
                    width: 15
                }

                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: enabled ? (checked ? qsTr("Thinking Mode enabled (Check model list support it)")
                                                 : qsTr("Thinking Mode disabled"))
                                      : qsTr("Thinking Mode is not available for this provider")
            }

            QoAButton {
                id: settingsButtonId

                anchors.verticalCenter: parent.verticalCenter

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/settings-icon.svg"
                    color: palette.window.hslLightness > 0.5 ? "#000000" : "#FFFFFF"
                    height: 15
                    width: 15
                }

                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: qsTr("Open Chat Assistant Settings")
            }
        }

        Item {
            height: firstRow.height
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
            id: secondRow

            Layout.preferredWidth: root.width
            Layout.preferredHeight: firstRow.height

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
                id: compressButtonId

                visible: !root.isCompressing

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/compress-icon.svg"
                    height: 15
                    width: 15
                }
                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: qsTr("Compress chat (create summarized copy using LLM)")
            }

            Row {
                id: compressingRow
                visible: root.isCompressing
                spacing: 6

                BusyIndicator {
                    id: compressBusyIndicator
                    running: root.isCompressing
                    width: 16
                    height: 16
                }

                Text {
                    text: qsTr("Compressing...")
                    color: palette.text
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    height: parent.height
                }

                QoAButton {
                    id: cancelCompressButtonId
                    text: qsTr("Cancel")

                    ToolTip.visible: hovered
                    ToolTip.delay: 250
                    ToolTip.text: qsTr("Cancel compression")
                }
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
                id: contextButtonId

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/context-icon.svg"
                    color: palette.window.hslLightness > 0.5 ? "#000000" : "#FFFFFF"
                    height: 15
                    width: 15
                }

                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: qsTr("View chat context (system prompt, role, rules)")
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
