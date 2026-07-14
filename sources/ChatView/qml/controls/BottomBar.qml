// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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
    property bool isProcessing: false
    property alias sendButtonTooltip: sendButtonTooltipId

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

            QoAToolTip {
                visible: attachFilesId.hovered
                delay: 250
                text: qsTr("Attach file to message")
            }
        }

        QoAButton {
            id: attachImagesId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/image-dark.svg"
                height: 15
                width: 15
            }

            QoAToolTip {
                visible: attachImagesId.hovered
                delay: 250
                text: qsTr("Attach image to message")
            }
        }

        QoAButton {
            id: linkFilesId

            icon {
                source: "qrc:/qt/qml/ChatView/icons/link-file-dark.svg"
                height: 15
                width: 8
            }

            QoAToolTip {
                visible: linkFilesId.hovered
                delay: 250
                text: qsTr("Link file to context")
            }
        }

        CheckBox {
            id: syncOpenFilesId

            text: qsTr("Sync open files")

            QoAToolTip {
                visible: syncOpenFilesId.hovered
                text: qsTr("Automatically synchronize currently opened files with the model context")
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Row {
            id: compressingRow

            visible: root.isCompressing
            spacing: 6

            QoABusyIndicator {
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

                QoAToolTip {
                    visible: cancelCompressButtonId.hovered
                    delay: 250
                    text: qsTr("Cancel compression")
                }
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

            QoAToolTip {
                visible: compressButtonId.hovered
                delay: 250
                text: qsTr("Compress chat (create summarized copy using LLM)")
            }
        }

        QoAButton {
            id: sendButtonId

            leftPadding: root.isProcessing ? 22 : 4

            icon {
                height: 15
                width: 15
            }

            QoABusyIndicator {
                id: sendBusyIndicator

                anchors.left: parent.left
                anchors.leftMargin: 5
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14
                running: root.isProcessing
            }

            QoAToolTip {
                id: sendButtonTooltipId

                visible: sendButtonId.hovered
                delay: 250
            }
        }
    }
}
