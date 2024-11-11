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

pragma ComponentBehavior: Bound

import QtQuick
import ChatView
import "./dialog"

Rectangle {
    id: root

    property alias msgModel: msgCreator.model
    property color fontColor
    property color codeBgColor
    property color selectionColor

    height: msgColumn.height
    radius: 8

    Column {
        id: msgColumn

        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
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
