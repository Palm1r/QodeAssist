// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
