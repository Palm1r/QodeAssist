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
import QtQuick.Controls
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
                property var itemData: modelData

                width: parent.width
                sourceComponent: {
                    switch(modelData.type) {
                    case MessagePart.Text: return textComponent;
                    case MessagePart.Code: return codeBlockComponent;
                    default: return textComponent;
                    }
                }
            }
        }
    }

    Component {
        id: textComponent

        TextBlock {
            height: implicitHeight + 10
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            text: itemData.text
            color: fontColor
            selectionColor: root.selectionColor
        }
    }


    Component {
        id: codeBlockComponent

        CodeBlock {
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
}
