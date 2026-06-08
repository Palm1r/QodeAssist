// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import ChatView
import UIControls

Rectangle {
    id: root

    property bool isInEditor: false

    property alias saveButton: saveButtonId
    property alias loadButton: loadButtonId
    property alias clearButton: clearButtonId
    property alias newChatButton: newChatButtonId
    property alias tokensBadge: tokensBadgeId
    property alias recentPath: recentPathId
    property alias openChatHistory: openChatHistoryId
    property alias pinButton: pinButtonId
    property alias relocateButton: relocateButtonId
    property alias contextButton: contextButtonId
    property alias toolsButton: toolsButtonId
    property alias thinkingMode: thinkingModeId
    property alias settingsButton: settingsButtonId
    property alias configSelector: configSelectorId
    property alias roleSelector: roleSelector
    property alias relocateTooltip: relocateTooltipId

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

                QoAToolTip {
                    visible: pinButtonId.hovered
                    delay: 250
                    text: pinButtonId.checked ? qsTr("Unpin chat window")
                                              : qsTr("Pin chat window to the top")
                }
            }

            QoAButton {
                id: relocateButtonId

                anchors.verticalCenter: parent.verticalCenter

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/open-in-editor.svg"
                    color: palette.window.hslLightness > 0.5 ? "#000000" : "#FFFFFF"
                    height: 15
                    width: 15
                }

                QoAToolTip {
                    id: relocateTooltipId

                    visible: relocateButtonId.hovered
                    delay: 250
                }
            }

            QoASeparator {
                anchors.verticalCenter: parent.verticalCenter
            }

            QoAButton {
                id: clearButtonId

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/clean-icon-dark.svg"
                    height: 15
                    width: 8
                }

                QoAToolTip {
                    visible: clearButtonId.hovered
                    delay: 250
                    text: qsTr("Clean chat")
                }
            }

            QoASeparator {
                anchors.verticalCenter: parent.verticalCenter
            }

            QoAButton {
                id: newChatButtonId

                visible: root.isInEditor

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/new-chat-icon.svg"
                    color: palette.window.hslLightness > 0.5 ? "#000000" : "#FFFFFF"
                    height: 15
                    width: 15
                }

                QoAToolTip {
                    visible: newChatButtonId.hovered
                    delay: 250
                    text: qsTr("Open new chat in a new tab")
                }
            }

            QoAComboBox {
                id: configSelectorId

                implicitHeight: 25

                model: []
                currentIndex: 0

                QoAToolTip {
                    visible: configSelectorId.hovered
                    delay: 250
                    text: qsTr("Switch saved AI configuration")
                }
            }

            QoAComboBox {
                id: roleSelector

                implicitHeight: 25

                model: []
                currentIndex: 0

                QoAToolTip {
                    visible: roleSelector.hovered
                    delay: 250
                    text: qsTr("Switch agent role (different system prompts)")
                }
            }
        }

        Row {
            spacing: 10

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

                QoAToolTip {
                    visible: toolsButtonId.hovered
                    delay: 250
                    text: {
                        if (!toolsButtonId.enabled) {
                            return qsTr("Tools are disabled in General Settings")
                        }
                        return toolsButtonId.checked
                                ? qsTr("Tools enabled: AI can use tools to read files, search project, and build code")
                                : qsTr("Tools disabled: Simple conversation without tool access")
                    }
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

                QoAToolTip {
                    visible: thinkingModeId.hovered
                    delay: 250
                    text: thinkingModeId.enabled
                        ? (thinkingModeId.checked ? qsTr("Thinking Mode enabled (Check model list support it)")
                                                  : qsTr("Thinking Mode disabled"))
                        : qsTr("Thinking Mode is not available for this provider")
                }
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

                QoAToolTip {
                    visible: settingsButtonId.hovered
                    delay: 250
                    text: qsTr("Open Chat Assistant Settings")
                }
            }

            QoASeparator {
                anchors.verticalCenter: parent.verticalCenter
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

                    QoAToolTip {
                        visible: parent.containsMouse && recentPathId.text.length > 0
                        text: recentPathId.text
                        delay: 500
                    }
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

                QoAToolTip {
                    visible: saveButtonId.hovered
                    delay: 250
                    text: qsTr("Save chat to *.json file")
                }
            }

            QoAButton {
                id: loadButtonId

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/load-chat-dark.svg"
                    height: 15
                    width: 8
                }

                QoAToolTip {
                    visible: loadButtonId.hovered
                    delay: 250
                    text: qsTr("Load chat from *.json file")
                }
            }

            QoAButton {
                id: openChatHistoryId

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/file-in-system.svg"
                    height: 15
                    width: 15
                }

                QoAToolTip {
                    visible: openChatHistoryId.hovered
                    delay: 250
                    text: qsTr("Show in system")
                }
            }

            QoASeparator {}

            QoAButton {
                id: contextButtonId

                icon {
                    source: "qrc:/qt/qml/ChatView/icons/context-icon.svg"
                    color: palette.window.hslLightness > 0.5 ? "#000000" : "#FFFFFF"
                    height: 15
                    width: 15
                }

                QoAToolTip {
                    visible: contextButtonId.hovered
                    delay: 250
                    text: qsTr("View chat context (system prompt, role, rules)")
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
