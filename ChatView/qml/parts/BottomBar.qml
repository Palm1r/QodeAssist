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
import QtQuick.Layouts
import ChatView

Rectangle {
    id: root

    property alias sendButton: sendButtonId
    property alias stopButton: stopButtonId
    property alias sharingCurrentFile: sharingCurrentFileId
    property alias attachFiles: attachFilesId

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

            text: qsTr("Send")
        }

        QoAButton {
            id: stopButtonId

            text: qsTr("Stop")
        }

        CheckBox {
            id: sharingCurrentFileId

            text: qsTr("Share current file with models")
        }

        QoAButton {
            id: attachFilesId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/attach-file.svg"
                height: 15
                width: 8
            }
            text: qsTr("Attach files")
        }

        Item {
            Layout.fillWidth: true
        }
    }
}