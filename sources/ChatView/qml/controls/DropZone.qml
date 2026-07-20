// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import QtQuick.Controls

Item {
    id: root

    signal filesDropped(var urlStrings)

    property int filesCount: 0
    property bool isDragActive: false

    Item {
        id: dropOverlay

        anchors.fill: parent
        visible: false
        z: 999
        opacity: 0

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(palette.shadow.r, palette.shadow.g, palette.shadow.b, 0.6)
        }

        Rectangle {
            anchors {
                top: parent.top
                horizontalCenter: parent.horizontalCenter
                topMargin: 30
            }
            width: fileCountText.width + 40
            height: 50
            color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.9)
            radius: 25
            visible: root.filesCount > 0

            Text {
                id: fileCountText
                anchors.centerIn: parent
                text: qsTr("%n file(s) to drop", "", root.filesCount)
                font.pixelSize: 16
                font.bold: true
                color: palette.highlightedText
            }
        }

        Rectangle {
            anchors.fill: parent
            color: root.isDragActive
                   ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.3)
                   : Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.15)
            border.width: root.isDragActive ? 3 : 2
            border.color: root.isDragActive
                          ? palette.highlight
                          : Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.5)

            Column {
                anchors.centerIn: parent
                spacing: 15

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Attach")
                    font.pixelSize: 24
                    font.bold: true
                    color: root.isDragActive ? palette.highlightedText : palette.text
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Images & Text Files")
                    font.pixelSize: 14
                    color: root.isDragActive ? palette.highlightedText : palette.text
                    opacity: 0.8
                }
            }

            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on border.width { NumberAnimation { duration: 150 } }
            Behavior on border.color { ColorAnimation { duration: 150 } }
        }
    }

    DropArea {
        id: globalDropArea

        anchors.fill: parent

        onEntered: (drag) => {
                       if (drag.hasUrls) {
                           root.isDragActive = true
                           root.filesCount = drag.urls.length
                           dropOverlay.visible = true
                           dropOverlay.opacity = 1
                       }
                   }

        onExited: {
            root.isDragActive = false
            root.filesCount = 0
            dropOverlay.opacity = 0

            Qt.callLater(function() {
                if (!root.isDragActive) {
                    dropOverlay.visible = false
                }
            })
        }

        onDropped: (drop) => {
                       root.isDragActive = false
                       root.filesCount = 0
                       dropOverlay.opacity = 0

                       Qt.callLater(function() {
                           dropOverlay.visible = false
                       })

                       if (!drop.hasUrls || drop.urls.length === 0) {
                           return
                       }

                       var urlStrings = []
                       for (var i = 0; i < drop.urls.length; i++) {
                           var urlString = drop.urls[i].toString()
                           if (urlString.startsWith("file://") || urlString.indexOf("://") === -1) {
                               urlStrings.push(urlString)
                           }
                       }

                       if (urlStrings.length === 0) {
                           return
                       }

                       drop.accept(Qt.CopyAction)

                       root.filesDropped(urlStrings)
                   }
    }
}
