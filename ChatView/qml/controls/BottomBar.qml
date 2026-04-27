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
    property alias compressButton: compressButtonId
    property alias cancelCompressButton: cancelCompressButtonId

    property bool isCompressing: false

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

        Row {
            id: compressingRow

            visible: root.isCompressing
            spacing: 6

            BusyIndicator {
                id: compressBusyIndicator

                anchors.verticalCenter: parent.verticalCenter
                running: root.isCompressing
                width: 16
                height: 16
            }

            Text {
                text: qsTr("Compressing...")
                anchors.verticalCenter: parent.verticalCenter
                color: palette.text
                font.pixelSize: 12
            }

            QoAButton {
                id: cancelCompressButtonId

                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Cancel")

                ToolTip.visible: hovered
                ToolTip.delay: 250
                ToolTip.text: qsTr("Cancel compression")
            }
        }

        QoAButton {
            id: compressButtonId

            visible: !root.isCompressing
            text: qsTr("Compress")

            icon {
                source: "qrc:/qt/qml/ChatView/icons/compress-icon.svg"
                height: 15
                width: 15
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Compress chat (create summarized copy using LLM)")
        }

        QoAButton {
            id: sendButtonId

            icon {
                height: 15
                width: 15
            }
            ToolTip.visible: hovered
            ToolTip.delay: 250
        }
    }
}
