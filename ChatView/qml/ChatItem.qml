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
import ChatView
import QtQuick.Layouts
import UIControls

import "./dialog"

Rectangle {
    id: root

    property alias msgModel: msgCreator.model
    property alias messageAttachments: attachmentsModel.model
    property string textFontFamily: Qt.application.font.family
    property string codeFontFamily: {
        switch (Qt.platform.os) {
        case "windows":
            return "Consolas";
        case "osx":
            return "Menlo";
        case "linux":
            return "DejaVu Sans Mono";
        default:
            return "monospace";
        }
    }
    property int textFontSize: Qt.application.font.pointSize
    property int codeFontSize: Qt.application.font.pointSize
    property int textFormat: 0

    property bool isUserMessage: false
    property int messageIndex: -1

    signal resetChatToMessage(int index)

    height: msgColumn.implicitHeight + 10
    radius: 8
    color: isUserMessage ? palette.alternateBase
                         : palette.base

    HoverHandler {
        id: mouse
    }

    ColumnLayout {
        id: msgColumn

        x: 5
        width: parent.width - x
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5

        Repeater {
            id: msgCreator
            delegate: Loader {
                id: msgCreatorDelegate
                // Fix me:
                // why does `required property MessagePart modelData` not work?
                required property var modelData

                Layout.preferredWidth: root.width
                sourceComponent: {
                    // If `required property MessagePart modelData` is used
                    // and conversion to MessagePart fails, you're left
                    // with a nullptr. This tests that to prevent crashing.
                    if(!modelData) {
                        return undefined;
                    }

                    switch(modelData.type) {
                        case MessagePartType.Text: return textComponent;
                        case MessagePartType.Code: return codeBlockComponent;
                        default: return textComponent;
                    }
                }

                Component {
                    id: textComponent
                    TextComponent {
                        itemData: msgCreatorDelegate.modelData
                    }
                }

                Component {
                    id: codeBlockComponent
                    CodeBlockComponent {
                        itemData: msgCreatorDelegate.modelData
                    }
                }
            }
        }

        Flow {
            id: attachmentsFlow

            Layout.fillWidth: true
            visible: attachmentsModel.model && attachmentsModel.model.length > 0
            leftPadding: 10
            rightPadding: 10
            spacing: 5

            Repeater {
                id: attachmentsModel

                delegate: Rectangle {
                    required property int index
                    required property var modelData

                    height: attachText.implicitHeight + 8
                    width: attachText.implicitWidth + 16
                    radius: 4
                    color: palette.button
                    border.width: 1
                    border.color: palette.mid

                    Text {
                        id: attachText

                        anchors.centerIn: parent
                        text: modelData
                        color: palette.text
                    }
                }
            }
        }
    }

    Rectangle {
        id: userMessageMarker

        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: root.height - root.radius
        color: "#92BD6C"
        radius: root.radius
        visible: root.isUserMessage
    }

    QoAButton {
        id: stopButtonId

        anchors {
            right: parent.right
            top: parent.top
        }

        text: qsTr("ResetTo")
        visible: root.isUserMessage && mouse.hovered
        onClicked: function() {
            root.resetChatToMessage(root.messageIndex)
        }
    }

    component TextComponent : TextBlock {
        required property var itemData
        height: implicitHeight + 10
        verticalAlignment: Text.AlignVCenter
        leftPadding: 10
        text: textFormat == Text.MarkdownText ? utils.getSafeMarkdownText(itemData.text)
                                              : itemData.text
        font.family: root.textFontFamily
        font.pointSize: root.textFontSize
        textFormat: {
            if (root.textFormat == 0) {
                return Text.MarkdownText
            } else if (root.textFormat == 1) {
                return Text.RichText
            } else {
                return Text.PlainText
            }
        }

        ChatUtils {
            id: utils
        }
    }

    component CodeBlockComponent : CodeBlock {
        id: codeblock

        required property var itemData
        anchors {
            left: parent.left
            leftMargin: 10
            right: parent.right
            rightMargin: 10
        }

        code: itemData.text
        language: itemData.language
        codeFontFamily: root.codeFontFamily
        codeFontSize: root.codeFontSize
    }
}
