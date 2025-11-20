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
import QtQuick.Controls
import QtQuick.Layouts
import ChatView
import UIControls

Rectangle {
    id: root

    property alias sendButton: sendButtonId
    property alias syncOpenFiles: syncOpenFilesId
    property alias attachFiles: attachFilesId
    property alias attachImages: attachImagesId
    property alias linkFiles: linkFilesId

    color: palette.window.hslLightness > 0.5 ?
               Qt.darker(palette.window, 1.1) :
               Qt.lighter(palette.window, 1.1)

    RowLayout {
        id: bottomBar

        anchors {
            left: parent.left
            leftMargin: 5
            right: parent.right
            rightMargin: 5
            verticalCenter: parent.verticalCenter
        }

        spacing: 10

        QoAButton {
            id: sendButtonId

            icon {
                height: 15
                width: 15
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
        }

        QoAButton {
            id: attachFilesId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/attach-file-dark.svg"
                height: 15
                width: 8
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Attach file to message")
        }

        QoAButton {
            id: attachImagesId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/image-dark.svg"
                height: 15
                width: 15
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Attach image to message")
        }

        QoAButton {
            id: linkFilesId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/link-file-dark.svg"
                height: 15
                width: 8
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Link file to context")
        }

        CheckBox {
            id: syncOpenFilesId

            text: qsTr("Sync open files")

            ToolTip.visible: syncOpenFilesId.hovered
            ToolTip.text: qsTr("Automatically synchronize currently opened files with the model context")
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
