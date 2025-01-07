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
import ChatView
import QtQuick.Layouts
import "./dialog"

Rectangle {
    id: root

    property alias msgModel: msgCreator.model
    property alias messageAttachments: attachmentsModel.model
    property color fontColor
    property color codeBgColor
    property color selectionColor

    height: msgColumn.implicitHeight + 10
    radius: 8

    ColumnLayout {
        id: msgColumn

        width: parent.width
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5

        Repeater {
            id: msgCreator
            delegate: Loader {
                id: msgCreatorDelegate
                // Fix me:
                // why does `required property MessagePart modelData` not work?
                required property var modelData

                width: parent.width
                sourceComponent: {
                    // If `required property MessagePart modelData` is used
                    // and conversion to MessagePart fails, you're left
                    // with a nullptr. This tests that to prevent crashing.
                    if(!modelData) {
                        return undefined;
                    }

                    switch(modelData.type) {
                        case MessagePart.Text: return textComponent;
                        case MessagePart.Code: return codeBlockComponent;
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
                    color: root.codeBgColor

                    Text {
                        id: attachText

                        anchors.centerIn: parent
                        text: modelData
                        color: root.fontColor
                    }
                }
            }
        }
    }

    component TextComponent : TextBlock {
        required property var itemData
        height: implicitHeight + 10
        verticalAlignment: Text.AlignVCenter
        leftPadding: 10
        text: itemData.text
        color: root.fontColor
        selectionColor: root.selectionColor
    }


    component CodeBlockComponent : CodeBlock {
        required property var itemData
        anchors {
            left: parent.left
            leftMargin: 10
            right: parent.right
            rightMargin: 10
        }

        code: itemData.text
        language: itemData.language
        color: root.codeBgColor
        selectionColor: root.selectionColor
    }

}
